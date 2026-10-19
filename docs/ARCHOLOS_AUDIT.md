# Kroniki Myrtany — audyt zgodności OpenGothic

Stan: 19 października 2026. OpenGothic: `501b5c117a9ceb200c39fa3e485046fa4200b593`.

Aktualny stan przekazania po implementacji i próbach użytkownika z 20.10.2026: [ARCHOLOS_HANDOFF_CLAUDE.md](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_CLAUDE.md), szczegółowe tabele pozostałych obszarów: [ARCHOLOS_HANDOFF_BACKLOG.md](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_BACKLOG.md). Ten dokument zachowuje historyczne ustalenia audytu.

Aktualizacja po audycie: pierwszy pakiet napraw jest zaimplementowany i zbudowany w `build-touch`, bez uruchamiania testów ani gry przez agenta. Zakres i ręczne próby: [ARCHOLOS_TESTING.md](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_TESTING.md). Poniższy audyt i CSV zachowują stan wyjściowy; nie są bieżącą listą niezaimplementowanych funkcji.

**Do pełnej zgodności brakuje zarówno funkcji Ikarusa/LeGo, jak i podstawowych externali Gothica oraz poprawnej semantyki części istniejących mostów. Samo uruchomienie świata i brak błędów inicjalizacji nie wystarczają do potwierdzenia przechodniości gry.** Najpierw należy naprawić synchronizację AI, operacje questowe na przedmiotach i reprezentację obiektów, następnie sceny, wyposażenie i interfejs.

To audyt statyczny kodu i bytecode'u. Nie wykonałem pełnego przejścia ani nowych testów rozgrywki i nie zmieniałem implementacji silnika. Skutki fabularne opisane niżej są wnioskami z brakujących operacji, a nie raportami z odtworzonych w tej sesji awarii.

## Materiał i metoda

- Wskazana dekompilacja: `C:/Users/Ernest/AppData/Local/Temp/claude/C--Users-Ernest-Desktop-opengothic/b483b2a9-e243-4cde-b63e-7d3878f44c49/scratchpad/tcom/tcom-decompilation-main/Source` — 4999 plików `.d`, 37 428 rozpoznanych definicji funkcji. Jej README ostrzega o błędach i pominiętym kodzie.
- Dodatkowo odczytany, bez uruchamiania gry: `GOTHIC.DAT` z lokalnego `C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana/Data/KM_ScriptsPL.mod` — 206 709 symboli, 6 728 153 bajty bytecode'u. `GAME_VERSION=23`, `BUILD_VERSION=142565`, Ikarus `10202`, LeGo `2.7.0`.
- SHA-256 tego archiwum: `B4900CC7DD109DA79017F6A2F2E12020CCDE79886F9DFE0573CBB159887BF5F6`.
- Format VDF/DAT i rozmiary instrukcji odczytane z lokalnego ZenKit: `lib/ZenKit/src/Vfs.cc`, `DaedalusScript.cc` i `include/zenkit/DaedalusScript.hh`. Czytnik sprawdził koniec tablicy symboli, długość bytecode'u i koniec strumienia instrukcji.
- Wywołania `BE` porównane z rejestracjami externali w `common/`. Cele `CALL__*` i `HookEngine*` porównane z rejestracjami adresów, nadpisaniami funkcji i rzeczywistymi miejscami generowania zdarzeń.
- Cel natywny uznany za rozpoznany statycznie tylko przy bezpośrednio poprzedzającym `PUSHI` albo `PUSHV` stałej całkowitej. Wyrażenia i wskaźniki wyliczane w czasie działania pozostają osobną kategorią. Wartość początkowa 0 wskaźnika DLL nie oznacza wywołania adresu 0.
- Istniejący `build-touch/opengothic/log.txt` posłużył pomocniczo do nazw callbacków. Nie jest dowodem zachowania aktualnego buildu: np. zgłasza `_FF_HOOK`, który aktualny kod obsługuje osobno. Dla 55 jednoznacznych wpisów nazwy z logu zgadzają się z numerami callbacków dekompilacji odczytanymi z lokalnego DAT.

