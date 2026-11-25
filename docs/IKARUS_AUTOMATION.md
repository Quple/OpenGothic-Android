# Ikarus/LeGo bez obsługi: konwerter silnika, rejestr luk, przebieg gry bez człowieka

Stan: **25.11.2026 wieczorem, praca wznowiona** (sekcja „Postęp 25.11 wieczorem” niżej). Wszystko poniżej jest zaimplementowane,
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
| Wszystkie dialogi z konsoli | `mods/devtools/index.js` | `dlgall [słowa]`, `dlgall resume` (po crashu, pomija dialog, który go wywołał), `dlgall stop` — to samo co przycisk „Test all”, kilka dialogów na klatkę. |
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

Mod:

```powershell
$g = 'C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana'
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini                        # tylko statycznie
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini --run "wait 30; comcheck test ikarus; comcheck fix all; wait 5" --timeout 300
python tools/ikarus/modcheck.py --game $g --ini TheChroniclesOfMyrtana.ini --run "wait 30; dlgall; waitfor Done: 1700; wait 5" --timeout 1800 --out build-ikarus/com-dialogs   # ok. 630 dialogów; dalej: "wait 30; dlgall resume; waitfor Done: ..."
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
