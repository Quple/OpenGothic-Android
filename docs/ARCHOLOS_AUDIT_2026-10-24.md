# Kroniki Myrtany — audyt 24.10.2026

Analiza obejmuje rzeczywisty `GOTHIC.DAT` z lokalnego `KM_ScriptsPL.mod`, dekompilację
CoM oraz źródła Ikarusa i LeGo. Narzędzie i interpretację wyników opisuje
[tools/ikarus/README.md](../tools/ikarus/README.md). Zestawienie zastępuje historyczne
liczby braków z audytów 19–20.10; nie zastępuje ręcznej weryfikacji rozgrywki.

## Wynik statyczny

| Pomiar | Wynik |
| --- | ---: |
| Pliki `.d` | 5087 (4999 CoM oraz 88 bibliotek) |
| Deklaracje źródłowe, z instancjami/prototypami | 52042 |
| Miejsca wywołań w źródłach | 238532 |
| Symbole DAT | 206709 |
| Miejsca wywołań BL/BE w DAT | 240563 |
| Nierozpoznane cele bezpośredniego BL | 0 |
| Wywoływane externale bez wiązania | 0 |
| Zadeklarowane, bez bezpośrednich wywołań, externale bez wiązania | 76 |
| Różne adresy natywne bez zarejestrowanego handlera | 44 (49 miejsc) |
| Różne adresy HookEngine bez wykrytego mostu | 49 (50 miejsc) |
| Dynamiczne wywołania/hooki DAT bez rozstrzygniętego adresu | 41 miejsc |

SHA-256 DAT: `7fba1af724ae528836a2537573b0d36024cd8a92d493e15a11c8e159ef44d54e`.

Liczby opisują miejsca w kodzie, nie częstotliwość podczas gry ani osiągalność.
Brakujące adresy obejmują także funkcje przywoływane wyłącznie dynamicznie i kod,
który może mieć odpowiednik w natywnym UI. Sam wpis w tabeli handlerów nie dowodzi
poprawnej implementacji. Raport osobno wskazuje puste implementacje, stałe wyniki,
TODO oraz funkcje zastąpione przez C++.

Małe migawki są w [archolos-audit/2026-10-24](archolos-audit/2026-10-24).
Pełne `functions.csv`, `callgraph.csv`, JSONL oraz około 47 MiB dezasemblacji pozostają
w lokalnym, ignorowanym `build-touch/archolos-audit-2026-10-24`. Nazwy i liczby nie
stanowią kopii pełnych skryptów moda.

## Zmiany w tym pakiecie

- Dziennik: `QuestStat::Old` błędnie odpowiadał `Failed`, a `Failed` odpowiadał
  `Success`. Poprawiono mapowanie filtrów obu interfejsów. Stan zadania w zapisie
  nie jest przepisywany; stare ukończone zadania powinny przejść do właściwej listy.
- Trialog: pominięty głos nie jest już przechowywany w `lingering` przy kamerze
  skryptowej. Pominięcie zwalnia dźwięk i zatrzymuje gesty wszystkich rozmówców.
- Muzyka: CoM definiuje `MUSICTRACK` i `MUSICZONE` w `GOTHIC.DAT`, a utwory OGG
  przechowuje w `_WORK/DATA/MUSIC/KM` archiwum `KM_Music.mod`. Dodano mapowanie
  stref dzień/noc/walka, strumieniowy dekoder Vorbis, pętlę i przejścia głośności.
  `MUSIC_OVERRIDETRACK` oraz `MUSIC_DISABLEOVERRIDE` mają rzeczywistą obsługę.
  Menu wybiera `02.OGG` jawnie poza rozgrywką: skrypty CoM pozostawiają
  `C_MENU.MUSICTHEME` puste i normalnie polegają na KmLib. Samo mapowanie
  `SYS_MENU` było niewystarczające, co potwierdziło zgłoszenie użytkownika.
- VDF: po sortowaniu archiwów według priorytetu moda i malejącego czasu montowanie
  zachowuje pierwszy plik. Nowszy plik bazowej gry nie zastępuje pliku moda.
- `STOPALLSOUNDS`: natywne `RemoveAllActiveSounds` zatrzymuje efekty świata i efekty
  globalne, również gdy pozostały współdzielone uchwyty dźwięku.
- Natywne Equip/Unequip: wyposażanie posiadanego przedmiotu jest idempotentne;
  ponowne wywołanie nie zdejmuje wyposażenia. Pochodnia korzysta z `setTorch`.
- `WLD_INSERTOBJECT`: utworzenie wizualnego obiektu w punkcie świata dla obsługiwanych
  siatek/modeli/morfów/PFX; transformacje i nazwy wizualizacji trafiają do osobnego,
  opcjonalnego wpisu zapisu. Nieobsługiwane rozszerzenia są zgłaszane jawnie.
- Ikarus: `MEM_WriteString` zapisuje rzeczywisty tekst, także przez mapowanie pamięci;
  `CALL_RetValAszStringPtr` odczytuje pożyczony wskaźnik bez zwalniania go.
  `MEM_CallByID/Ptr` respektują externale i nadpisania VM przez `invoke_function`.
  `_FF_HOOK` jest rozpoznawany bez różnicy wielkości liter; frame functions nadal
  działają przez tick, a AI_Function przez natywne zdarzenia animacji.
- Zabezpieczenia: brak dereferencji pustej instancji, brak zwalniania pamięci pinned,
  poprawne kopiowanie nakładających się regionów, kontrola przepełnień/OOM,
  sprzątanie buforów callbacków, brak nadpisywania nadal żywych uchwytów po
  wyczerpaniu tablicy. Destruktory i przenoszenie przedmiotów aktualizują `user_ptr`.
  Nieznane wywołanie lub nieobsługiwana instrukcja x86 kończą thunk z wyzerowanym
  wynikiem, zamiast używać poprzedniego EAX lub interpretować argumenty jako kod.

## Otwarty zakres Ikarus/LeGo

Nie ma podstaw do deklaracji „100% zgodności” ani „braku critical error”. Pozostają
między innymi renderer/sprite'y LeGo, część CATINV i niestandardowego craftingu,
materiały domów, wybrane operacje slotów/kolizji/zaklęć i kontrakty hooków. Szczegóły
adresów i callbacków zawiera `addresses.csv`; `native-and-hooks.csv` zachowuje każde
miejsce osobno. Część wywołań Windows dotyczy schowka i nie ma priorytetu fabuły.

Puste `MEMINT_StackPushInt`/`PopResult` bywają celowe: adapter naked-call korzysta
z istniejącego stosu VM. Nie należy zamieniać ich w zwykłe funkcje zerujące stos.
Z drugiej strony `FINAL` nadal jest atrapą (bez bezpośrednich wywołań w tym DAT),
a obecne `qsort` nie realizuje dowolnego komparatora. Dynamiczne cele funkcji,
arbitralne eksporty KmLib i dodatkowe instrukcje x86 wymagają dalszej pracy.

## Weryfikacja i próby użytkownika

Kompilacja `build-touch` zakończyła się powodzeniem dla `Gothic2Notr.exe` i
`Spacer.exe` po głównym pakiecie oraz po poprawce menu (01:51, kod wyjścia 0).
Gry, testów automatycznych ani scen `comcheck`
agent nie uruchamiał, zgodnie z wcześniejszą instrukcją zachowaną w handoffie.
Analizator przeszedł pełny DAT i wyeksportował raporty. To nie jest test rozgrywki.

Log z uruchomienia użytkownika potwierdził wczytanie 41 OGG i 101 mapowań stref/menu
oraz odtworzenie `03.OGG` po wczytaniu świata. Nie potwierdził odtworzenia menu;
dlatego dodano osobną poprawkę pustego `C_MENU.MUSICTHEME`. Odsłuch i ocena
poprawności doboru muzyki w świecie nadal należą do użytkownika.