**Liczby oznaczają miejsca w kodzie, nie wykonania podczas gry ani procent ukończenia zgodności.** Część funkcji jest warunkowa, diagnostyczna lub nieosiągalna w typowym przejściu.

| Zakres | Wynik |
|---|---|
| Hooki w dekompilacji | 73 miejsca: 15 z mostem zdarzenia, 57 bez mostu, 1 nierozwiązane wyrażenie |
| Hooki w pełnym DAT | 87 miejsc: 19 z mostem, 2 ze specjalną obsługą FF/AI, 60 bez mostu, 1 deklarowane jako obsługiwane bez wywołania, 1 w nadpisanej funkcji, 4 dynamiczne/wewnętrzne |
| Wywołania `CALL__*` w DAT | 163 miejsca: 40 z zarejestrowaną implementacją, 9 z implementacją pozorną, 66 bez handlera, 8 w nadpisanych funkcjach, 40 dynamicznych |
| Rozpoznane braki adresów natywnych | 56 różnych adresów w 66 miejscach; dodatkowe cele mogą kryć się w wyrażeniach dynamicznych |
| Externale Daedalusa | 317 deklaracji; 86 bez rozpoznanej rejestracji, z czego **10 ma bezpośrednie wywołania w bytecode** |

## Co już istnieje

W kodzie są: emulowana pamięć 32-bitowa, operacje na symbolach i instancjach, pętle Ikarusa, dynamiczne wywołania skryptowe, część funkcji matematycznych i ciągów, obsługa plików LeGo w zapisie OpenGothic, FrameFunctions, podstawowe `AI_Function`, szukanie moverów i odczyt ich stanu, część widoków/fontów/tekstów oraz przekazanie zdarzeń gry do hooków.

Zdarzenia obejmują m.in. podnoszenie przedmiotu, zmianę atrybutu, obrażenia i upadek, klawisze, zmianę strefy muzyki, zmianę świata, część otwierania zamków, ekran statystyk i rozpoczęcie/zakończenie kamery. Aktualny checkout ma też wpisywane odpowiedzi dialogowe (`?Name`). Tych elementów nie należy ponownie traktować jako całkowicie niezaimplementowanych.

## P0 — poprawność scen i postępu gry

### 1. Zwykłe externale Gothica używane przez Kroniki

To największy brak pomijany przez audyt skupiony wyłącznie na Ikarusie. Poniższe symbole są externalami w DAT, mają instrukcje wywołania `BE`, a ich nazw nie ma w rejestracjach ani implementacjach `common/`.

| External | Miejsca wywołania w DAT | Przykłady użycia i skutek brakującej operacji |
|---|---:|---|
| `AI_WAITTILLEND` | **1577** | `TRIA_WAIT`, `_TRIA_INITNPC`, sceny questowe; brak oczekiwania na kolejkę drugiego NPC może rozjeżdżać kolejność dialogów i animacji |
| `MOB_CREATEITEMS` | **123** | `B_TRANSFERALLINVENTORYTOMOB`, Huxlay, SQ509; przedmioty nie trafiają do obiektu/skrzyni |
| `WLD_REMOVEITEM` | 12 | KQ402, SQ202; quest nie usuwa przedmiotu ze świata |
| `AI_AIMAT` | 10 | Q602, GQ001, Q205; brak zaplanowanego celowania |
| `AI_SHOOTAT` | 10 | Q205, Q304, dialog Thiago; brak zaplanowanego strzału |
| `AI_SND_PLAY` | 5 | `B_GIVEINVITEMS`, Q514, Ruud; brak dźwięku w kolejce AI |
| `AI_DROPITEM` | 3 | SQ505, próba Wolfsona, SQ304; brak wyrzucenia przedmiotu |
| `AI_STOPAIM` | 2 | `Q406_KESSELSHOOT`; brak zakończenia celowania |
| `WLD_INSERTOBJECT` | 2 | `MEM_INSERTVOB`, diagnostyka wersji; brak utworzenia voba przez tę drogę |
| `AI_STOPFX` | 1 | `USE_SQ509_PLANT`; brak zakończenia efektu |

