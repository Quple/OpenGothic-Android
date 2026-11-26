# Ikarus/LeGo bez obsługi: konwerter silnika, rejestr luk, przebieg gry bez człowieka

Stan: **26.11.2026, PC: narzędzie dialogów szybkie i powtarzalne, kolej na zadania z worklisty** (sekcja „Postęp 26.11 na PC”
niżej). Wszystko poniżej jest zaimplementowane,
zbudowane (desktop, MSVC, z `-DOPENGOTHIC_TOUCH_UI=ON` i bez) i sprawdzone na Kronikach Myrtany, chyba że napisano inaczej.
Pierwsza część zmian jest też w jednym pliku: `patches/025-ikarus-automation.patch` (zastosowanie: niżej).

## Po co

Do tej pory każda luka Ikarusa/LeGo wymagała ręcznej pracy: ktoś grał na telefonie, czytał log, szukał adresu w
źródłach Ikarusa, zgadywał sygnaturę funkcji silnika, pisał handler i znowu grał. Teraz pętla jest zamknięta i może ją
prowadzić agent AI bez człowieka:

```
modcheck.py  ->  DAT moda  ->  audyt statyczny  ->  gra bez człowieka (-autorun)  ->  rejestr luk  ->  worklist (zadania)
     ^                                                                                                     |
     +---------------------------- agent implementuje zadanie, buduje, uruchamia ponownie <----------------+
```

## Elementy