Aktualizacja po kolejnym odsłuchu menu: log potwierdził wybór `02.OGG`, lecz użytkownik
nadal słyszał muzykę Gothica. Użytkownik odsłuchał plik wyciągnięty bezpośrednio
z `KM_Music.mod` i potwierdził, że to właściwa muzyka. Statyczna analiza eksportu
`InitializeMenu` w lokalnym KmLib również potwierdza wybór „02”. Przeszukanie
katalogów wszystkich lokalnych archiwów znalazło `02.OGG` tylko w `KM_Music.mod`.
Nie uznajemy więc samego komunikatu `playing` za potwierdzenie poprawnego odsłuchu.
Odtwarzacz jawnie pauzuje źródło OpenAL DirectMusic przy przejściu na OGG i zapisuje
odcisk wczytanych danych. Dla właściwego pliku oczekiwane wartości to
`bytes=1371813 fnv1a32=1710945220 rate=48000 channels=2` (SHA-256 wyciągniętego
pliku: `3e254d6acba637577ba2030ee230b3ad523a9c31ee8dfda2244ebc491eb195ac`).
Ta poprawka oraz faktyczne dane wybrane przez VFS wymagają jeszcze potwierdzenia
w następnym uruchomieniu użytkownika. Zmiana głośności nie restartuje już aktywnego
OGG przez ponowne `play()`; pauza i wznowienie muzyki nadal działają osobno.

Kolejna próba użytkownika, jeszcze na EXE z 01:51: w lokalnym `build-touch/opengothic/Gothic.ini`
znaleziono `musicVolume=0.000000`. Po ustawieniu `0.500000` użytkownik potwierdził,
że słyszy **jednocześnie** motyw Kronik i Gothica 2. To potwierdza działanie utworu
Kronik, ale nie potwierdza źródła drugiego strumienia. Pauza źródła OpenAL i wyłączenie
ponownego uruchamiania DirectMusic przy aktywnym OGG są w nowym buildzie; ich efekt
wymaga odsłuchu. Nie przypisywać rozmiarowi/formatowi pliku przyczyny nakładania muzyki.

Build tej poprawki zakończył się 24.10.2026 o 02:12, kod wyjścia 0;
zlinkowano `Gothic2Notr.exe` i `Spacer.exe` w `build-touch/opengothic`.
Odsłuch nowego EXE przez użytkownika pozostaje do potwierdzenia.

### Ustalona przyczyna nakładania muzyki menu

Odsłuch EXE z 02:12 nadal wykazał oba motywy. Śledzenie źródeł OpenAL wykazało:

- `OpenGothicMusicProvider`: źródło 1 urządzenia muzyki, poprawnie zapauzowane
  (`AL_PAUSED`, 4115).
- `RecordedMusic`: źródło 2 tego urządzenia, odtwarzające właściwy OGG
  (`AL_PLAYING`, 4114).
- Dodatkowe **statyczne** źródło na innym urządzeniu: **7610368 bajtów PCM,
  44100 Hz**, odtwarzane mimo braku klikania w menu.

To `GAMESTART.WAV` z `Sounds_Addon.vdf`: 16-bit stereo, 44100 Hz, dokładnie
7610368 bajtów PCM, około 43,14 s. `MainWindow::render()` uruchamiał ten fragment
przy pierwszej klatce przez `Gothic::emitGlobalSoundWav`, niezależnie od odtwarzacza
i głośności muzyki. Pauzowanie DirectMusic nie mogło zatrzymać tego efektu.
Poprawka pomija startowy WAV, gdy aktywne są definicje nagranej muzyki CoM;
pozostałe profile zachowują dotychczasowy dźwięk startowy. To ustalona przyczyna,
a nie przypuszczenie o kolejności VDF lub niewłaściwym `02.OGG`.

Diagnostyka jest opcjonalna: `OPENGOTHIC_TRACE_AUDIO=1` lub lokalny plik
`audio-trace.enabled` w katalogu roboczym zapisuje zdarzenia do `audio-sources.log`.
Po zakończeniu weryfikacji należy usunąć lokalny plik włączający śledzenie.

1. Po zamknięciu gry i nowym buildzie uruchom ją od nowa. Zostań w menu głównym:
   ma grać motyw Kronik. Otwórz ustawienia i wróć. Po zamknięciu gry w `log.txt`
   powinien być wpis `CoM music: playing _WORK/DATA/MUSIC/KM/02.OGG`.
2. Wczytaj istniejący zapis z ukończonym „W gorącej wodzie kąpany”. W dzienniku
   wybierz ukończone, następnie niezaliczone. Zadanie ma być w ukończonych.
3. W trialogu pomiń długą wypowiedź w trakcie głosu, także podczas ruchu kamery.
   Poprzedni głos ma zamilknąć natychmiast. Powtórz przy innym rozmówcy.
4. W Silbach odsłuchaj strefę, zmień porę dnia zwykłą rozgrywką/snem, wywołaj walkę,
   zakończ ją i sprawdź powrót muzyki. Sprawdź suwak, wyłączenie muzyki, zapis/wczytanie
   i powrót do menu. Zgłoś miejsce, porę dnia i słyszany motyw przy rozbieżności.
5. Sprawdź scenę korzystającą z AI_Function, pochodnię i kołczan; zapisz/wczytaj i
   powtórz wyposażanie. Zgłoś podwójny przedmiot, zniknięcie wyposażenia lub wyjątek VM.

EXE `RelWithDebInfo` zawiera około 616 MiB symboli debugowania z około 645 MiB
całości. Jest to informacja do diagnozy awarii, a nie osadzone światy/muzyka.
Wydanie dystrybucyjne może zachować symbole osobno.

### Zakres po ponownym przeglądzie raportów (24.10, kolejny pakiet)

Dalsze implementacje mają wynikać z wywołań DAT Kronik i braków wskazanych
w raportach. Liczby oznaczają statyczne miejsca wywołań, nie częstość w grze.
Nie zaliczać ogólnych, niewywoływanych funkcji bibliotecznych jako postępu CoM.

- `NpcEquipBestWeaponByType` / `oCNpc::EquipBestWeapon` (2 miejsca w DAT):
  wyposażanie najlepszej używalnej broni kategorii melee/ranged. Most hooka
  `0x745fa6` przekazuje NPC w ESI do `_FIXEQUIPBESTWEAPONS`. Zachowano
  warunki używania ekwipunku i zabezpieczenie przed ponownym zdejmowaniem.
  Próba ręczna: `comcheck bestweapon`, stojąc bez wyciągniętej broni.
- `APPLYHOUSEWALLTEXTURE` (13 miejsc, w tym `INIT_FEATURES_ALWAYS` oraz
  wybory ścian domu 0..11): natywna adaptacja używa nazw tekstur zapisanych
  w DAT i zapisanej zmiennej `HOUSE_CRAFT_WALL_CHANGETEXTURE`. Przekierowuje
  rzeczywiste materiały świata, odświeża deskryptory GPU i ray tracingu,
  obejmuje również później dodane siatki. Wariant 0 przywraca teksturę
  domyślną; INIT moda ponawia wybór po wczytaniu świata. Nie jest to pełna
  implementacja ogólnego API zCTexture/zCResource. Dotychczasowy zerowy
  handler `ZCTEXTURE__LOAD` pozostaje osobnym brakiem dla innych użytkowników
  tego API; nadpisanie konkretnej funkcji domu omija go z rzeczywistym efektem.
  Próba ręczna wewnątrz domu: `comcheck housewall status`, następnie warianty
  1, 2, 10, 11, 0 i `restore`. Sprawdzić też zapis/wczytanie wybranego wzoru.