Dowód zachowania: [Gothic::setupVmCommonApi](C:/Users/Ernest/Desktop/opengothic/common/gothic.cpp:955) ustawia domyślną obsługę nieznanego externala. [ZenKit](C:/Users/Ernest/Desktop/opengothic/lib/ZenKit/src/DaedalusVm.cc:809) usuwa argumenty, wstawia zerowy/pusty wynik i wywołuje logger. Operacja gry nie zostaje wykonana.

**Do zrobienia:** zaimplementować te operacje w kolejce AI i świecie, z poprawną semantyką oczekiwania. Nie wystarczy dodać nazw do rejestracji. Pozostałe 76 niepowiązanych externali nie ma bezpośrednich wywołań `BE` w tym DAT — nie są automatycznie 76 kolejnymi blokerami.

### 2. `AI_Function` gubi kontekst wykonującego NPC

[GameScript::eventPlayAni](C:/Users/Ernest/Desktop/opengothic/common/game/gamescript.cpp:915) dostaje `Npc&`, ale przekazuje dalej wyłącznie nazwę animacji. [DirectMemory::eventPlayAni](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/directmemory.cpp:253) wywołuje funkcję bez ustawienia `SELF` na tego NPC i bez odtworzenia kontraktu `OTHER`/`ITEM`.

Oryginalne `LeGo/AI_Function.d::_AI_FUNCTION_EVENT` ustawia `self = _^(ECX)`, zeruje `other` i `item`, a mechanizm hooka odtwarza poprzedni kontekst. To ma znaczenie dla skryptów, które wykonują operacje na `SELF`. W dekompilacji jest 1729 nazwanych wywołań `AI_FUNCTION`, 30 `AI_FUNCTION_S`, po jednym `AI_FUNCTION_I` i `AI_FUNCTION_SS`.

**Do zrobienia:** zachować wykonującego NPC, ustawić i odtworzyć globalne instancje oraz kontekst VM także przy wyjątkach i zagnieżdżeniu. Dokończyć dekodowanie wszystkich faktycznie używanych parametrów i ucieczek tekstu. Gałąź `NSII` jest pusta, ale nie znalazłem jej wywołań w tej dekompilacji — ma niższy priorytet niż kontekst.

### 3. Reprezentacja obiektów nie zapewnia jeszcze semantyki silnika

- **Żywy wskaźnik może zmienić właściciela.** [InstanceHandles](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/instancehandles.cpp:54) po 4096 różnych instancjach nadpisuje najstarszy slot, nawet jeśli poprzedni obiekt nadal żyje. Stary adres zaczyna wskazywać inną postać/przedmiot. Brakuje stabilnej tożsamości, zwalniania po śmierci obiektu i kontroli nieaktualnych adresów. To deterministyczny problem konstrukcji, nie potwierdzenie, że konkretny quest przekroczył limit w tej sesji.
- **Focus nadpisuje prawdziwe pole skryptowe.** [eventFocus](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/directmemory_events.cpp:235) zapisuje wskaźnik do `C_NPC.voice_pitch`, a `OCNPC.FOCUS_VOB` jest aliasem tego pola. Kroniki używają `VOICEPITCH` m.in. w `SPL_COST_SLOWDOWN.d`. Potrzebne osobne dane zgodności, bez współdzielenia z polem dostępnym skryptom.
- `OCNPC.INTERACTMOB` jest aliasem `spawn_delay`, lecz most nie wypełnia go rzeczywistym interaktywnym obiektem NPC. Zaklęcie otwierania zamków odczytuje to pole. `OCNPC.BITFIELD` trafia do tablicy `mission`; samo zapisanie bitu nie blokuje ruchu ani nie ustawia odpowiedniego stanu natywnego.
- Brakuje m.in. odwzorowania `_ZCVOB_*`, `MAG_BOOK`, pól modelu i wyglądu oraz części inwentarza. Wskazana dekompilacja odczytuje je m.in. w telekinezie, zmianie wyglądu, spowolnieniu, nurkowaniu i kategoriach ekwipunku. [syncHandleObject](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/directmemory_engine.cpp:36) synchronizuje głównie pozycję; zapis rotacji macierzy nie obraca rzeczywistego obiektu.
- Alias członka VM i surowy `MEM_ReadInt(ptr+offset)` to różne ścieżki. Zarejestrowanie `OCNPC.ATTRIBUTE` nie sprawia, że cały surowy obiekt ma poprawny layout i aktualne wartości.

