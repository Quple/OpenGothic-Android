# Handoff: przebieg wszystkich dialogów Kronik Myrtany na PC (Kiro CLI)

Dla agenta na PC użytkownika. Użytkownik skopiował cały folder repo z laptopa na PC i otworzył go w VS Code.
Twoje zadanie: **sprawdzić i przygotować środowisko, zbudować grę desktop, zrobić krótki test, a potem uruchomić
pełny przebieg dialogów i przekazać wyniki.** Tło: `docs/IKARUS_AUTOMATION.md` (przeczytaj sekcje „Użycie” i
„Postęp 25.11 wieczorem”).

Pisz do użytkownika po polsku, krótko i konkretnie.

## Zasady

- **Gałąź `ikarus-lego-automation`.** Nie przełączaj gałęzi i nie ruszaj `touch-ui`. To jest stan po patchu 051 i ma taki zostać.
- **Bez `git push`, bez commitów, bez zmian w `.gitignore` i w submodułach** (`lib/ZenKit`, `lib/Tempest` mają zmiany
  nałożone z patchy i tak ma zostać). To zadanie nie wymaga zmian w kodzie. Gdy coś wygląda na błąd w kodzie, opisz
  go użytkownikowi zamiast poprawiać.
- **Instalacja programów tylko za zgodą użytkownika.** Najpierw sprawdź wszystko (krok 1) i podaj jedną listę braków
  z linkami albo komendami `winget`. Instaluj dopiero, gdy użytkownik się zgodzi, albo pozwól mu zrobić to samemu.
- Wolno uruchamiać grę, ale tylko przez `tools/ikarus/modcheck.py`. Gra działa w `build-ikarus/<out>/run`, a folderu
  gry w Steamie nie zmienia. Jedna gra naraz. Zabijaj tylko proces gry, który sam uruchomiłeś.
- Wolno usuwać tylko katalogi buildów `build-desktop` i `build-notouch` (patrz krok 2). Nie usuwaj `build-ikarus`,
  bo tam są baza silnika i postęp dialogów z laptopa.

## Krok 1: sprawdź środowisko (nic nie instaluj)

Wykonaj i zbierz wyniki:

```powershell
git status -sb                                   # oczekiwane: ## ikarus-lego-automation...origin/ikarus-lego-automation
python --version                                 # Python 3.10+ (na laptopie 3.12)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& $vswhere -latest -products * -property catalog_productLineVersion      # oczekiwane: 2022
$env:VULKAN_SDK; Get-ChildItem C:\VulkanSDK -ErrorAction SilentlyContinue   # glslangValidator.exe w <SDK>\Bin
vulkaninfo --summary 2>$null | Select-String "deviceName|apiVersion"        # karta z Vulkanem
Test-Path build-ikarus\enginedb\G2A.json                                    # baza silnika skopiowana z laptopa
Test-Path build-ikarus\com-dialogs\run\dialogtest_progress.txt              # postęp dialogów z laptopa
```

Folder Kronik: domyślnie `C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana`. Jeśli go nie ma,
poszukaj w bibliotekach Steama (`C:\Program Files (x86)\Steam\steamapps\libraryfolders.vdf`, klucze `"path"`, w każdej
`steamapps\common\TheChroniclesOfMyrtana`). W folderze muszą być `System\Gothic2.exe` i
`System\TheChroniclesOfMyrtana.ini`.

Co zrobić, gdy czegoś brakuje (podaj użytkownikowi):