Te najnowsze zmiany nie były uruchamiane ani testowane. Zgodnie z poleceniem
użytkownika kolejna kompilacja ma nastąpić po jego komunikacie STOP.
Muzyka menu jest potwierdzona przez użytkownika po poprawce GAMESTART.WAV:
„Tak, tylko Kroniki”. Usunięto lokalny plik włączający szczegółowy ślad audio.

### Kolejne funkcje potwierdzone jako potrzebne w Kronikach

- `ENABLELOADGAME`: rzeczywista blokada wczytania, sprawdzana przed usunięciem
  bieżącego świata, również dla quick-load. `ENABLELOADSAVEGAME` przełącza obie
  blokady; wcześniej zmieniała tylko zapis. Użycie: hooki kamery Kronik.
  `comcheck scenelock load|save|both|off` służy wyłącznie ręcznej próbie na
  zapisie roboczym. Na końcu zawsze `off`.
- `GetSlotItem` i `DropFromSlot` (`0x74a590`): rzeczywisty slot dłoni, broni
  schowanej, tarczy i przedmiotów animacji. Drop jest natychmiastowy, bez
  animacji wymaganej przez zwykłe wyrzucanie. Kroniki używają go dwa razy
  w `WEAPON_STATCHECK`, gdy zmieniają się wymagania broni. Usuwanie/przenoszenie
  ostatniej sztuki czyści także uchwyty slotów modelu, stanu i amunicji.
- `oCNpc::IsSpellActive` (`0x73d340`): rozpoznaje trwający, zatwierdzony efekt
  zaklęcia przypisany do jego ID, z uwzględnieniem końca czasu efektu.
  `EVENTSMANAGER_Q501` sprawdza `SPL_LIGHT` w ciemnym pokoju; posiadanie lub
  trzymanie runy nie spełnia warunku. `comcheck spellactive [SPL_LIGHT]`.
  Ogólna serializacja aktywnych efektów pozostaje ograniczeniem istniejącego
  systemu efektów; nie deklarować jej naprawy tą zmianą.
- Pochodnia / `OCNPC_REMOVEFROMSLOT`: `GetSlotItem` zwraca żywą instancję płonącej
  pochodni przypisaną do faktycznego modelu. `TORCH_HEROHOLDINGTORCH` i poprawka
  moda po wczytaniu mogą ją rozpoznać. `TORCH_UNEQUIP` gasi model i overlay,
  a oryginalny skrypt zwraca jedną zgaszoną pochodnię. Adapter nie dodaje drugiej
  ani nie wyrzuca kopii na ziemię. Hook chronionego ogłuszenia dostaje rzeczywisty
  uchwyt broni do ponownego wyposażenia. To obsługa ścieżek Kronik, nie kompletna
  implementacja wszystkich wariantów natywnego RemoveFromSlot.
  `comcheck torch status|stow`; sprawdzić również normalny klawisz pochodni,
  zapis/wczytanie i sceny ogłuszenia areny.

### CATINV, mapy i podglądy LeGo

`CATINV_SETCATEGORY` ma natywną adaptację: ustawia rzeczywisty stan skryptu,
UI pobiera maski z `INV_CAT_GROUPS`. Dotychczasowa heurystyka artefaktów pomijała
przedmioty z ITEM_KAT_MAGIC niebędące pierścieniem/amuletem. Własny ekwipunek
zachowuje kategorię; otwarcie handlu, skrzyni i przeszukiwania resetuje ją jak
oryginalne CATINV_OPEN. Iteratory UI filtrują rzeczywiste magazyny, bez zastępowania
ich atrapami list linked-list Gothica. To odpowiednik zachowania, nie emulacja
około 25 hooków i oryginalnych pul kontenerów. Nadal potrzebne próby przenoszenia,
ostatniej sztuki, przedmiotów aktywnych i obu układów UI.

- zCTexture_Load: rzeczywista tekstura UI, stabilny uchwyt i nazwa zCObject.
- DrawPolySimple: sprawdzone rozmiary/floaty, triangulacja i UV/obrót spriteów.
- _SPRITE_DORENDER: emitowany podczas zbierania nakładki, w kolejności priorytetów.
  Mapy i strzałka są potwierdzonymi wywołaniami CoM (SPRITEMAP_INIT: 17 miejsc).
- _RENDER_INIT: lista RenderItem i most GameRender do _RENDER_HOOK, bez starego
  błędu not implemented. Aktualny DAT używa tu surowych wskaźników.
- oCItem::Render: modele przez InventoryRenderer z osobnymi buforami klatek GPU.
- Itm_GetPtr: izolowane instancje przedmiotów do receptur i podglądów; bez
  przyznawania przedmiotów graczowi i tworzenia helpera w punkcie TOT.
- Nakładka zna faktyczny rozmiar okna, zdarzenie rozdzielczości ponawiane po zmianie.

Zastrzeżenia: gradienty kolorów i blend modes poza 1/2/3 nie są obsługiwane;
rotacja z argumentu natywnego oCItem::Render jest pomijana na rzecz orientacji
inwentarza przedmiotu. Nie oznacza to zakończenia całego craftingu.
Komendy ręczne i aktualny licznik: ARCHOLOS_PROGRESS_2026-10-24.md.

### SetWalkStopChasm: poprawione błędne oznaczenie w historycznym raporcie

0x688a70 jest oCNpc::SetWalkStopChasm(int), potwierdzone nagłówkiem
https://github.com/PhoenixTales/nyx-core/blob/main/ZenGin/Gothic_II_Addon/API/oNpc.h
oraz instrukcjami w oryginalnym zainstalowanym Gothic2.exe: ustawia bit 2 przy
GetHumanAI()+0xb8. Nowy handler steruje blokowaniem krawędzi w MoveAlgo. Kolizje,
grawitacja i skoki pozostają osobnymi mechanikami. Ustawienie trafia do opcjonalnego
wpisu NPC compat-movement (wersja 1), domyślnie true dla starych zapisów.
Ręczny odbiór: Q204 (próba Araxos) i Q304 (strażnicy pułapki).

### Zgłoszony błąd Q102: „Idę z Jornem” / niedokończony trialog

Dokładna opcja: CHOICE_FABIO_Q102_JORNTRIALOG_QUESTION_GOWITHJORN.
Komunikat „Trialog has not finished! It has to be ended with TRIA_Finish()”
pochodzi z ZS_TALK_END. Gałąź skryptu wywołuje TRIA_FINISH poprawnie, po czym
kolejkuje Q102_CHANGEROUTINE i AI_STOPPROCESSINFOS. W adapterze mówcy mają
oddzielne kolejki, więc te działania mogły wyprzedzić _TRIA_FINISH bohatera.
Nowy wrapper zachowuje oryginalną walidację, TRIA_WAIT, kamerę i końcowe wywołanie,
a następnie synchronizuje kolejki wszystkich uczestników po zakończeniu bohatera.
_TRIA_FINISH przywraca także AIV_INVINCIBLE zaproszonych NPC, jak w DAT.
Nie usuwa ani nie wycisza komunikatu kontrolnego. Implementacja czeka na build
po STOP i ponowienie tej samej naturalnej rozmowy przez użytkownika.

### Hook opisu receptury i kontrole zakresów

`OCITEMCONTAINER__DRAWITEMINFO_HOOK`, adres 0x706e40: most przekazuje wybrany
przedmiot w [ESP+4]. Natywny interfejs wywołuje go po zmianie zaznaczenia,
ponownym otwarciu i działaniach na przedmiocie, w układzie zwykłym i dotykowym.
Oryginalny skrypt uzupełnia statystyki wyniku i sufiks znanej receptury.
Tymczasowy C_RECIPE i jego tekst są zwalniane po skopiowaniu danych, a wiązania
instancji przywracane. Odbiór: znane/nieznane receptury kowalskie, alchemiczne
oraz kulinarne; opis ma odpowiadać wynikowi i odświeżyć się po nauce receptury.