**Do zrobienia:** stabilny rejestr obiektów i jawne adaptery używanych pól, z zapisem do rzeczywistego stanu gry. Rozszerzać go według użycia w DAT, zamiast bezrefleksyjnie kopiować wszystkie struktury Gothica.

### 4. Zegary, waynet i lista obiektów świata

[setupEngineMemory](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/directmemory.cpp:531) ustawia `WLDTIMER` i `WAYNET` na `BAD_BUILTIN_PTR`, `TIMESTEP` na stałe 1. `zTimer` jest podpięty do pamięci, ale nie jest aktualizowany w `tick()`. `refreshNpcList` odświeża wyłącznie listę NPC; ogólnej listy `MEM_World.voblist` brakuje.

To dotyczy `SCALETIME`, `CHANGETIMESPEED`, kodu waypointów i losowania zamków. `CHANGETIMESPEED` ma 15 nazwanych miejsc wywołania w dekompilacji. `TIMER()` LeGo korzysta z osobno obsługiwanego `MEM_GetSystemTime`, więc **nie oznacza to, że wszystkie FrameFunctions stoją**. Problemem są konkretne pola i zgodność czasu/pauzy.

**Do zrobienia:** adapter czasu świata, czasu klatki, pauzy i zmiany poziomu; odczyt waypointów i potrzebnych list vobów. `MEM_SearchVobByName` obecnie znajduje tylko movery, a nie dowolne NPC, przedmioty, skrzynie czy dekoracje.

### 5. Ochrona przedmiotów, kompanów i zdarzenia walki

Brak mostów dla:

- `OCNPC__EVDROPVOB_HOOK` (`0x7538c0`) i `ITEMIMPORTANTFLAG_CHECKSELLINGITEMS` (`0x68b840`): blokowania wyrzucania/sprzedaży istotnych przedmiotów. Natywne menu bezpośrednio wywołuje `sellItem`/`dropItem`.
- `OCNPC__GETNEXTENEMY_REPLACEHOOK` (`0x734f4d`) i pięciu hooków `HIDEFOCUSFORPARTYMEMBERS_*`: poprawki dla kompanów i focusu.
- `OCNPC__DROPUNCONSCIOUS_HOOK` (`0x735fab`): skrypt zawiera jawne ścieżki `Q205_ONDROPUNCONSCIOUS` i `Q306_ONDROPUNCONSCIOUS`, więc to nie jest jedynie kosmetyka upuszczonej broni.
- `EV_FINISHINGMOVE_HOOK` (`0x751af0`), `LOCKINVENTORY` (`0x762250`) i ustawiania właściwości zakończonego dialogu (`0x702910`).

**Do zrobienia:** odtworzyć decyzje skryptowe przed operacjami natywnego silnika: czy wolno sprzedać/upuszczać, czy dany NPC jest poprawnym przeciwnikiem, co zrobić przy utracie przytomności i kiedy blokować ekwipunek.

### 6. Zamykanie/otwieranie zamków jest tylko częściowe