| Brak | Co doinstalować |
|---|---|
| vswhere nie znajduje VS albo brak `VC.Tools.x86.x64` | Visual Studio 2022 Community (https://visualstudio.microsoft.com/pl/vs/community/) albo Build Tools 2022, pakiet **„Programowanie aplikacji klasycznych w języku C++”** (zawiera MSVC, Windows SDK, „Narzędzia CMake języka C++ dla systemu Windows” z CMake i Ninja). `winget install Microsoft.VisualStudio.2022.Community` i potem dodanie pakietu w Visual Studio Installer. |
| brak Vulkan SDK / `glslangValidator.exe` | https://vulkan.lunarg.com/sdk/home#windows (albo `winget install KhronosGroup.VulkanSDK`). Po instalacji **zamknąć i otworzyć VS Code**, żeby terminal zobaczył `VULKAN_SDK` i `PATH`. |
| brak Pythona | https://www.python.org/downloads/ (zaznaczyć „Add python.exe to PATH”) albo `winget install Python.Python.3.12`. |
| `vulkaninfo` nie pokazuje karty | aktualny sterownik karty graficznej (NVIDIA/AMD/Intel). |
| brak Kronik | instalacja Kronik Myrtany w Steamie albo podanie przez użytkownika ścieżki folderu. |
| brak `build-ikarus\enginedb\G2A.json` | `python tools/ikarus/enginedb.py build --fetch` (potrzebny `git` i internet, ok. 15 s). |
| brak `build-ikarus\pylib\capstone` (opcjonalne) | `python -m pip install --target build-ikarus/pylib capstone` |

Python jest potrzebny tylko z biblioteką standardową, `capstone` jest opcjonalny.

## Krok 2: build gry desktop

Skopiowane `build-desktop` i `build-notouch` mają w `CMakeCache.txt` ścieżki z laptopa (Ninja z Android SDK
`C:/Users/User/AppData/Local/Android/Sdk/...`, Vulkan SDK `C:/VulkanSDK/1.4.363.0`). Na PC ich nie używaj, tylko usuń
i skonfiguruj od nowa:

```powershell
Remove-Item -Recurse -Force build-desktop, build-notouch -ErrorAction SilentlyContinue
$vs  = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
# plik .cmd zamiast "cmd /c ...": ścieżka Build Tools ma nawiasy "(x86)", a cmd /c gubi wtedy cudzysłowy
@"
@echo off
call "$vs\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake -S . -B build-desktop -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOTHIC_TOUCH_UI=ON > build-ikarus\build-desktop-config.log 2>&1 || exit /b 2
cmake --build build-desktop --target Gothic2Notr > build-ikarus\build-desktop.log 2>&1
"@ | Set-Content -Encoding ascii build-ikarus\build-pc.cmd
cmd /c build-ikarus\build-pc.cmd; "exit $LASTEXITCODE"
Get-Content build-ikarus\build-desktop.log -Tail 5 -ErrorAction SilentlyContinue
```

- Komunikat `'vswhere.exe' is not recognized` z `vcvars64.bat` jest nieszkodliwy (tak samo na laptopie).
- Kod wyjścia 1: nie ma `vcvars64.bat`, czyli brakuje Visual Studio/C++. Kod 2: konfiguracja CMake się nie udała,
  wtedy przeczytaj `build-ikarus\build-desktop-config.log`. Inny kod: błąd kompilacji, przeczytaj `build-ikarus\build-desktop.log`.
- Pełny build trwa ok. 15–20 min. Daj komendzie długi timeout (np. 30 min). Jeśli narzędzie na to nie pozwala,
  uruchom `cmd /c build-ikarus\build-pc.cmd` jeszcze raz, bo Ninja kontynuuje od miejsca przerwania.
- Wynik: `build-desktop\opengothic\Gothic2Notr.exe` (tej ścieżki domyślnie używa `modcheck.py`).
- Typowe błędy: `glslangValidator required` oznacza brak Vulkan SDK albo terminal otwarty sprzed instalacji;
  `cmake`/`ninja` nie znaleziony oznacza brak składnika „Narzędzia CMake języka C++” w Visual Studio Installer;
  CMake przy konfiguracji nakłada `patches/zenkit-opengothic.patch` sam, a w tym folderze jest już nałożony (to OK).

## Krok 3: krótki test (ok. 1,5 min)

```powershell
$g = 'C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana'   # albo ścieżka z kroku 1
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini --fresh --run "wait 30; comcheck test ikarus; comcheck fix sort; wait 10" --timeout 400
```

Oczekiwane w wyniku (jak na laptopie 25.11): `Run: done in ~70-140 s`, `0 failed checks`, `38 gaps (38 hits)`.
Gdy test nie przechodzi albo gra się nie uruchamia, pokaż użytkownikowi `build-ikarus\TheChroniclesOfMyrtana\SUMMARY.md`
i ostatnie 40 linii `build-ikarus\TheChroniclesOfMyrtana\run\log.txt` i zatrzymaj się.

## Krok 4: pełny przebieg dialogów (kilka godzin)

Na laptopie zrobiono 669 z 6145 dialogów (`build-ikarus\com-dialogs\run\dialogtest_progress.txt`). `dlgall resume`
kontynuuje od tego miejsca. Tempo na laptopie: ok. 20 dialogów na minutę, jeden rdzeń CPU w 100%, cały przebieg ok.
5 h. Na PC powinno być szybciej.

Uruchom grę jako osobny proces, **ze zwykłym, widocznym oknem**:

```powershell
$g = 'C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana'
Start-Process -FilePath python -WorkingDirectory (Get-Location) `
  -ArgumentList 'tools/ikarus/modcheck.py','--game',"`"$g`"",'--ini','TheChroniclesOfMyrtana.ini','--run','"wait 30; dlgall resume; waitfor Done: 20000; wait 5"','--timeout','21000','--out','build-ikarus/com-dialogs' `
  -RedirectStandardOutput build-ikarus\com-dialogs.out -RedirectStandardError build-ikarus\com-dialogs.err -PassThru
```

Doświadczenia z laptopa:
- **Nie używaj `-WindowStyle Hidden` ani `Minimized`.** Okno gry dziedziczy ten tryb i przebieg prawie staje (ok. 1
  dialog na 10 s). Uruchamianie w tle przez narzędzie powłoki agenta (`run_in_background`) na laptopie nie
  uruchamiało procesu niezawodnie. `Start-Process` jak wyżej działało.
- Powiedz użytkownikowi: nie minimalizować okna gry (przykryte innym oknem jest OK), wyłączyć usypianie PC na czas
  przebiegu, nie grać w tym czasie w nic innego.

Monitorowanie co kilka minut (pliku `log.txt` nie da się czytać w trakcie, bo gra trzyma go na wyłączność):

```powershell
$p = Get-Process Gothic2Notr -ErrorAction SilentlyContinue
"{0:HH:mm:ss} RAM={1:N0}MB dialogi={2} ostatni={3}" -f (Get-Date), ($p.WorkingSet64/1MB), `
  (Get-Content build-ikarus/com-dialogs/run/dialogtest_progress.txt | Measure-Object).Count, `
  (Get-Content build-ikarus/com-dialogs/run/dialogtest_last.txt)
```

Gdy liczba dialogów rośnie wolniej niż kilka na minutę przez ponad 10 min, sprawdź, czy okno gry nie jest
zminimalizowane. Gdy gra zniknie, a `modcheck.py` się skończy, przeczytaj `build-ikarus\com-dialogs.out`.

Po crashu (w `SUMMARY.md`: „the game ended without a report”):
1. zapisz kopię `build-ikarus\com-dialogs\run\crash.log`, `crash.dmp` i `SUMMARY.md` (np. do
   `build-ikarus\com-dialogs\crash-<nazwa dialogu>\`), bo następny przebieg je kasuje;
2. uruchom ponownie to samo `Start-Process` (`dlgall resume` pomija dialog, który wywołał crash);
3. po trzech crashach z rzędu na różnych dialogach zatrzymaj się i zapytaj użytkownika.

## Krok 5: raport dla użytkownika

Napisz krótko:
- co było zainstalowane, czego brakowało i co doinstalowano;
- czas buildu i wynik krótkiego testu;
- przebieg dialogów: ile z 6145, ile czasu, tempo (dialogi na minutę), crashe (dialog, pierwsza linia `crash.log`);
- 15 pierwszych zadań z `build-ikarus\com-dialogs\worklist\INDEX.md`.

Wyniki do przeniesienia na laptopa (folder `build-ikarus` jest w `.gitignore`, więc nie idzie przez git): cały
`build-ikarus\com-dialogs\` (bez `GOTHIC.DAT`, jeśli trzeba oszczędzać miejsce) i kopie z crashy. Użytkownik skopiuje je
ręcznie do tego samego folderu na laptopie.
