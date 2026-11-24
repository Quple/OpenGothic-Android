# Mods from Sefaris (Android)

The launcher has one tile, **Spine/Sefaris**, that asks which of the two databases to open (the header of the launcher is narrow, so there
is one tile and not two). Sefaris lists the Gothic and Gothic II mods of [sefaris.eu](https://sefaris.eu)
(Polish mods and translations): the list, the page of a mod, the install and the play. A mod is played and kept like a mod of
Spine (`docs/SPINE.md`): `SpineStore`, `SpineActivation`, the saves screen. Sefaris mods are stored with the id
`SefarisApi.ID_BASE + id`, so they do not meet the ids of Spine, and they have their own screen (`SefarisActivity`).

## What Sefaris gives, and what it does not

The public API (`https://api.sefaris.eu/swagger`) has the list (`GET /Public/Mod`) and the page of a mod
(`GET /Public/Mod/{externalId}`: the description, the guide and `details.downloadLinks`). It has **no files**: the links lead to
Google Drive (files and folders), MEGA (files and folders), MediaFire, ModDB, Steam. Of the 119 Gothic and Gothic II mods 46 have a link
to a file on Google Drive, 66 to a file on MEGA, 14 to a folder on MEGA, 19 to a folder on Google Drive, 8 to ModDB, 3 to MediaFire (counts of
the day this was written; a mod can have more links). There are no hashes and no list of files. Most of the archives are RAR (RAR4 and RAR5)
or Windows installers (.exe), a few are 7z or ZIP. ZIP is opened by Java itself; RAR and 7z by libarchive (see below).

## How a mod is installed

The button *Download* loads the page of the mod and asks for the way:

1. **Download (Google Drive)**, when the mod has a link to a file there. The app reads the file from
   `drive.usercontent.google.com/download?id=...&confirm=t`. The first bytes tell the kind of the file. A ZIP is unpacked on the way, so
   only the mod takes space. RAR, 7z and the like are saved to a file first (they are read with seeking), unpacked, and the file is deleted;
   it takes about twice the size of the archive for a while. An installer (.exe) stops at once with a message.
2. **Download (MEGA)**: a public link of MEGA. The address of the data comes from the API of MEGA, the data is decrypted with the key from
   the link (AES-128-CTR, `SefarisMega`). For a folder the biggest archive in the folder itself is taken (the older versions lie in its
   subfolders). MEGA limits the transfer of one address; when it does, the app says so
   and the file can be tried again some hours later.
3. **Open the page: <host>** for the other links (ModDB, MediaFire, a folder of Google Drive): the page opens in a browser of the app
   (`SefarisWebActivity`, a WebView). The player presses the download button of the page; when the page starts a download the browser
   stops it and gives the address, the cookies and the agent back, and the app downloads the file itself. The page needs a real browser:
   ModDB answers a program with a check of Cloudflare.
   When the app cannot connect to the server of the file (the message names the reason: a timeout, a refusal), the file is taken by the
   system download manager with the same cookies, and the app unpacks it when it is there (`SefarisService.installBySystem`).
4. **I have the file (ZIP, RAR, 7z)**: the file picker; ZIP is read as a stream, the others from the file itself.
5. **I have the unpacked folder**: the folder picker; for what the app cannot open (an installer .exe that was run on a computer, a
   multi-volume or password-protected archive).

After the unpacking (`SefarisInstaller`): the folder of the mod (the nearest one with `System` and an ini in it) takes the place
of the whole tree, so the files go to the paths of the folder of Gothic (`Data/`, `System/`, `_work/`), `.dll` and `.exe` are left out (the plugins of Union run only on Windows; the screen says how
many were skipped), the scripts are searched for Ikarus and LeGo as for Spine (`SpineScan`; a mod that needs them is removed), and the
ini file of `System/` that is not the one of the game is the one the mod starts with. A mod without such an ini is refused: it is a
patch for the game that needs Union.

## Filters

The bar of the list has the same chips as the one of Spine: the game (all, Gothic, Gothic II, downloaded), the language and the sort
(newest, name). Sefaris has **no field of languages**, so the language chip uses the tags of the mods: *Polish dubbing* is the tag
``Dubbing PL``, *Translated to Polish* is the tag ``Bractwo Spolszczenia`` (the team that translates mods). A mod without these tags is
shown under *any language* only.

## Reports of the mods (``mod-logs/``)

Every mod that is installed from Sefaris or Spine, or played, has a text file in ``mod-logs/`` of the files of the application
(``/sdcard/Android/data/org.opengothic.mobile/files/mod-logs/``). The button **Report** of the Sefaris screen shows all of them and shares
them as text; the screens of Spine have the same button. The lines are marked: ``RESULT`` (how the install ended), ``SKIPPED`` (a file that was left out and why), ``WARN`` and ``ERROR``
(what is wrong), ``GUIDE`` (what the guide of the mod says it needs: Union, plugins, Ikarus), ``INFO``, ``ENGINE`` (what the engine said
while the mod ran) and ``TODO``, which says what has to be done for the mod to work fully. A mod is thus checked at three moments:

1. **Install** (``SefarisReport.install`` and ``installSpine``; a check of the scripts of a Spine mod is reported too): the source, the
   result or the reason of the failure, the files that were left out (plugins ``.dll``, installers ``.exe``), the ini files, the files outside
   Data/System/_work, the guide of the mod. A mod that needs Ikarus or LeGo gets the file and the symbol that gave it away (for example
   ``Data/ModVDF/X.mod, symbol MEM_READINT``) and a ``TODO``; an archive that holds only an installer gets a ``TODO`` too.
2. **Start** (``SefarisReport.beforePlay``, called by ``SpineActions.play``): the ini is in the mod, every archive that the ini lists is in
   the mod or in Data of the game, the archives of the mod with their sizes, which files of the game the mod replaces, what the install
   left out. The player sees the short warnings.
3. **End of the run** (``SefarisReport.afterRun``, called when the launcher or the screens of Spine and Sefaris come back, and before the next
   mod starts): the lines of ``log.txt`` of the engine that name a problem (error, failed, invalid, unable, missing, unbound...), once each
   with the number of times (``(x37)``), the head and the end of the log, how long the run was, the last line of ``lifecycle.txt``, and
   ``crash.log`` when the game crashed. No ``log.txt`` newer than the start means that the game did not start. The log of the engine is of
   the last run only, so the lines are taken at once.
   For every ``unable to load X`` the catalogs of the archives of the mod and of the game are read (``SefarisAssets``, the VDF format, about
   150 ms for the 26 archives of Gothic II) and the line says one of two things: ``NOT in any archive of the mod or of the game`` (a part of
   the mod is missing, or the world asks for a wrong name) or ``is in [archive], the engine failed to load it`` (a bug of a loader). The
   names are compared as the compiled files are named: ``X.MDS`` is ``X.MDL``, ``X.TGA`` is ``X-C.TEX``, ``X`` is ``X_01.WAV``.
A report grows to 200 KB, then it loses its oldest part.
## Limits

* RAR and 7z need libarchive (`SefarisArchive`, `me.zhanghai.android.libarchive:library` in `CMakeLists.txt`). The script
  `build-apk.sh` has no libraries and leaves `SefarisArchive` out, so that build opens ZIP only and tells the player so.
* A password-protected or multi-volume archive and an installer (.exe) are refused with a message; use the folder for them.
* Mods that need the plugins of Union (`.dll`) may miss features or not run.
* Google Drive may answer with a page instead of the file (a limit of downloads, a private file); it is reported and the other ways stay.
* About a third of the mods of Sefaris are installers (`.exe`, Inno Setup) that make the folder of the mod when they are run. Windows runs
  them, the phone does not: use *I have the unpacked folder* after running them on a computer.
* There is no resume of an interrupted download and no check of the version after the update; install again to replace a mod.
* The files belong to their authors: the app only opens the links that Sefaris publishes.