Most `CanOpen` oraz `SpellSetup` istnieje. Brakuje `HANDLE_SETPROPERPICKLOCKSTRLEN` (`0x723ee2`) i `SPELL_PICKLOCK_FOCUS` (`0x485fc4`). Losowanie kodów przechodzi po brakującej ogólnej liście vobów; adapter moba nie przenosi zapisu tekstu `PICKLOCKSTR` do rzeczywistego zamka. `syncFocusMage` przenosi jedynie priorytety, a `ClearKeyBuffer` jest pusty.

**Do zrobienia:** poprawne skracanie kodu przez talent/bonus, losowanie i zapis kodu, walidacja celu zaklęcia, odczyt aktualnego moba NPC oraz obsługa stanu klawiszy. Testować osobno wytrychy i magię.

## P1 — wyposażenie, questowe sceny i prezentacja

### 7. Sloty i wyposażenie

[setupNpcFunctions](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/directmemory.cpp:1946) ma `GetSlotItem -> 0` i pusty `EquipWeapon`. W DAT potwierdzone są też cele bez handlerów: `PutInSlot 0x749cb0`, `RemoveFromSlot 0x74a270`, `DropFromSlot 0x74a590`, `Equip 0x739c90`, `UnequipItem 0x7326c0`, `EquipBestWeapon 0x74ef30`, `DropAllInHand 0x7375e0`.

`INIT_QUIVERS_ALWAYS` jest celowo wyłączone. Skutki obejmują pochodnie i ich przywrócenie po wczytaniu, kołczany, zajętość rąk, uzbrajanie NPC i przedmioty po utracie przytomności. Brakujące definicje wrapperów w dekompilacji nie są dowodem, że to externale DLL: pełny DAT zawiera normalne funkcje skryptowe `OCNPC_PUTINSLOT` i `OCNPC_REMOVEFROMSLOT`.

### 8. Kategorie ekwipunku, crafting i handel

Brak większości `CATINV_*`, zarządzania listami przedmiotów i hooków kontenerów. Z 23 miejsc HookEngine w `APPLYGOTHIC1MODESETTINGS.d` tylko `InventoryInsert` ma obecnie most. Część pozostałych to poprawki oryginalnego UI, które można zastąpić równoważnym zachowaniem menu OpenGothic.

Brakuje również `OCINFORMATIONMANAGER__UPDATE_HOOK`, `CRAFTINGVIEW_UPDATE`, `ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK` i `OCITEMCONTAINER__DRAWITEMINFO_HOOK`. Potrzebna zgodność wyboru i prezentacji receptur, informacji o składnikach/wyniku, blokowania działań oraz interakcji z handlem. Wpisywanie odpowiedzi `?Name` jest już dodane; wymaga weryfikacji w scenariuszach moda, a nie ponownego napisania.

### 9. Kamery i specjalne operacje questowe

Samo `OnTrigger/OnUntrigger` kamery nie zapewnia pełnej zgodności. Brak `zCAICamera::StartDialogCam 0x4b20f0` i trzech hooków sprawdzania globalnej cutscenki w ruchu, HUD i dostępności menu. Należy zachować kolejność `TRIA_*`, blokady sterowania i odtworzenie kamery po scenie.

Są też bezpośrednie wywołania silnika spoza bibliotek: `0x688a70` w `Q204_ARAXOSJOINTEST_COLLISION` i `Q304_TRAPCUTSCENE_SETCHASMDETECTION_ONFRAME`, `0x475a50` w spowolnieniu, `0x73d340` w `OCNPC_ISSPELLACTIVE`, operacje aktywnych animacji i zatrzymania dźwięków. Cele te nie mają handlerów. Ich dokładną semantykę należy odtworzyć przed wdrożeniem odpowiedników; nazwa questowej funkcji nie zastępuje identyfikacji API.

Most obrażeń istnieje, lecz descriptor nie zawiera wszystkich rzeczywistych danych obrażenia, a `FDAMAGETOTAL` jest odczytywane jako dodatni bonus. Należy porównać wynik z oryginałem dla penetracji pancerza, pierścieni, strzałów, magii i obrażeń nieśmiertelnych; nie uznawać samej obecności hooka za dowód zgodności balansu.

