# Kroniki: bieżąca lista prac

Postęp: **24/27 pozycji**. Licznik dotyczy tej listy, nie procentu zgodności gry.
`[x]` oznacza implementację w kodzie. Użytkownik zezwolił na okresowe kompilacje
przed STOP; nowy pakiet nadal wymaga ręcznych prób. Agent nie uruchamia gry,
testów ani komend comcheck. Użytkownik potwierdził muzykę menu i poprawny wygląd
panelu craftingu po pakiecie naprawczym.

- [x] Muzyka menu Kronik; użytkownik potwierdził wyłącznie właściwy motyw.
- [x] OGG i mapowanie muzyki stref świata.
- [x] Priorytet plików moda w VDF/MOD.
- [x] Kategorie ukończonych i niezaliczonych zadań dziennika.
- [x] Zatrzymanie głosu pominiętej wypowiedzi trialogu.
- [x] STOPALLSOUNDS dla efektów świata, globalnych i głosu DialogMenu.
- [x] WLD_INSERTOBJECT dla obsługiwanych wizualizacji, z zapisem obiektów.
- [x] Equip/Unequip bez zdejmowania przy ponownym wyposażeniu.
- [x] NpcEquipBestWeaponByType i most hooka wyposażania NPC Kronik.
- [x] GetSlotItem rozróżnia dłonie, schowaną broń i przedmioty animacji.
- [x] DropFromSlot dla WEAPON_STATCHECK.
- [x] APPLYHOUSEWALLTEXTURE: rzeczywiste materiały domu i powiązania GPU.
- [x] ENABLELOADGAME: blokada ładowania, w tym quick-load.
- [x] ENABLELOADSAVEGAME: blokada obu operacji.
- [x] MEM_WriteString i pożyczany wskaźnik tekstu.
- [x] Wywołania Ikarusa respektują nadpisania VM.
- [x] Zabezpieczenia pamięci i uchwytów oraz sprzątanie slotów przedmiotów.
- [x] Analizator źródeł/DAT, licznik miejsc wywołania, raport atrap/TODO.
- [x] oCNpc::IsSpellActive, potrzebne dla światła w Q501.
- [x] Ścieżki RemoveFromSlot używane przez pochodnię i chronione ogłuszenie.
- [x] Przegląd CATINV i natywna adaptacja masek/stanu kategorii ekwipunku.
- [x] Sprite'y map LeGo, Render modeli i wybór podglądu receptury w craftingu.
- [x] Synchronizacja TRIA_FINISH przed zmianą rutyn/zamknięciem dialogu Q102.
- [ ] Pozostałe hooki Kronik: kontrakty zdarzeń, rejestrów i stosu.
- [ ] Pozostałe wywołania natywne/dynamiczne: implementacje i działające zamienniki.
- [x] Kompilacje pakietów i poprawki kompilatora; oba EXE zlinkowane.
- [ ] Ręczne próby zmian i weryfikacja kampanii.

## Nowe komendy, wyłącznie do uruchomienia przez użytkownika

Na zapisie roboczym. Źródło: `mods/devtools/lib/archoloschecks.js`; komendy są kopiowane
do EXE/kopii moda podczas kompilacji; ostatnio dodano cutscene.

- `comcheck bestweapon` — warunki używania i idempotencja wyposażania.
- `comcheck housewall 0..11|status|restore` — wzory ścian, w tym 11 świąteczny;
  wewnątrz domu sprawdzić też zapis/wczytanie wybranego wzoru.
- `comcheck scenelock load|save|both|off` — po próbie zawsze `off`.
- `comcheck cutscene` — tylko odczyt flag. W naturalnej scence sprawdzić blokadę
  menu również po dwóch minutach, brak HUD/fokusu, dolne pole kwestii NPC oraz
  nazwę/kolor przy planszach końcowych. Po scenie zwykłe zachowanie wraca.