GETBUFFERCRC32 nie dereferencjonuje nieprawidłowego regionu; QSORT_G2 odrzuca
przepełniony rozmiar tablicy. To kontrole bezpieczeństwa, nie pełna implementacja
arbitralnego komparatora qsort. Żadnych testów gry/VM nie wykonano.

### Podgląd receptury przy wyborze dialogowym

`CRAFTINGVIEW_UPDATE` jest adaptowane przez zdarzenie rzeczywistego menu wyboru,
bez fikcyjnego wskaźnika prywatnego widgetu Gothica. Reguły odczytano z kodu i DAT:
NR 10000–10399 wybiera odpowiednie tablice receptur, podwybory alchemii/zwojów/
gotowania przekazują ilość lub dokładnie dopasowane „Wszystko (maksimum)”,
a zbroje rozpoznaje oryginalne CRAFTINGVIEW_CHECKARMORS. CRAFTINGVIEW_SHOW/HIDE
nadal tworzą treść, tłumaczenia, model oraz zakładki oryginalnych skryptów.
Zatwierdzenie, brak aktywnego wyboru i zamknięcie rozmowy usuwają podgląd.
Tymczasowe C_RECIPE z SHOW i obliczania maksimum są sprzątane wraz z napisami;
wiązania SELF/OTHER/ITEM i instancji receptury są przywracane.

To alternatywa dla jednego callbacka pod 0x660bb0, nie pełna implementacja adresu
ani struktury InformationManagera. Drugi callback obsługujący wpisywanie tekstu
ma osobny natywny interfejs; statyczny brak ogólnego mostu nadal pozostaje jawny.
Odbiór ręczny: `comcheck crafting` oraz naturalne użycie stanowisk, wybór ilości,
zakładki i wyjście, potem wykonanie przedmiotu. Bez kompilacji i prób do STOP.

### Q102: automatyczne zamykanie pustego menu

Oprócz synchronizacji kolejek znaleziono niezależną ścieżkę przedwczesnego końca:
DialogMenu samo zamykało pustą listę, zanim kolejka bohatera wykonała _TRIA_FINISH.
Adapter oznacza teraz oczekujące zakończenie; automatyczne i AI_StopProcessInfos
czekają na rzeczywiste sprzątnięcie. Nie zeruje kontrolnej zmiennej na skróty.
Wybór nie daje się ponownie zatwierdzić podczas oczekiwania na kolejki.
OCNPCSTATES_CHANGEROUTINE_HOOK zawiera wyłącznie MEM_FATAL z komunikatem błędu
oryginalnego silnika; nie jest funkcją kończącą trialog i nie dodano pustego handlera.

### Przedmioty pod wodą i DoTakeVob

Kroniki rejestrują OCAIHUMAN__PC_DIVING_HOOK pod 0x699c60. Jego kontrakt to akcja
na przedmiocie w fokusie, T_WATERTAKEITEM, komunikat otrzymania oraz DoTakeVob.
Natywne sterowanie już rozpoznaje przedmioty podczas nurkowania, ale korzystało
z ziemnego S_IGET. Gdy hook Kronik jest zarejestrowany, używa teraz właściwej
animacji i komunikatu. Operacja usuwa rzeczywisty przedmiot świata i przenosi go
do ekwipunku z dotychczasowym zdarzeniem kradzieży. DoTakeVob 0x7449c0 obsługuje
bezpośredni wariant tej operacji bez drugiej animacji; zwraca powodzenie/porażkę,
odrzuca nieprzedmiotowy lub martwy uchwyt. Nie emuluje podnoszenia dowolnego voba.
Jest to natywna alternatywa dla wejściowego callbacka nurkowania, nie ogólny most
do wszystkich hooków AIHuman. Odbiór: przedmiot pod wodą trafia raz do ekwipunku,
znika ze świata, animacja kończy się i można dalej nurkować; kontrolnie zwykłe
podnoszenie na lądzie. Kompilacja i próby nadal dopiero po STOP.

### Slowdown: niepowiązany MAG_BOOK

SPELL_CAST_SLOWDOWN odwołuje się do OCNPC.MAG_BOOK, a następnie do [book+0x18]
i 0x475a50 (oCMag_Book::SetOwner). Niepowiązany member uniemożliwiał wykonanie
skryptu. Dodano dedykowany compat_magic_book w INpc i w zapisywanym patchu ZenKit,
oddzielnie od pól Daedalusa. Przed rzucaniem tworzona jest 64-bajtowa struktura
właściciela/świata per żywy NPC. Nie jest serializowana; martwe wpisy są zwalniane.
SetOwner sprawdza właściciela i ustawia pola +0x18 świat, +0x1c właściciel,
+0x20 model (natywny GetModel używa uchwytu NPC). Skrypt zaklęcia, mana, flagi,
efekt i opóźnione FF pozostają oryginalne.

To obsługa kontraktu odczytywanego przez Kroniki, nie pełne oCMag_Book. Osobno
wymaga sprawdzenia rzeczywiste spowolnienie ruchu/animacji/dźwięku, upływ 60 s,
odświeżenie efektu, zapis/wczytanie oraz Otis w Q306. Nie oznaczono całego
zaklęcia jako przetestowanego ani ukończonego. Brak nowych prób gry/VM.

### Kamera dialogowa i faza przed zapisem LeGo

DIACAM_UPDATE / StartDialogCam 0x4b20f0 przekazują żądanie do prawdziwej kamery
OpenGothic. Żądanie czeka na koniec kamery scenki, potem ustawia tryb rozmowy
i przelicza cel, odległość i kierunek dla aktualnego uczestnika. Zakończona
rozmowa, wyłączona kamera dialogu i tryb swobodny nie wymuszają zmiany kadru.
Ładowanie usuwa oczekujące żądanie. Odbiór razem z trialogami Q102, po cięciu
kamery i po pomijaniu kwestii; również normalna następna rozmowa.

TORCHLOADFIX_CHECKTORCH ma most zdarzenia 0x6c5250 przed zapisaniem świata
i zmiennych Daedalusa. Używa rzeczywistego uchwytu płonącej pochodni.
_BW_SAVEGAME (LeGo pod 0x439105) również przeniesiono do tej fazy: wcześniej
uruchamiało się po saveVar, więc zmiany globali callbacka nie trafiały do
tego samego zapisu. Końcowe dmaSave kopiuje gotowe pliki wirtualne do archiwum
bez ponownego wywołania hooków. Nie zmienia to formatu istniejących wpisów.
Odbiór: zapis z pochodnią, zapis po jej schowaniu, ponowne uruchomienie procesu
i wczytanie obu; liczba pochodni i model bez duplikatów. Dodatkowo trwałość
obiektów PermMem i FrameFunctions po ponownym uruchomieniu.

### Ustalony kontrakt lokalnego czasu modelu (stan przed kolejnym pakietem)

Z KM_Scripts.mod odczytano VISUALFX.DAT (199575 bajtów). SLOW_TIME_SPELL:
visName_S="time.slw", userString[0]="1.0", userString[1]="0.5", czas 60 s.
Przed kolejnym pakietem GlobalEffects::addSlowTime obsługiwał tylko pierwszy
parametr. Był to rzeczywisty brak niezależny od naprawionego MAG_BOOK.