### 10. KmLib: muzyka, osiągnięcia i statystyki

[setupWinapiFunctions](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/directmemory.cpp:1640) zwraca pozorne uchwyty `LoadLibrary`/`GetProcAddress`; [Cpu32::callFunction](C:/Users/Ernest/Desktop/opengothic/common/game/compatibility/cpu32.cpp:241) pomija wywołania uzyskanych w ten sposób funkcji.

Nie są wykonywane `InitializeMenu`, `InitializeGamestart`, `InitializeAlways`, `GameServices_UnlockAchievement/GetStat/IncrementStat`, `MusicSystem_OverrideTrack/DisableTrackOverride` oraz część diagnostyki. Osiągnięcia same w sobie nie muszą blokować ukończenia fabuły, ale muzyka i statystyki nie spełniają kryterium pełnej zgodności. `GetStat` nie powinno zwracać przypadkowego poprzedniego wyniku rejestru.

**Do zrobienia:** jawny adapter eksportów używanych przez Kroniki, z właściwymi wynikami i trwałością danych; dla obsługiwanych platform odpowiednik integracji muzycznej i usług gry. Nie jest konieczne emulowanie całego procesu Gothic2 ani dowolnej biblioteki DLL.

### 11. Rysowanie i zmiany świata

Istnieją podstawowe widoki, teksty i teksturowane prostokąty. Nadal brakuje sprite'ów, `Render/Draw3D`, części funkcji renderera, zagnieżdżonych widoków i animacji okien. Kolor/alpha tekstu nie trafiają do `GthFont::drawText`. Rozmiar ekranu widziany przez skrypt jest stały `800x600`, a zdarzenie rozdzielczości generowane tylko raz.

`zCTexture::Load` zwraca 0; brak funkcji używanych przez `APPLYHOUSEWALLTEXTURE` (`0x5f5530`, `0x5a9ce0`, `0x5f54d0`). `RemoveVob`, `SetVobName` i `ApplySomeSettings` mają puste implementacje. Przydzielenie pamięci pod patch kodu oryginalnego EXE również nie przenosi jego efektu na silnik OpenGothic.

Do pełnej zgodności dochodzą szczegóły statusu, fontów (`GetFontName`), przełączania ustawień, dziennika (`LOG_MOVETOTOP` jest pusty), wyglądu domu, efektów i muzyki. Ich priorytet dla samego przejścia gry jest zwykle niższy od operacji questowych.

## P0/P1 — zapis i stabilność wymagają osobnej weryfikacji

Obsługa plików LeGo **istnieje**: `CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`, `GetFileSize`, `GetLastError` działają na wirtualnych plikach zapisanych w `game/dma`. Gra ponawia inicjalizację skryptów po wczytaniu. Nie należy zgłaszać tych funkcji jako całkowicie brakujących.

Wymagają sprawdzenia:

- zapis → zamknięcie procesu → wczytanie, a także ponowne wczytanie w tym samym procesie;
- odtworzenie PermMem, funkcji klatkowych, widoków, ciągów i referencji do NPC/przedmiotów;
- zmiana świata i powrót do odwiedzonego świata bez referencji do zniszczonych obiektów;
- zapis w kilku slotach i quicksave — warstwa LeGo celowo widzi jeden wewnętrzny slot, ponieważ jej pliki są osadzone w każdym zapisie OpenGothic;
- hook `ReadSavegame` jest wpisany do `isHookAnswered`, ale nigdzie nie jest generowany. Ustawienie slotu jest zastąpione bezpośrednio w `setupSaveSupport/eventLoadGame`, więc sam brak callbacka nie dowodzi awarii wczytywania, natomiast raport obsługi hooków jest zbyt optymistyczny.