- `comcheck spellactive [SPL_LIGHT]` — przed rzuceniem, w trakcie i po wygaśnięciu;
  sprawdzić naturalny warunek światła w ciemnym pokoju Q501.
- `comcheck torch status|stow` — płonący model, zwrot jednej zgaszonej pochodni,
  normalny klawisz oraz zapis/wczytanie.
- `comcheck category 0..8|status` — maski przedmiotów, zachowanie kategorii własnej,
  reset przy otwarciu handlu/skrzyni/przeszukiwania, przenoszenie ostatniej sztuki.
- `comcheck map archolos|silbach|city|sewers|off` — tekstura, kwadratowe proporcje,
  położenie i obrót kursora; dodatkowo naturalne użycie przedmiotu i mapy questowe.
- `comcheck preview [przedmiot]|off` — 3D przez Render_AddItemPrio; zamknąć przed
  zapisem/wczytaniem lub zmianą świata. Sprawdzić też naturalne okna craftingu.
- `comcheck crafting` — tylko odczyt stanu. Przy naturalnym stanowisku sprawdzić
  kilka receptur, ilości i „Wszystko”, składniki, wymagane/posiadane liczby,
  zakładkę właściwości, wyjście i ponowne wejście. Następnie normalne wytworzenie.
  Nowy pakiet: niezerowa skala panelu/modelu, DLG_CHOICE.TGA, kolory i odstępy
  tekstu, dynamiczny komparator listy LeGo. Skompilowany; użytkownik potwierdził
  wygląd. Pełne wytwarzanie, koszty, zakładki i resize nadal do odbioru.
- Pod wodą: naturalnie podnieść przedmiot. Ma zniknąć ze świata, trafić raz do
  ekwipunku i użyć wodnej animacji; po niej dalsze nurkowanie. Porównać z lądem.
- Spowolnienie: naturalne rzucenie ze zwoju i runy, właściwy koszt many, efekt,
  wygaśnięcie po 60 s i ponowne rzucenie; osobno scena Otisa w Q306. Sprawdzić
  wspólne tempo animacji/ruchu/ciosów oraz powrót do normy. Zapisać w trakcie,
  ponownie uruchomić proces, wczytać i zaczekać na pozostały czas efektu.
- LeGo/zapis: zapisać osobno z zapaloną i schowaną pochodnią, zamknąć proces
  i wczytać oba zapisy; model oraz liczba bez duplikatów. Sprawdzić również
  trwałość obiektów PermMem i FrameFunctions po ponownym uruchomieniu.
- Eliksir nurkowania: naturalnie wypić, nurkować dłużej niż zwykły zapas tlenu,
  sprawdzić pasek, brak obrażeń podczas buffa, wygaśnięcie i ponowne nurkowanie.
  Powtórzyć z zapisem/wczytaniem pod wodą; bez buffa zwykłe duszenie pozostaje.
- `comcheck breath [NPC_INSTANCE]` i `comcheck slowdown [NPC_INSTANCE]` — odczyt
  limitu/pozostałego powietrza, HP i żądanej skali modelu. Domyślnie bohater;
  przy Slowdown wskazać trafionego NPC, a w scenie Otisa obserwować bohatera.
- `comcheck attitude NPC_INSTANCE` — odczyt nastawienia bieżącego i stałego.
  Porównać przed, podczas i po naturalnej walce Q205/Q306/Q602, także po loadzie.
- `comcheck telekinesis` — odczyt fazy i kroków ruchu. Naturalnie przyciągnąć
  luźny przedmiot: koszt many, ruch, ukończenie i opadanie. Przerwać rzucanie,
  zmienić broń, zapisać i wczytać; przedmiot nie może pozostać bez fizyki.
- Kolejkowane teleporty: naturalne limity odległości Jorna/Ruperta w Q102,
  przygotowanie postaci w Q602 i manekiny domu; brak skoku do środka świata.