Instrukcje zainstalowanego oryginału, oCVisualFX::SetVisualByString:
0x48d01f–0x48d024 parsuje userString[0] i zapisuje globalny skaler 0x99b3d4;
0x48d060–0x48d092 parsuje userString[1], dzieli przez globalny skaler i zapisuje
wynik do origin->visual (model) +0x1fc. To skala modelu postaci pochodzenia
efektu, a nie bezwarunkowe spowolnienie gracza. Układ pola timeScale potwierdza
[nagłówek zCModel](https://raw.githubusercontent.com/PhoenixTales/nyx-core/main/ZenGin/Gothic_II_Addon/API/zModel.h).
Kontrakt wymaga spójnego zegara animacji/root motion/zdarzeń postaci
i przywrócenia po końcu efektu oraz po zapisie/wczytaniu. Nie dodano arbitralnego
multSpeed(0.5), który zmieniłby sam ruch bez animacji i okien trafień.

### Wybór przeciwnika i pola wyglądu NPC

Hook 0x734f4d dostaje rzeczywiste uchwyty: EBP wybierającego NPC i EDI kandydata.
ECX != 0 odrzuca kandydata, zgodnie z testem oryginalnego EXE pod 0x734f7b.
Adapter działa także dla zachowanego celu, po natywnej kontroli wrogości i zmysłów.
Callback Kronik zachowuje reguły drużyny i widoczności; odbiór w naturalnej walce.

BODY_VISUALNAME/HEAD_VISUALNAME mają własne ciągi w INpc. BITFIELD ma osobną tablicę
zamiast błędnego aliasu C_NPC.MISSION. setVisualBody uaktualnia nazwy i maski
tekstur ciała 0x3fffc000, głowy 0xffff0000 i zębów 0x0000ffff. To umożliwia
NPC_CHANGEFACE/B_REFRESHATINSERT; pozostałe flagi silnika nadal wymagają przeglądu.
TELEPORTHEROTOPOS odczytuje bity float, sprawdza skończoność i zmienia realną
pozycję/kolizję bohatera; nie obiecuje ogólnego zapisu dowolnej macierzy voba.

### BUFF_DIVE10_TICK i zapas powietrza

Oryginalny callback używa BUFF_GETNPC, następnie zapisuje niepowiązane DIVETIME
i DIVECTR. Adapter zachowuje lookup LeGo i ustawia rzeczywisty limit 30000 oraz
pozostały zapas 31000 ms. W oryginalnym EXE 0x73e5da–0x73e5f0 odejmuje czas od
DIVECTR pod wodą, a 0x73e6d5–0x73e70f uzupełnia go dwukrotnie szybciej poza wodą
do DIVETIME. Natywny licznik odtwarza tę semantykę, a HUD i obrażenia używają
tego samego limitu. Oddzielny, wersjonowany compat-breath zapisuje oba pola;
stare zapisy bez wpisu zachowują dotychczasowe liczenie z wartości gildii.
Nie zmienia zegara ruchu/animacji nurkowania. Odbiór: eliksir, wygaśnięcie,
ponowne nurkowanie, zapis/wczytanie pod wodą, kontrolnie brak buffa.

### Pierścień maga i descriptor obrażeń

Wejściowy descriptor otrzymuje ARYDAMAGE, sumę FDAMAGETOTAL, mnożnik 1.0,
maskę obrażeń, dane zaklęcia i uchwyt trafionego. Zmodyfikowane ARYDAMAGE
wraca do obliczenia czaru, również pocisku, przed ochroną/odpornością i maską
kolizji. Usunięto dodawanie FDAMAGETOTAL bezpośrednio do utraty HP: Kroniki
zwiększają jednocześnie total i MAGIC w tablicy, więc poprzedni adapter omijał
ochronę. Po ONDAMAGESCRIPT ponownie odczytywane jest dontKill. Penetracja pancerza
nadal obejmuje obliczenie trafienia pomiędzy oryginalnymi callbackami obniżania
i przywracania ochrony. Nie jest to implementacja wszystkich wariantów dowolnego
oSDamageDescriptor; wymagane ręczne porównania magii, strzał i walk bez zabijania.

### Zmiana zasad kompilacji przez użytkownika

Użytkownik zezwolił na okresowe kompilowanie przed STOP. Wcześniejsze zdania
tego raportu odraczające każdy build do STOP są historyczne. Zakaz uruchamiania
gry/testów/comcheck przez agenta pozostaje. Pierwszy build wykrył i pozwolił
poprawić konflikt makra string w nowych polach ZenKit; wynik końcowy będzie
zapisany po zakończeniu kompilacji.

### Cel zaklęcia otwierania i trwały postęp zamka

TARGET_TYPE_MOB=128 przechodzi przez rzeczywisty most 0x485fc4: ECX typ,
[ESP+4] uchwyt interaktywnego obiektu; ECX=1 pozwala wybrać cel. Oryginalny
SPELL_PICKLOCK_FOCUS rozpoznaje klasę i zamknięcie. Wcześniej wyszukiwanie
interaktywnych obiektów zawsze używało TARGET_TYPE_ALL, więc mogło wybrać
otwartą skrzynię lub stanowisko zamiast zamka. Zwykłe wyszukiwanie poza tym
rozszerzeniem zachowuje dotychczasową politykę.

Odgadnięte znaki z BITFIELD nie są już przechowywane w rotującym mobPick[256].
Należą do Interactive i są ograniczone długością kodu oraz maską bitową.
Wersjonowany opcjonalny compat-lock zapisuje je przy skrzyni/drzwiach, także
przy zmianie świata. Odczyt locked używa rzeczywistego stanu zamka, co pozwala
oryginalnemu skryptowi rozpoznać również zamknięcie kluczem bez kodu i odmówić
otwarcia. Stare zapisy domyślnie zaczynają z postępem zero. Odbiór naturalnie
zwojem: cel, przerywanie, zapis/wczytanie, koszt i zużycie tylko po powodzeniu.

### QSORT i rozdzielenie FINAL od destruktorów

Domyślny komparator Gothic2.exe 0x553ca0 porównuje unsigned DWORD spod dwóch
adresów (cmp/jbe), nie signed int. QSORT_G2 obsługuje teraz pełny stride rekordu,
w tym 8-bajtowe pary offset/symbol MEMINT_BuildFuncStartsArray, sortując po
pierwszym polu bez rozdzielania par. Przepełnienie rozmiaru i nieznany komparator
są jawnie odrzucane. Nie oznacza to obsługi dowolnego komparatora x86.

LeGo/Locals.d i bytecode pokazują, że FINAL przepisuje blok if na dodatkową
funkcję w pamięci parsera. Nie jest mechanizmem destruktorów PermMem i ma zero
bezpośrednich wywołań w badanym DAT. Pozostał w raporcie z tą informacją;
destruktory w DELETE są oddzielnym zakresem do odbioru.

### Zegar modelu time.slw — implementacja

Pose przechowuje segmenty zegara świata/modelu ze skalą. Początki warstw,
przejścia, sampling/blending, root motion, SFX/PFX, znaczniki animacji,
okna obrony i combo używają tego samego modelowego czasu. Zewnętrzny barrier
zdarzeń nadal jest czasem świata, przeliczanym przy każdym odczycie. Osobne
wewnętrzne startAnim/stopItemStateAnim zapobiegają podwójnemu przeliczeniu.
Czasy oczekiwania pełnej animacji i ataku przeliczane są na czas świata.

Globalny time.slw zachowuje skaler świata, a local/global wskazanej przez
efekt postaci trafia do zegara modelu; właściciel jest weak INpc, nie surowym
wskaźnikiem. Npc::startEffect ustawia brakujący origin dla Wld_PlayEffect.
Ostatni aktywny efekt ustawia skalę modelu, bez mnożenia spowolnienia tego
samego modelu przy kolejnych rzutach; wygaśnięcie/StopEffect przywraca 1.0.
Wartości NaN/Inf/nieprawidłowe skale nie trafiają do konwersji na licznik.

Opcjonalne compat-model-clock NPC i compat-time-effects świata zapisują zegar,
właściciela oraz koniec/lifetime/remainder aktywnych time.slw. Stare zapisy
bez wpisów zachowują tempo 1.0. Nie dodano żadnego uruchomionego testu/gry.
Odbiór: naturalne zaklęcie, Otis Q306, ruch i walka/zdarzenia animacji,
ponowne rzucenie, końcowe 1.0, load w trakcie, zniknięcie źródła i zmiana
świata. Synchronizacja co-op lokalnych zegarów nie jest potwierdzona.

### Światło Q501 po wczytaniu

Aktywny SPL_LIGHT ma wersjonowany opcjonalny compat-light-spell przy NPC.
Zapisywany jest tylko faktycznie aktywny efekt i pozostały czas. Wczytanie
odtwarza światło z definicji tego samego SPL_LIGHT, ustawia znacznik aktywnego
zaklęcia i oryginalny pozostały czas bez skryptu rzucania/kosztu many/zwoju.
Nie wydłuża czasu do pełnego przy każdym loadzie. Nie jest to ogólna
serializacja wszystkich zaklęć ani poszczególnych cząstek/dźwięków efektu.
Odbiór: światło i warunek IsSpellActive w Q501, wygaśnięcie, load i restart.

### Kolizje obiektów interaktywnych i bufor pozycji

Interactive::setVobVisual ignorowało collideDynamic, więc istniejący override
CHANGEVOBCOLLISION mógł zmienić wizualizację bez odblokowania przejścia.
Przekazuje teraz zmianę do ObjVisual, a odtworzona fizyka dostaje właściciela
Interactive i aktualną pozę. Sceny CoM przełączają flagi razem; pełna oddzielna
semantyka staticVob/cdStatic nadal nie jest emulowana. Odbiór naturalnych
zmian drzwi/przeszkód, zapis/wczytanie, ponowne zablokowanie.

SetPositionWorld wcześniej sprawdzało cztery bajty, po czym kopiowało dwanaście.
Sprawdza teraz cały vec3, a oba wejścia zapisu pozycji (native i surowa macierz
uchwytu) odrzucają niefinitywne współrzędne przed przekazaniem do fizyki.

### Kompilacja po pierwszym pakiecie

build-touch/archolos-compat-build-2026-10-24.log zakończył się kodem 0 i linkiem
Gothic2Notr.exe oraz Spacer.exe. Poprawiono konflikt makra string w ZenKit,
jawne konwersje wymiarów tekstury LeGo oraz const dostęp do świata przy tlenie.
Kolejny pakiet zegara/światła/kolizji kompilowany jest osobno; to nie jest wynik
jego końcowego odbioru. Agent nie uruchamiał gry ani testów.

### Nastawienia tymczasowe i ścieżki walk questowych

Npc_SetTempAttitude miało 84 statyczne miejsca wywołań w 48 funkcjach CoM,
m.in. EventsManager_Q205/Q306/Q602, zakończenia efektów magii i przywracanie
postaci. Wartość była zapisywana, ale personAttitude/GetAttitude ją ignorowały.
Odczyt i korzystające z niego decyzje AI uwzględniają teraz tmpAttitude przed
permanentnym/gildyjnym; GetPermAttitude odczytuje osobno stałą relację. Nie
resetuje się tymczasowej wartości przy zmianie stałej: oryginalne SetAttitude
i SetTmpAttitude zapisują oddzielne pola.

Kontrakt potwierdzono statycznie w Gothic2.exe: GetAttitude 0x72fab0 odczytuje
0x7e8 przed 0x7e4 dla relacji do gracza; GetPermAttitude 0x72fb30 czyta 0x7e4;
settery pod 0x72fb80 i 0x72fbd0 zapisują je oddzielnie. Adresy zweryfikowano
z [nagłówkiem oCNpc](https://raw.githubusercontent.com/PhoenixTales/nyx-core/main/ZenGin/Gothic_II_Addon/API/oNpc.h).
Nie zmieniono dotychczasowej symetrii relacji gracz/NPC w OpenGothic ani relacji
dwóch NPC według gildii. Odbiór: walka questowa i uspokojenie, pomoc drużyny,
zapis/wczytanie. comcheck attitude odczytuje bieżące i stałe nastawienie.

### Bezpieczne odwiązanie usuwanego przedmiotu

onWldInstanceRemoved sprawdza symbol i tożsamość jego bieżącej instancji.
Usunięcie starszego obiektu tej samej instancji skryptowej nie kasuje już
powiązania nowszego, a brak symbolu dla instancji przejściowej nie dereferencjuje
nullptr. Dotyczy również skryptowych podniesień/spawnów wielu sztuk w Kronikach.

### Kolejne kompilacje i narzędzia odczytu

archolos-time-build, archolos-light-build i archolos-diagnostics-build-2026-10-24
zakończyły się kodem 0 i linkiem obu EXE. Ręczne comcheck breath/slowdown odczytują
tlen i żądaną skalę modelu, również dla wskazanego NPC. Nie ustawiają buffów ani
questów. Nowy getter JS compatibilityState jest tylko do odczytu. Agent nie
uruchamiał tych komend. Pakiet nastawień i odwiązania skompilował oba EXE.

### Napisy, timer, walidacja i ScreenFX

memory_instance przechowuje odczytany tekst osobno dla adresu. Wcześniejszy
wspólny static string unieważniał argumenty string_view wieloparametrowego
wywołania. MEM_ReplaceFunc sprawdza symbol przed odczytem nazwy; Cpu32 czyści
wynik także przy pustym buforze. TeleportNpcToWP i findNpc sprawdzają istnienie
symbolu, klasę i żywego właściciela przed dostępem do NPC.

Zegar zTimer i bieżący mob są aktualizowane przed FF_Hook. Zapisy callbacków
do WORLDTIME stosuje oddzielna synchronizacja bez ponownego dodawania dt.
Nie zmienia to implementacji Timer/TimerGT, które mają własną ścieżkę LeGo.
ScreenFX indeksuje tekstury modulo frames.size(), wcześniej modulo FPS.

Statyczny przegląd 134 bezpośrednich WLD_PLAYEFFECT w aktualnym DAT wykazał
134 zestawy zerowych effectLevel/damage/damageType/projectile. Niezerowe
warianty TODO nie są obecnie wykazanym wymaganiem tych miejsc wywołania.
Tabela: archolos-audit/2026-10-24/playeffect-parameters.csv.

### Telekineza CoM

SPELL_LOGIC_TELEKINESIS i NEW_TELEKINESIS pracują na żywym IItem z fokusu,
sprawdzają przynależność do świata oraz dystans i przesuwają realny przedmiot.
Zachowują fazy, kroki i koszt many skryptu. Przy pierwszym ruchu wyłączają
fizykę i podnoszą przedmiot o wysokość bryły; cast/focus remover/przerwanie,
zapis i zmiana świata przywracają fizykę. Nie obiecują kontynuowania tej samej
fazy rzucania przez load. Weak referencje nie utrzymują usuniętego obiektu gry.

GameScript::invokeSpell używa invoke_function, aby wykonać również adapter
SPELL_CAST_TELEKINESIS. Poprzednie call_function omijało nadpisania VM.
Podpis funkcji i typ opcjonalnego argumentu nadal są sprawdzane.
comcheck telekinesis tylko odczytuje stan i podaje instrukcję ręcznej próby.

### Klawiatura Ikarusa i LeGo

MainWindow przekazywało packed INI code (np. A=0x1e00) do bitset<256>, więc
zwykłe klawisze nie dochodziły do dmaKeyState ani dmaKey. Osobna konwersja
scriptKeyCode podaje rzeczywiste skany DirectInput, w tym cyfry, Escape,
Enter, F1–F15 i przyciski myszy. Tablica wciśnięć mieści również mysz 524–528.
ClearKeyBuffer zeruje tablicę MEM_KeyPressed. Utrata fokusu/clearInput oddaje
puszczenia wszystkich klawiszy. Format ustawień i mapowanie sterowania gracza
pozostają oddzielne od kodów wymaganych przez skrypty.

MEM_DeleteVob ma już adapter dekoracji kołczanu; puste RemoveVob nie stanowi
braku na tej zastąpionej ścieżce. Pozostała ogólna obsługa vobów i logicznych
bindingów klawiatury wymaga oddzielnego uzupełnienia.

### Crafting: panel, model, kolory i dynamiczny komparator

Zrzut użytkownika pokazywał brak DLG_CHOICE.TGA i modelu oraz nagłówki wśród
wierszy składników. BAR_GetInterfaceScaling dzieli wirtualną szerokość HPBAR
przez szerokość 180 px. Fasada HPBAR miała szerokość zero, więc oryginalny
CRAFTINGVIEW_SHOW wyliczał zerowe rozmiary panelu i modelu. Fasada ma teraz
rzeczywistą niezerową szerokość, aktualizowaną według viewportu, i skaluje panel
800x200 proporcjonalnie tak, by mieścił się na ekranie.

Osobny błąd z logu: RENDERITEM.PRIORITY bez kontekstu w LIST_INSERTSORTED.
LIST_COMPARE wywołuje MEM_Call(compare), pozostawiając dwa argumenty poniżej
referencji do funkcji. Skryptowy void MEM_Call usuwał wynik przy pop_call VM.
MEM_Call i MEM_GetFuncID mają teraz adaptery naked; rozwiązują również łańcuch
zmiennych func i zachowują wynik komparatora. MEM_PushIntParam/StringParam/
InstParam zachowują przygotowane argumenty, zamiast tracić je przy powrocie
z wrappera void. W DAT: odpowiednio 25/38/19/7/10 statycznych miejsc wywołania.

invoke_function w patchu ZenKit traktuje nadpisania tak samo jak opcode BL:
adapter naked nie dostaje dodatkowej ramki sprzątającej, a zwykły adapter
posiada własną ramkę. Nadal obsługuje osobno ramki externali.

Rysowanie tekstów używa przekazanego koloru i alpha (tinted GthFont). Pomiar
szerokości/wysokości czcionki oraz rysowanie mają wspólną skalę. Zmiana rozmiaru
okna odświeża aktywną recepturę; bezpośrednia zmiana zakładki również sprząta
tymczasowy C_RECIPE. Ilości nadal liczy oryginalny skrypt z ekwipunku, bez
podstawiania liczb ze zrzutu referencyjnego.

archolos-crafting-build-2026-10-24.log zakończył się kodem 0 i linkiem obu EXE.
Agent nie uruchamiał gry ani testów. Użytkownik potwierdził poprawny wygląd:
tło, model i rozdzielone kolorowe składniki. Pełne wytwarzanie, koszty,
zakładki, ilości/Wszystko, wyjście i resize nadal pozostają do odbioru.

### Mosty menu, HUD i napisów scenek CoM

CutsceneMenu 0x42abe7 wywołuje oryginalny callback z wynikiem w EAX. Jawna
blokada scenki/plansz końcowych ma pierwszeństwo przed awaryjnym limitem dwóch
minut, używanym dotąd dla heurystyki. CutsceneStatus 0x6c31d6 steruje widocznością
fokusu i pasków postaci, również przy ISINFINALBOARDS.

DialogMessage 0x7a9240 dostaje rzeczywiste pola kontraktu: ECX jest tożsamością
widoku dialogu, ESP+4 wskazuje napis nazwy, ESP+16 wskaźnik koloru. Oddzielne
fasady ARRAY_VIEW[1] i [5] pozwalają callbackowi rozróżnić i zmienić widok.
DialogMenu stosuje zmienioną nazwę/kolor oraz przenosi pole na dół przy zmianie
widoku. Dzięki temu oryginalny callback CoM może usunąć nazwę i wybrać żółty
kolor na planszach końcowych. To nie pełna emulacja dowolnego widgetu zCView.

Pełny pakiet archolos-input-cutscenes-build-2026-10-24.log skompilował oba
EXE kodem 0. Nowe mosty nie mają jeszcze odbioru w grze.

### Kolejne bezpośrednie operacje Ikarusa

MEM_CallByString ma naked adapter, zachowujący argumenty i wynik funkcji tak
samo jak MEM_Call. MEM_ReadByte i MEM_WriteByte używają operacji jednobajtowej,
bez czterobajtowego odczytu poza końcem bufora; zapis synchronizuje ownedStrings.
MEM_CompareBytes porównuje dokładny zakres, obsługuje granice sąsiednich regionów,
pusty zakres, odrzuca ujemny rozmiar/null/overflow/niezmapowane fragmenty i kopiuje
pierwszy fragment przed callbackiem odczytu drugiego. Pakiet skompilowany kodem 0.

### Logiczne przypisania wejścia i __ftol

MEM_GetKey i MEM_GetSecondaryKey odczytują oba przypisania z bieżących ustawień
KEYS, konwertują kody do skanów Ikarusa i zwracają 0 dla nieprzypisanej opcji.
GetState 0x4d4d30 zwraca float dla logicznego ZOPT_GAMEKEY_WEAPON=8, jedynego
identyfikatora używanego tutaj przez CoM. IsBindedToggled 0x4cc470 sprawdza
przypisanie i konsumuje pojedyncze zbocze wciśnięcia. BindKeys czyści zbocza;
ClearKeyBuffer i utrata fokusu nie pozostawiają wciśnięć. Pozostałe logiczne
akcje nie są ogólnie emulowane; nieznane żądanie jest raportowane.

__ftol 0x7d0f48, wywoływany po GetState, ma kontrakt sprawdzony w oryginalnym
EXE: obcięcie ST0 do signed int64, wynik EDX:EAX, wartość nieokreślona x87
przy NaN/Inf/przekroczeniu zakresu. To nie pełny wieloelementowy stos FPU.
Pakiet archolos-input-cutscenes-build-2026-10-24.log skompilował oba EXE.

### STOPALLSOUNDS: osobny kanał głosu dialogu

Native RemoveAllActiveSounds 0x4ed7a0 zatrzymywało dźwięki WorldSound i globalne,
ale głos DialogMenu należał do osobnego SoundDevice. Dodano sygnał zatrzymania
currentSnd również w DialogMenu. Nie zeruje czasu tekstu ani barier AI i nie
zatrzymuje muzyki. Pełny archolos-voices-build-2026-10-24.log zakończył się
kodem 0. Odbiór w scence pozostaje do wykonania.

### MEM_SetKeys i domyślne skróty pochodni

Usunięto pusty override MEM_SETKEYS. INIT_TORCHKEY w tym DAT wywołuje go,
gdy nie istnieje KEYS.keyTorch; zapisuje KEY_T=20 i KEY_COMMA=51. Adapter
koduje dwa uint16 little endian jako osiem znaków hex, zapisuje bieżące
ustawienia i odświeża natywne przypisania przez onSettingsChanged; zeruje
stare zbocza. MEM_SetKey i MEM_SetSecondaryKey mogą używać tego samego
adaptera bez utraty drugiego przypisania. Nie nadpisuje istniejącej opcji:
warunek pierwszego uruchomienia nadal wykonuje skrypt INIT_TORCHKEY.

Pozostała rzeczywista atrapa: WLD_ISRAINING zwraca false. Pięć miejsc DAT:
B_SAY_GUILDGREETINGS, dwa C_CANNPCCOLLIDEWITHSPELL, DIA_OLDMAN_GOODWATER_CONDITION
i EVENTSMANAGER_NS06. Brakuje wspólnego systemu pogody/renderowania deszczu;
nie dodano losowej wartości odłączonej od świata. NPC staruszka i jego dialog
wymagają suchości, więc false nie blokuje tych dwóch warunków; brak wpływa
na warianty powitań i warunki zaklęć. Pozostaje jawnie niezaimplementowany.

### Kolejkowany _AI_TeleportAlways — weryfikacja callbacków

AI_TELEPORTALWAYS ma 95 miejsc BL w 53 funkcjach DAT, m.in. Q102_JornDistanceLimit,
Q602, KQ402, QA304 i zmianach rozdziału. Najpierw kolejkuje standardowe AI_Teleport,
a następnie _AI_TeleportAlways przez AI_Function_IS. Rozszerzony analizator
potwierdza tę krawędź mimo braku bezpośredniego BL do callbacka.

Przegląd początkowo błędnie uznał callback za brakujący. Adapter istniał już
w setupQuiverFunctions w directmemory_events.cpp: rozwiązuje rzeczywistego NPC
oraz punkt, czyści GoTo, ustawia pozycję przy ziemi i kierunek. Usunięto dodany
przez pomyłkę drugi override; nie ma nowej implementacji teleportu w tym pakiecie.
Ta ścieżka nie korzysta z ogólnego WLD_GetWaypointByName ani SetColl. Sam wynik
statycznego przeglądu nie zastępuje odbioru scen w grze.

### ENABLEMUSICZONE, enabled i priorytety ZEN

EVENTSMANAGER_INEXTREMOCONCERT dwa razy wywołuje ENABLEMUSICZONE dla HARBOUR_INX.
Oryginał szukał strefy jako voba, ale wyszukiwarka zgodności miała tylko movery.
Adapter zmienia rzeczywisty stan WorldSound. Strefy wczytują enabled/local_enabled,
priority i ellipsoid z ZEN; wybór rozpatruje nakładanie się stref przy każdym
odświeżeniu, zachowując bieżącą tylko przy remisie priorytetu. Wcześniej wejście
w strefę o wyższym priorytecie mogło nie zmienić muzyki aż do wyjścia z poprzedniej.

Zmiana enabled wymusza odświeżenie, a stan jest zapisywany w opcjonalnym wpisie
worlds/<świat>/music-zones, także przy zmianie świata. Stare zapisy korzystają
z wartości ZEN. Przy braku aktywnej strefy cichnie ścieżka strefy, bez zmiany
ustawienia musicEnabled i bez zatrzymania jawnego MusicSystem_OverrideTrack.
Nie emuluje reverb/volumeLevel ani wejściowych motywów day/nightEntrance.
Pakiet archolos-teleport-zones-build oraz archolos-zones-final-build z datą
2026-10-24 skompilowały oba EXE. Odbiór koncertu i stref w świecie nadal wymagany.

### Callbacki w analizatorze

Nowy callback-references.csv rozpoznaje stałe cele AI_Function, FF_Apply,
CallAfterWorldLoaded, MEM_Call/ByID/ByString, gdy argumenty są jednoznacznym
ciągiem push. Wynik tego DAT: 2529 miejsc, 2118 rozwiązanych, 411 dynamicznych.
Są oddzielne od 240563 BL/BE; nie zmieniają licznika bezpośrednich wywołań.
Nie oznaczają częstotliwości wykonania ani pełnej analizy przepływu sterowania.
Pozwoliło to potwierdzić istniejący adapter _AI_TeleportAlways i usunąć duplikat.
Nie stwierdzono brakującego externala w rozwiązanych celach callbacków.

Końcowy archolos-checkpoint2-build-2026-10-24.log zakończył się kodem 0; oba EXE
zlinkowane, dołączono comcheck cutscene i musiczone. git diff --check oraz
odwrotna walidacja obu patchy bibliotek przeszły. Bez uruchamiania gry/testów.

### WLD_SendUntrigger dla dźwięków ZEN

WorldSound::execTriggerEvent zawsze ustawiał active=true, także dla Untrigger.
Kroniki wysyłają WLD_SENDUNTRIGGER m.in. do Q101_SND_SMALLTALK (DIA_JORN_EXIT),
Q101_SND_SCARED (Q101_SHIP_FINAL) i SQ125_SAWSOUND. Teraz Trigger uruchamia,
a Untrigger wyłącza bieżący kanał i przyszłe wznowienia. Zatrzymanie obejmuje
współdzielony slot efektu, a nie tylko referencję WSound.current. Odrzucono
traktowanie pozostałych typów zdarzeń jako polecenia odtworzenia.

Stan active i pozostałe opóźnienie losowych dźwięków ZEN zapisują się w opcjonalnym
wpisie worlds/<świat>/sound-zones. Odtwarzanie sprawdza indeks i nazwę z ZEN,
żeby nie zastosować starego wpisu do innego dźwięku po zmianie pliku świata.
Stary zapis bez tego wpisu zachowuje stan początkowy z ZEN. Nie jest to zapis
pozycji próbek w odtwarzanej kwestii. archolos-soundtriggers-build z datą
2026-10-24 zakończył się kodem 0, oba EXE. Bez uruchamiania gry.

Normalizacja stałych unary (w szczególności -(1)) zwiększyła rozpoznanie celów
callbacków do 2490/2529, pozostawiając 39 dynamicznych. Licznik BL/BE bez zmian.

### NPC_CHANGEARMOR: pominięty stan skryptowy i NO_ARMOR

Oryginał w DAT kończy się zapisem SLF.AIVAR[48]=ARMOR. Dotychczasowy adapter
zmieniał tylko wyposażenie i pomijał ten zapis, także dla braku żywego obiektu
NPC. Wykonuje teraz końcowe przypisanie do przekazanej instancji, jeśli istnieje.
88 miejsc wywołania w 47 funkcjach DAT korzysta z NPC_CHANGEARMOR.

NO_ARMOR=-1 z QA306_REMOVENPC dla Bloodwyna usuwa bieżącą zbroję zamiast
wracać bez zmiany. Dodatnia wartość używa idempotentnego equipItem, zamiast
useItem przełączającego stan już założonej zbroi. Analogiczna poprawka dotyczy
MDL_SETVISUALBODY. Wartość 0 pozostawia wyposażenie — taki argument przekazuje
NPC_CHANGEFACE w autorytatywnym DAT. Adapter ChangeArmor nadal zachowuje
bieżące ciało/głowę, a zapis/wczytanie AIVAR korzysta ze zwykłej serializacji VM.
Pakiet archolos-armor-spelllevel-build-2026-10-24.log zakończył się kodem 0,
oba EXE. Naturalny QA306 i zmiany munduru wymagają odbioru.

Kontrakt NO_ARMOR potwierdzają DAT i referencja API:
https://auronen.cokoliv.eu/gmc/zengin/scripts/externals/mdl/#mdl_setvisualbody

### Poziom wywołania SPELL_CAST i zakończenie na STOP

invokeSpell przekazuje activeSpellLevel zamiast stałego 0. Getter uwzględnia
CS_Emit_0..CS_Emit_Last, tak samo jak istniejący wzór poziomu obrażeń pocisku.
W tym DAT osiem SPELL_CAST przyjmuje jeden parametr int, ale nie odczytuje go
po przypisaniu. Nie stwierdzono na tej podstawie naprawy obrażeń konkretnego
zaklęcia CoM. Obsługa bezparametrowych funkcji i nadpisań pozostaje zachowana.

Na polecenie STOP zatrzymano nowe implementacje. Rozpoznanie MDL_ApplyRandomAni/
Freq pozostaje analizą: nie dodano kodu; funkcje nadal są atrapami. WLD_IsRaining
również nadal zwraca false. Odbiór kampanii oraz pozostałe hooki/adresy nie są
zakończone. Końcowy build i commit/push obejmują cały bieżący kod oraz raporty.

archolos-stop-build-2026-10-24.log: kod 0, ninja: no work to do. Potwierdza
aktualność obu wcześniej zlinkowanych EXE; ostatni Gothic2Notr.exe z 06:19:47.
Końcowa kontrola diff i obu patchy przeszła.