| Element | Plik | Co robi |
|---|---|---|
| Konwerter silnika | `tools/ikarus/enginedb.py` | Z nagłówków Gothic API (Union, https://github.com/gothicuef/gothic-api) buduje bazy JSON dla G1, G1A, G2, G2A: ok. 11 tys. funkcji z pełną sygnaturą C++ i konwencją wywołania, 650 klas z polami i offsetami (z klasami bazowymi i polami bitowymi), zmienne globalne, statyczne składowe, vtable. Komendy: `build --fetch`, `lookup`, `find`, `class`, `field`, `stub` (szkielet handlera C++), `symbolize` (nazwy adresów w dowolnym logu/CSV). |
| Rejestr luk w grze | `common/game/compatibility/gaplog.h/.cpp` | Każda luka trafia do rejestru z licznikiem i stosem wywołań skryptu: wywołanie silnika bez handlera (Cpu32, z ECX i argumentami), hook bez mostu, odczyt/zapis nieistniejącej pamięci, pole `oCNpc`/`oCItem` czytane przez uchwyt a niewypełniane (`observeHandleAccess`), external bez bindingu, wyjątek VM. Pierwsze wystąpienie to linia `IKGAP {json}` w `log.txt` (więc jest też w raportach z telefonów), całość z licznikami w `ikarus-gaps.json` (zapis przy każdym zapisie gry, na końcu sesji, na żądanie). JS: `debug.gaps(flush)`. |
| Stos skryptu | `patches/zenkit-opengothic.patch` | ZenKit dostał `DaedalusVm::call_stack()` (sam akcesor, bez zmiany działania). |
| Obserwator pamięci | `mem32.h/.cpp` | `Mem32::setObserver(Type, fn)`: odczyty i zapisy skryptów w regionach typowanych (readInt/writeInt/bajty/copyBytes). |
| Gra bez człowieka | `common/utils/autorun.h/.cpp`, `commandline.*`, `mainwindow.cpp` | `-autorun "<komendy>"` i `-autoexit <s>`. Komendy po `;`: `wait <s>`, `waitfor <tekst> [s]`, `gaps`, `quit`, reszta do konsoli modów (`comcheck ...`, `dlgall`, `eval <mod> <kod>`). Po ostatniej komendzie gra sama się zamyka i pisze `autorun-report.json` (wynik, konsola modów, błędy logu, luki). |
| Wszystkie dialogi z konsoli | `mods/devtools/index.js` | `dlgall [słowa]`, `dlgall resume` (po crashu, pomija dialog, który go wywołał), `dlgall stop` — to samo co przycisk „Test all”, kilka dialogów na klatkę, stały krok gry 1000/60 ms (wyniki nie zależą od szybkości komputera). |
| Lista zadań | `tools/ikarus/worklist.py` | Łączy audyt, DAT, bazę silnika, rekordy z gry (log.txt z telefonu, `ikarus-gaps.json`, `autorun-report.json`, crash) i oryginalny exe. Jedno zadanie = jeden plik `.md`: sygnatura, klasa i jej układ, gdzie są handlery tej samej klasy, bajtkod wywołujących funkcji z nazwami adresów, kod x86 w miejscu hooka wewnątrz funkcji (capstone), rekordy z gry, szkielet C++, kryterium „gotowe”, zasady warstwy. Kolejność: crash, nieudane testy, trafienia w grze, zasięg w skryptach. |
| Jedna komenda | `tools/ikarus/modcheck.py` | Instalacja gry → GOTHIC.DAT (najnowszy wolumen, jak w VDF Gothica) → baza silnika → audyt → (opcjonalnie) przebieg gry → worklist → `SUMMARY.md`. Crash bez raportu jest zadaniem z ogonem logu i nazwą dialogu (`dialogtest_last.txt`). Folderu gry nie zmienia. |
| `disasm.py` | `tools/ikarus/disasm.py` | `Dat.find`, `Dat.disasm_lines` (tekst z nazwami celów `bl` i wartościami stałych) dla worklist. |

## Użycie

Raz na maszynę:

```powershell
python tools/ikarus/enginedb.py build --fetch                       # klonuje gothic-api do build-ikarus/, ~15 s
pip install --target build-ikarus/pylib capstone                   # opcjonalnie: kod x86 hooków w zadaniach
# gra desktop (Vulkan; z touch UI albo bez, -DOPENGOTHIC_TOUCH_UI=OFF):
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cmake -S . -B build-desktop -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOTHIC_TOUCH_UI=ON && cmake --build build-desktop --target Gothic2Notr'
```

CMake 4.5.0-rc1 (osobna instalacja w `PATH`) generuje dla Ninja regułę do `CXXInterfaceObjects.rsp`, którego nie tworzy:
build kończy się `ninja: error: ... CXXInterfaceObjects.rsp ... missing`. CMake z Visual Studio (4.3.1-msvc1) działa: po
`vcvars64.bat` dopisz `set "PATH=<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"` (na PC tak robi
`build-ikarus\build-pc.cmd`). Visual Studio Build Tools 18 (2026) też działa, nie tylko 2022.

Mod:

```powershell
$g = 'C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana'
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini                        # tylko statycznie
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini --run "wait 30; comcheck test ikarus; comcheck fix all; wait 5" --timeout 300
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini --run "wait 30; dlgall; waitfor Done: 900; wait 5" --timeout 1000 --out build-ikarus/com-dialogs   # wszystkie 6145 dialogów, ok. 3 min (PC, Ryzen 7 5800X3D); po crashu: "wait 30; dlgall resume; waitfor Done: ..."
# log z telefonu:
python tools/ikarus/worklist.py --audit build-ikarus/TheChroniclesOfMyrtana/audit --dat build-ikarus/TheChroniclesOfMyrtana/GOTHIC.DAT --gaps <log.txt> --out build-ikarus/phone
```

Wynik: `build-ikarus/<mod>/SUMMARY.md`, `worklist/INDEX.md`, `worklist/tasks/NNN-*.md`, `run/autorun-report.json`,
`run/log.txt`. Gra działa w `build-ikarus/<mod>/run` (tam `Gothic.ini`, logi, kopia `mods/`). Po zmianie C++ dodaj
`--fresh`, żeby audyt czytał nowe rejestracje. `--save N` wczytuje zapis zamiast nowej gry.

Pętla agenta: weź pierwsze zadanie z `INDEX.md` → zaimplementuj według karty → zbuduj → ten sam `modcheck.py ... --run`
→ zadanie znika z listy, a test w przebiegu przechodzi → commit.

## Wyniki na Kronikach (25.11.2026)

- Konwerter: wszystkie 178 adresów z audytu CoM mają nazwę i sygnaturę (128 to początki funkcji, 50 hooków wewnątrz
  funkcji z offsetem, np. `0x437d9f` = `oCSavegameInfo::Archive+0x42f`).
- Przebieg nowej gry + `comcheck test ikarus` + `comcheck fix all`: 76–90 s, bez człowieka. Wyniki:
  - **FAIL** `QSORT_G2`: tablica z liczbą ujemną kończy się `-3` (komparator bez znaku), test oczekuje kolejności ze
    znakiem; trzeba rozstrzygnąć na podstawie oryginału, kto ma rację (test czy warstwa). Także `comcheck fix sort`:
    „the log still says TODO: QSORT_G2”.
  - **FAIL** „a pointer of a character is positive”: wskaźnik postaci = 0 w teście.
  - Luka co klatkę: odczyt nieistniejącego globalnego `zrenderer` (0x00982f08) przez
    `_SPRITE_DORENDER → ZRND_XD3D_SETRENDERSTATE → CALL_END` (ok. 50 razy na sekundę).
  - Błędy ZenKit „Accessing member without an instance” w `RESTORENPCNAMES` (C_NPC.AIVAR) i `INIT_ITEMGROUPS_MEAT` (C_ITEM.HP).
  - 38 hooków bez mostu (te same co w `ARCHOLOS_BLOCKERS_2026-10-26.md`), 0 nieznanych wywołań silnika, 0 pól uchwytów.
- `dlgall` na wszystkich 6145 dialogach: po ok. 18 min gra **zakończyła się bez raportu** (kod 0xCFFFFFFF, bez
  `crash.log`) na dialogu `PC_BOW_H_01`. Sam ten dialog osobno przechodzi — przyczyna zależy od stanu po wcześniejszych
  dialogach (pamięć? stan świata?). Do zbadania: `dlgall resume`, ewentualnie debugger.
- Przez 18 min dialogów żadne nieznane wywołanie silnika ani pole uchwytu się nie pojawiło: braki CoM to dziś głównie
  semantyka i hooki, nie brakujące adresy.

Zadania tych wyników: `build-ikarus/TheChroniclesOfMyrtana/worklist`, `build-ikarus/com-dialogs/worklist`,
`build-ikarus/com-crash/worklist` (lokalnie, `build-*/` jest w `.gitignore`).

## Postęp 25.11 wieczorem

Zrobione (przebieg `wait 30; comcheck test ikarus; comcheck fix sort; wait 10`: 3/3 testy ikarus PASS, `fix sort` PASS,
luki spadły z 39 (2951 trafień) do 38 hooków po 1 trafieniu, błędy logu z 84 do 76):

- **QSORT_G2**: błąd był w teście, nie w warstwie. Komparator silnika `zArraySortDefaultCompare` (0x553ca0:
  `cmp ecx,eax; jbe; sbb; neg`) porównuje słowa bez znaku, więc w Gothicu `-3` ląduje na końcu. Testy
  (`selftests.js`, `fixcases.js`) oczekują teraz kolejności bez znaku.
- **Wskaźnik postaci = 0**: błąd w teście. `MEM_InstToPtr(var int inst)` dostaje indeks symbolu (jak skrypt
  `MEM_InstToPtr(hero)`), a test podawał obiekt JS, który `debug.call` zamieniał po cichu na 0. Test podaje teraz
  `debug.symbol("HERO").index`. `debug.call` odrzuca obiekt dla parametru int/string z czytelnym błędem.
- **`logSerial` w `fixcases.js`** zwracał numer najstarszej linii bufora, więc „błędy od początku przypadku” obejmowały
  2000 najstarszych linii logu (stąd fałszywe „the log still says TODO: QSORT_G2”). `debug.log` zwraca teraz też
  `last` (numer najnowszej linii).
- **`zrenderer` (0x982f08)**: globalny wskaźnik jest odwzorowany (obiekt z vtable `zCRnd_D3D`). LeGo woła
  `SetRenderState`/`DrawPolySimple`/`SetAlphaBlendFunc` z `ECX=[zrenderer]`; handlery w `directmemory_ui.cpp` już
  istniały, ale odczyt wskaźnika kończył się luką ok. 50 razy na sekundę. Wizualnie sprite'ów nie sprawdzałem.
- Znane problemy 1 i 2 rozwiązane: `stopAll` wypisuje „Done: …” do konsoli (sprawdzone: `dlgall PC_BOW; waitfor Done: 120`
  kończy się po 67 s), a `rainfx.cpp` ma osłonę `#ifdef OPENGOTHIC_TOUCH_UI` (bez touch UI: deszcz 2, mokra ziemia 2,
  jak domyślne ustawienia touch UI). Pełny build bez touch UI w `build-notouch` przechodzi.
- Crash na Windows zostawia teraz dane: `crash.dmp` (minidump, `MiniDumpWriteDump`), kod i adres wyjątku w pierwszej
  linii `crash.log`, 80 ostatnich linii logu z pamięci (linie info w `log.txt` są buforowane i crash je gubił),
  rezerwa stosu na przepełnienie stosu, obsługa „pure virtual call” i „invalid parameter” CRT. `modcheck.py` kasuje
  stare pliki crasha przed przebiegiem, pokazuje kod wyjścia szesnastkowo i wskazuje `crash.dmp`.

Przebieg `dlgall` (30 min, limit `-autoexit 1800`): bez crasha, 669 z 6145 dialogów. **Narzędzie dialogów jest wolne:**
ok. 20 dialogów na minutę przy jednym rdzeniu CPU zajętym w 100%, czyli pełny przebieg to ok. 5 godzin. Wcześniejszy
crash na `PC_BOW_H_01` był więc dalej w kolejce, niż sądziłem. Przebieg można dzielić: `dlgall resume` czyta
`dialogtest_progress.txt` z `build-ikarus/<out>/run` (modcheck go nie kasuje). Okno gry nie może być ukryte
(`Start-Process -WindowStyle Hidden` ukrywa też okno gry i przebieg prawie staje).

Nowe zadania z tego przebiegu (`build-ikarus/com-dialogs/worklist`): odczyt `0x0067b968` (`oCNpc::OnDamage+0x108`),
`write to symbol PRINT.PARN table is not implemented`, `mem_free: address is not an owned allocation`,
`mem_readint: address translation failure`, `_takeref - not a memory-instance: AI_TELEPORTALWAYS.NPC`, ostrzeżenia
`MEM_WARN`/`MEM_PRINTSTACKTRACE` oraz kilkanaście „Accessing member … without an instance set”.

„Accessing member … without an instance set” (`RESTORENPCNAMES`, `INIT_ITEMGROUPS_MEAT` i inne) zostawiłem. Skrypty
czytają tam pola instancji, które w nowej grze jeszcze nie istnieją (np. `BDT_6289_VILLAN` wstawia dopiero
`KQ405_HUNTERSSPAWN`, `ITGR_MEAT` tworzy dopiero `Itm_GetPtr`). OpenGothic działa z `ALLOW_NULL_INSTANCE_ACCESS`
(odczyt 0, zapis pominięty). Nie sprawdziłem w kodzie x86, czy oryginalny `zCParser` robi to samo; do potwierdzenia,
zanim te zadania zostaną zamknięte jako „jak w oryginale”.

Następne kroki: wolność `dlgall` (profilowanie: co zajmuje ok. 3 s CPU na dialog), pełny przebieg dialogów
(na szybszym PC albo w kilku częściach przez `dlgall resume`), potem zadania z `com-dialogs/worklist` od góry.
(Zrobione 26.11 na PC, sekcja niżej.)

## Postęp 26.11 na PC

Pełny przebieg `dlgall` (6145 dialogów) trwa ok. 3 min zamiast ok. 5,5 h i daje te same wyniki przy każdym uruchomieniu,
także na zajętym komputerze. Commity `130deac3`, `e964cbd6`, `4a1ff35f`.

- **Wolność nie była w dialogach.** Pomiar czasu klatki: sam `dlgRun` 5–40 ms, reszta to funkcje klatkowe LeGo
  (`_FF_Hook`: `_ANIM8_LOOP`, `GCPRINTS_ALPHA`, napisy ekranowe modu), z 130 ms do 11 s na klatkę po 600 dialogach.
  Profil próbkujący licznik instrukcji VM: 84% w `MEM_ArrayRemoveValue`/`MEM_ReadIntArray`, potem `_HT_Get`/`MEM_ArrayRead`.
  Przyczyna: `Mem32::implTranslate` szukał bloku liniowo (było „TODO: binary-search”), więc każdy `MEM_ReadInt` kosztował
  tyle, ile bloków pamięci (uchwyty PermMem, napisy, Anim8), a bloków przybywało z każdym dialogiem.
  - `Mem32`: blok szukany binarnie (`regionAt`, regiony są posortowane po adresie), także w `free` i `realloc`;
    po zwolnieniu łączone są tylko sąsiednie wolne bloki.
  - Tablice Ikarusa natywnie (`MEM_ArrayRead/Write`, `MEM_ArrayIndexOf`, `MEM_ArrayRemoveValue(Once)`,
    `MEM_Read/WriteIntArray`), z semantyką skryptów: ostatni element w lukę, `MEMINT_ArrayRemoveValue_OnlyOnce`,
    `MEM_Error` przy złym indeksie. Test w `comcheck test ikarus` („an array: index of a value, remove a value…”)
    przechodzi tak samo na nadpisaniach i na oryginalnym kodzie Ikarusa.
- **`dlgall` gubił dialogi przy zmianie świata.** Gdy dialog przenosił gracza do innego świata, `debug.dlgRun` przez czas
  ładowania zwracał `null`, a kolejka szła dalej: ok. 340 dialogów (wszystkie `MIL_*`) nie miało wyniku. Teraz dialog czeka
  na następną klatkę; po 5 min bez gry przebieg kończy się komunikatem „There is no game for 5 minutes”.
- **Wyniki zależały od obciążenia komputera.** Dłuższa klatka to większy krok gry (do 50 ms zamiast 16), więc na dialog
  przypadało więcej czasu świata: włączało się `Q602_START` (zmiana świata na `ARCHOLOS_ENDGAME`), mnożyły się napisy LeGo,
  a przebieg trwał 7–26 min z innymi wynikami. Podczas `dlgall` (`DialogTest::guardExit`) krok gry wynosi 1000/60 ms
  (`MainWindow::tick`). Do tego limit 400 ms na wywołanie skryptu liczył też pracę gry w `debug.dlg*`: przy obciążeniu
  `dlgall` przerywał się przy budowie listy (`InternalError: interrupted`), a przerwanie po `dlgRun` powtarzało dialog.
  Funkcje `dlg*` odnawiają limit po swojej pracy (`ModHost::renewDeadline`), kolejka przesuwa się przed wywołaniem,
  lista dialogów to jedno wywołanie `dlgList`. Sprawdzone: bez obciążenia 2,8 min, przy 16 zajętych wątkach 4,4 min,
  wyniki identyczne co do linii.
- Wynik przebiegu: 5786 ok, 165 issues, 131 cond-false, 42 cond-unsolved, 21 skipped; świat do końca
  `ARCHOLOS_MAINLAND.ZEN`; worklist 87 zadań (call 31, hook 38, log 10, script 1, unmapped 7).
- Nocny przebieg na PC (stary kod, 5,5 h, `dlgall` + `dlgall resume`) jest w `build-ikarus/com-dialogs` i
  `com-dialogs-part1`; jego worklist pochodzi w połowie ze świata `ARCHOLOS_ENDGAME` i nie nadaje się już na listę zadań.

Zadanie `memory 0x00000194` (pierwsze na liście z nocnego przebiegu) to nie luka warstwy. `Q514_CHANGEDOOR_APPLY`
(funkcja klatkowa z `DIA_KESSEL_Q514_GARGOYLE`, opóźnienie w czasie gry) szuka `Q514_MOVER_GARGOYLEDOOR_01/02`, a te
drzwi są tylko w `ARCHOLOS_VOLFZACKE.ZEN` (`KM_Worlds.mod`). Narzędzie uruchomiło dialog w innym świecie,
`MEM_SearchVobByName` zwróciło 0, a skrypt co klatkę czytał `moverState` przez pusty wskaźnik (0 + 0x194). W
deterministycznym przebiegu ta funkcja nie zdąża się włączyć i luki nie ma. Handlera nie dodałem (zasady warstwy).

Testy JS (`node --test tests/js`): `devtools` 22/22; w `comtest` 1, a w `fixcases` 2 testy nie przechodzą tak samo na
HEAD sprzed tych zmian (do sprawdzenia osobno).

Następne kroki: zadania z worklisty deterministycznego przebiegu od góry (`memory 0x00000070`, `0x38`, `0x0c`, `0x00`,
`0x20`, `0x1c`: odczyty przez pusty wskaźnik; każde rozpoznać, czy to luka warstwy, czy dialog spoza swojego świata,
patrz problem 8 niżej).

## Znane problemy i następne kroki

1. ~~`waitfor Done:` po `dlgall` nie działa~~ — rozwiązane 25.11 wieczorem: `waitfor Done: <s>` działa.
2. ~~Desktop bez touch UI się nie kompiluje~~ — rozwiązane 25.11 wieczorem (osłona w `rainfx.cpp`).
3. Crash bez raportu: od 25.11 wieczorem `crash.dmp` i ogon logu w `crash.log` (patrz wyżej). Gdy proces zginie bez
   wywołania filtra wyjątków (np. zabity przez sterownik), dalej zostaje tylko `dialogtest_last.txt`.
4. `observeHandleAccess` obejmuje tylko uchwyty `oCNpc`/`oCItem`; movery (`zCMover`) i moby (`oCMobLockable`) mają
   wypełniane pola i można je objąć tym samym sposobem.
5. Gry G1: `enginedb` ma bazę G1, ale audyt rozwiązuje `MEMINT_SWITCHG1G2` zawsze na G2; DAT z G1 trzeba sprawdzić.
6. Inne mody: Spine/Sefaris na Androidzie **usuwają** mody z Ikarusem/LeGo przy instalacji (`SpineScan`). Na desktopie
   wystarczy rozpakowany mod i `modcheck.py --game <folder> --ini <ini>`; skompresowane wolumeny: `--dat` z ZenKit.
7. Zasady warstwy (bez pustych handlerów, bez oznaczania adresu jako obsłużonego bez skutku) są w każdej karcie zadania.
8. `dlgall` uruchamia wszystkie dialogi w jednym świecie (postać spoza świata stawia obok gracza). Dialog z innego
   świata może zostawić luki, których w grze nie ma: skrypt szuka voba po nazwie (`Ikarus: MEM_SearchVobByName: there is
   no vob X` w logu), dostaje 0 i czyta pola przez pusty wskaźnik (np. `memory 0x00000194`, drzwi z
   `ARCHOLOS_VOLFZACKE.ZEN`). `worklist.py` jeszcze tego nie rozpoznaje; propozycja: przy luce `unmapped` z małym adresem
   sprawdzić w logu nieznalezione voby z tego samego stosu i ich świat w plikach ZEN, i oznaczyć zadanie jako „dialog
   spoza swojego świata”.
9. `Mem32::implAlloc` (pierwszy pasujący wolny blok) dalej przegląda bloki liniowo; przy obecnej liczbie alokacji to nie
   jest wąskie gardło.

## Stosowanie patcha na czystym repozytorium

```powershell
git apply --check patches/025-ikarus-automation.patch
git apply patches/025-ikarus-automation.patch
```

Patch zawiera też nową wersję `patches/zenkit-opengothic.patch`. Na czystym klonie CMake nałoży ją na ZenKit sam. W
istniejącym checkoucie, gdzie stara wersja jest już nałożona na `lib/ZenKit`, CMake jej nie nałoży (nie da się jej
odwrócić ani nałożyć): wtedy wycofać starą (`git -C lib/ZenKit apply -R <stary patch>` albo `git -C lib/ZenKit checkout .`
— to kasuje tylko wygenerowane zmiany submodułu) i ponownie uruchomić konfigurację CMake. Nowe pliki źródłowe są zbierane
przez `GLOB_RECURSE`, więc po nałożeniu patcha konfigurację trzeba uruchomić ponownie.