- `comcheck musiczone` — tylko odczyt strefy/świata i flag koncertu. Wejście do nakładających się stref, wyjście, dzień/noc, koncert
  In Extremo 18–21, zapis podczas aktywnego koncertu i powrót po zmianie świata.
- Klawiatura Ikarusa: mapę LeGo zamknąć Escape, Enter, PPM i przypisanym
  klawiszem mapy/ekwipunku. Przy zaklęciu zamka sprawdzić puszczenie akcji;
  po Alt+Tab klawisze nie pozostają wciśnięte dla skryptów. Sprawdzić skrót
  pochodni po zmianie przypisania, także drugie przypisanie, bez powtarzania
  przełączenia od trzymania klawisza.
- Obrażenia: ten sam czar z pierścieniem maga i bez, przeciwnicy z ochroną oraz
  odpornością na magię; broń penetrująca nie może trwale odejmować pancerza.
  Oddzielnie strzały i magiczna broń w walkach, które mają kończyć się ogłuszeniem.
- Drużyna: naturalna walka z towarzyszem, zmiana celu po pokonaniu przeciwnika
  i przeciwnik za przeszkodą; towarzysze nie wybierają siebie nawzajem.
- Otwieranie zamków: zwojem wybrać zamkniętą skrzynię lub drzwi; otwarty obiekt,
  ławka i ognisko nie mogą zasłaniać właściwego celu. Kluczowy zamek bez kodu ma
  odmówić otwarcia. Przerwać inwestowanie many, zapisać/wczytać i dokończyć ten
  sam zamek; odgadnięte znaki pozostają, zwój znika dopiero po powodzeniu.
- `comcheck trialog` — tylko odczyt. Q102: naturalnie wybrać „Idę z Jornem” oraz
  w oddzielnym przebiegu „Idę z Fabio”, normalnie i z pomijaniem kwestii. Po końcu
  TRIA_RUNNING=0, TRIA_CPTR=0, bez błędu ZS_TALK_END; kamera, rutyny, dziennik
  i możliwość następnej rozmowy mają wrócić do poprawnego stanu.

## Ograniczenia najnowszego pakietu

- Zmiana ścian jest adaptacją konkretnej funkcji CoM. Nie oznacza pełnej emulacji
  zCResource CacheIn/CacheOut/zmiany nazwy dla dowolnego moda.
- Sprite'y obsługują pozycję, obrót, UV, priorytet i jednolity kolor, jak mapy CoM.
  Gradienty kolorów wierzchołków i pozostałe blend modes są jawnie zgłaszane.
- Render modeli korzysta z InventoryRenderer. Nie emuluje dowolnego świata
  renderowania Gothica, dowolnej rotacji przekazanej do oCItem::Render ani całej
  semantyki nakładania wielu warstw 2D/3D.
- Zapis odtwarza konkretnie SPL_LIGHT i aktywne time.slw; nie jest to ogólna
  serializacja wszystkich efektów, faz cząstek i dźwięków.
- Nie potwierdzono pełnego craftingu, wszystkich questów ani braku critical error.
- Buildy: archolos-compat/time/light/diagnostics/attitude/memory/timers/screenfx/
  telekinesis-build z datą 2026-10-24 zakończyły się kodem 0. Pakiet klawiatury
  i archolos-input-cutscenes-build również skompilowały oba EXE. Wygląd craftingu
  potwierdził użytkownik; brak odbioru kolejnych zmian.
- `CRAFTINGVIEW_UPDATE` korzysta z natywnego wyboru menu. Nie oznacza to pełnej
  emulacji prywatnego InformationManagera ani wszystkich hooków pod 0x660bb0.
- Drugi parametr time.slw ma już kod zegara modelu i zapisu. Kompilacja/odbiór
  naturalnego zaklęcia i sceny Q306 pozostają warunkiem uznania działania w grze.

- Pakiet voices oraz torchbindings z 2026-10-24 skompilował oba EXE kodem 0.

- Strefy muzyki: oba EXE skompilowane; najnowsze ręczne komendy nadal bez odbioru.