`Cpu32` rozpoznaje tylko część instrukcji x86; część pozostałych loguje i idzie dalej, a nieznane wywołanie natywne może zostawić poprzedni wynik. `qsort` i `FINAL` są niezaimplementowane. W `Mem32::readInt/writeInt` brakuje sprawdzenia, czy pełne cztery bajty mieszczą się w regionie. Są też ścieżki dereferencji bez pełnej kontroli błędów. To wymaga testów granic, wyjątków i callbacków pamięci; nie oznacza potrzeby budowy pełnego emulatora x86.

## Zalecana kolejność prac i kryteria odbioru

1. **Kolejki AI i postęp questów:** `AI_WAITTILLEND`, `MOB_CREATEITEMS`, `WLD_REMOVEITEM`, celowanie/strzelanie/upuszczanie; poprawny kontekst `AI_Function`. Test sceny co najmniej trzech NPC z asynchronicznym oczekiwaniem i przekazaniem przedmiotu.
2. **Stabilne obiekty i czas:** tożsamość wskaźników, oddzielenie focusu od `VOICEPITCH`, adaptery bitów i moba, zegary/listy świata. Test ponad 4096 różnych instancji, usuwania i ponownego tworzenia obiektów oraz zmiany świata.
3. **Mechaniki questowe:** ochrona przedmiotów, kompanów, utraty przytomności, wytrychów/magii, rąk i wyposażenia. Porównać Q205/Q306, sceny Q204/Q304/Q406 i zachowanie przedmiotów ważnych z oryginalnym Gothic2.
4. **Trwałość:** PermMem i FF po pełnym restarcie procesu, sloty, quicksave i powrót między światami. Zapisy wykonywać także przy aktywnym efekcie czasowym i niestandardowym widoku.
5. **UI i sceny:** crafting, kategorie, handel, odpowiedzi tekstowe, kamery, status, HUD, zmiana rozdzielczości i skalowanie.
6. **Pełna zgodność prezentacji:** KmLib, muzyka, statystyki/osiągnięcia, dom, grafika i dźwięk.
7. **Regresja fabuły:** przejście wszystkich rozdziałów, dróg gildyjnych i zakończeń oraz testy zadań opcjonalnych. Brak błędów w logu jest warunkiem pomocniczym, nie dowodem przechodniości. Wyniki i stan questów porównywać z oryginałem dla tej samej wersji danych.

## Dane pomocnicze i ograniczenia istniejącej dokumentacji

- [Hooki z DAT](C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/dat-hooks.csv) — adres, callback, funkcja rejestrująca, offset bytecode'u i status mostu.
- [Wywołania natywne z DAT](C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/dat-native-calls.csv) — cele rozpoznane i dynamiczne, implementacje pozorne, lokalizacja handlera.
- [Niepowiązane externale](C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/unbound-externals.csv) — także niewywoływane deklaracje; kolumna `static_call_sites` rozróżnia te przypadki.
- [Hooki dekompilacji](C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/source-hooks.csv) — plik i wiersz wskazanego źródła oraz nazwa symbolu pod numerem callbacka w lokalnym DAT.
- [Podsumowanie liczb](C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/summary.json).

`tools/ikarus/analyze.py` jest przydatną heurystyką, ale porównuje głównie wywołania bibliotek, nie pełny bytecode moda. Nie rozpoznaje rejestracji przez pomocnicze `winapi(...)`, więc błędnie zgłasza obsługiwane funkcje plikowe. Nie odróżnia też działającego handlera od pustej implementacji i nie pokrywa wielu bezpośrednich wywołań Kronik.

`docs/IKARUS.md` opisuje poprzednie etapy prac: jego lista braków jest częściowo nieaktualna, np. w sprawie fontów, zapisu, statusu i zdarzeń kamery. Jednocześnie nie uwzględnia powyższych podstawowych externali i błędów kontekstu. Ten audyt sprawdza stan kodu; nie podaje nieweryfikowalnego „procentu gotowości”.
