# Postęp: Ikarus / LeGo / Kroniki Myrtany (CoM)

Stan z 20.10.2026. Cel: Kroniki Myrtany w pełni grywalne w OpenGothic, przy zachowaniu zgodności z oryginalnym Gothic 1 i 2.

Legenda:

- `[x]` zrobione
- `[x]` + *(potwierdzone)* zrobione i zbudowane, czeka na ręczną próbę użytkownika
- `[ ]` do zrobienia

Zasada zgodności: wszystko, co dotyczy Ikarus/LeGo/CoM, działa tylko w grach, które mają warstwę Ikarus/LeGo (`GameScript::hasDma()`) albo zarejestrowany hook. Czyste G1 i G2 zachowują dawne zachowanie.

Szczegóły techniczne: [IKARUS.md](IKARUS.md), [ARCHOLOS_AUDIT.md](ARCHOLOS_AUDIT.md), [ARCHOLOS_TESTING.md](ARCHOLOS_TESTING.md), [ARCHOLOS_HANDOFF_CLAUDE.md](ARCHOLOS_HANDOFF_CLAUDE.md), [ARCHOLOS_HANDOFF_BACKLOG.md](ARCHOLOS_HANDOFF_BACKLOG.md).

---

## 1. Rdzeń warstwy zgodności (`common/game/compatibility`)

- [x] Interpreter x86 `Cpu32` (wywołania `thiscall`/`cdecl`/`stdcall`, stos, rejestry) dla kodu Ikarusa
- [x] Pamięć 32-bitowa `Mem32` z regionami typowanymi (callbacki odczytu i zapisu po indeksie elementu)
- [x] Poprawna kolejność argumentów wywołań (GCC liczy argumenty od prawej do lewej, naprawione przez inicjalizację w nawiasach klamrowych)
- [x] `DirectMemory`: centralna klasa, pliki `directmemory_*.cpp` (silnik, zdarzenia, hooki, UI, pliki)
- [x] Instancje `C_NPC`/`C_ITEM` widziane przez wskaźniki Ikarusa (`MEM_InstToPtr`, `_@`, `MEM_PtrToInst`)
- [x] Pola `oCNpc`/`oCItem` jako aliasy pól `C_NPC`/`C_ITEM`
- [x] Lustro `zCParser`, `zCPar_Symbol`, wartości zmiennych skryptowych (także stringi zbudowane przez skrypty)
- [x] `MEM_ReplaceFunc` jako prawdziwe przekierowanie (wcześniej nieskończona rekurencja przy zapisie)
- [x] Nadpisywanie funkcji VM (`override_function`, parametry tekstowe jako `std::string_view`)
- [x] Pętle Ikarusa (`repeat`, `while`, `break`, `continue`, `mem_goto`)
- [x] Wyjątki VM z pełnym stosem wywołań w logu
- [x] Rozmiar ekranu widziany przez skrypty (800x600, całkowite PSIZEX/PSIZEY, naprawiony `fracf: Denominator is 0` w paskach LeGo)
- [x] Odczyt `MEM_ReadInt`/`MEM_WriteInt` na końcu bloku nie wychodzi poza bufor (dostęp bajt po bajcie przez granicę bloku, 64-bitowe porównania zakresów)
- [x] `InstanceHandles`: 8192 sloty, najpierw wolne i martwe (nikt ich już nie trzyma), dopiero potem nadpisanie najstarszego żywego z logiem błędu
- [x] Wyjątek `BAR_DELETE`/`FREE` (LeGo woła `Bar_Delete(var int)` z instancją): `DaedalusVm::pop_int` zwraca indeks symbolu instancji, jak oryginalny silnik
- [x] `OCNPC.FOCUS_VOB` i `OCNPC.INTERACTMOB` mają własne pola (`compat_focus_vob`, `compat_interact_mob`), nie nadpisują `VOICEPITCH`/`SPAWNDELAY`
- [x] `INTERACTMOB` gracza odświeżany z rzeczywistego moba
- [ ] Stabilna tożsamość uchwytów obiektów między zapisami i światami (dziś tylko ponowne użycie slotów martwych obiektów)
- [ ] `OCNPC.BITFIELD` aliasuje `mission`: pełny wpływ na ruch i stany
- [ ] Pola `_ZCVOB_*`, `MAG_BOOK`, model, wygląd, wyposażenie
- [ ] Zapis macierzy rotacji ma obracać rzeczywisty obiekt (nie tylko pamięć emulowana)
- [ ] Odczyt członków VM i surowe `MEM_ReadInt(ptr+offset)` spójne dla wszystkich pól
- [ ] Życie uchwytów mobów i odtwarzanie referencji po usunięciu obiektu
- [ ] `qsort`, `FINAL` w LeGo; pełna semantyka destruktorów i locals
- [ ] Nieznane wywołanie nie może zostawiać przypadkowego starego wyniku jako sukcesu (uczciwa diagnostyka)

## 2. Zapisy i trwałość (PermMem)

- [x] Wirtualne pliki WinAPI (`CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`, `GetFileSize`, `GetLastError`) w pamięci, przechowywane w zapisie OpenGothic (`game/dma`)
- [x] Zapis, wczytanie i restart gry zachowują dane LeGo (PermMem)
- [x] Po wczytaniu uruchamiane są `INITPERCEPTIONS` i `Init_Global`/`Init_<świat>` dla modów z warstwą DMA
- [x] Hooki zmiany poziomu LeGo nie traktują zmiany poziomu jako wczytania
- [x] Zapis nie wiesza gry (naprawiona rekurencja `MEM_ReplaceFunc`)
- [x] Zapis w trakcie czekania (kolejka AI) i restart procesu
- [ ] Odbiór zapisu poza tym, co potwierdzone: PermMem destruktory, FF, widoki, ciągi, referencje NPC/przedmiotów, aktywne efekty czasowe
- [ ] Wielokrotne wczytania w jednej sesji, wiele slotów, quicksave
- [ ] Zmiana świata i powrót, usuwanie i odtwarzanie obiektów, brak wiszących wskaźników
- [ ] `ReadSavegame`/`_BR_SETSELECTEDSLOT`: callback jest deklarowany, ale nie jest emitowany
- [ ] Zgodność starych zapisów po każdej zmianie serializacji (obecnie `Serialize::Current` = 56, starsze wczytywane warunkowo)

## 3. Czas, świat, wyszukiwanie

- [x] `zTimer` (`MEM_Timer`): czas klatki i czas całkowity aktualizowane co klatkę
- [x] `oCWorldTimer` (`MEM_WorldTimer`): `SCALETIME`, `CHANGETIMESPEED`, `STOPTIMEATHOUR`, `RESUMETIME` zmieniają czas gry w OpenGothic *(potwierdzone próbą `comcheck time`)*
- [x] `MEM_SearchVobByName` dla moverów (zadania szukają ich po nazwie)
- [x] Lista NPC świata (`voblist_npcs`)
- [x] `Wld_SetObjectRoutine`
- [x] `Wld_RemoveItem`, `Mob_CreateItems`
- [ ] `WAYNET` (lista waypointów) nadal fałszywy wskaźnik
- [ ] Ogólna lista vobów (nie tylko NPC), wyszukiwanie po nazwie dla NPC, przedmiotów, skrzyń, dekoracji
- [ ] `Wld_InsertObject`, rzeczywiste `RemoveVob`, `SetVobName`, tworzenie i usuwanie obiektów zadań
- [ ] `MEM_InsertVob` (używane tylko przez kołczany, które są wyłączone)
- [ ] `TIMESTEP`, pauza i spowolnienie czasu klatek w silniku

## 4. Hooki silnika (zdarzenia gry oddane skryptom)

Emitowane i odebrane przez hooki Kronik/LeGo:

- [x] `ChangeLevel` (początek i koniec), `WriteSavegame`, `ReadSavegame` (osobną drogą)
- [x] `OCNPCINVENTORY__INSERT` (podniesienie przedmiotu), `ChangeAttribute` (skrypt może zmienić wartość)
- [x] Klawisze (`KeyEvent`, stan klawiszy), `ZoneMusicChanged`, `CreateFallDamage`
- [x] `OnDamage`, `OnDamageScript`, `SpellSetup`, `MobCanOpen`
- [x] Kamery scen filmowych (`CsCamera` trigger/untrigger)
- [x] Ekran statusu postaci (`StatusScreenShow`/`Close` + teksty `UPDATE_MENU_ITEM`)
- [x] `BarUpdateResolution`
- [x] `OCNPC__EVDROPVOB_HOOK`: ważny przedmiot (`ITEM_IMPORTANT`) nie wypada z ekwipunku gracza *(potwierdzone)*
- [x] `LOCKINVENTORY`: ekwipunek zamyka się po otwarciu, gdy skrypt ustawi blokadę *(potwierdzone próbą `comcheck inventory`)*
- [x] `ITEMIMPORTANTFLAG_CHECKSELLINGITEMS`: ważnych przedmiotów nie da się sprzedać handlarzowi *(potwierdzone)*
- [x] `HIDEFOCUSFORPARTYMEMBERS_*`: towarzysze (`AIVAR[15]`) poza focusem gracza z bronią i bez paska HP *(potwierdzone próbą `comcheck party`)*
- [x] `EV_FINISHINGMOVE_HOOK`: dobijanie ważnej postaci; zdrowie zostaje na 1 przez hook `ChangeAttribute` (postać nie ginie) *(potwierdzone)*
- [x] Zabójca dobitej postaci (`OTHER` w `ZS_DEAD`, doświadczenie gracza) jest ustawiany także przy dobijaniu *(niepotwierdzone)*
- [x] Dobijanie ważnej postaci gra tylko `T_NO` bez animacji dobijania; naprawiony `ANIISACTIVE` (`zCModel::IsAnimationActive`) *(potwierdzone)*
- [ ] `OCNPC__GETNEXTENEMY_REPLACEHOOK`: celowo niewdrożone (poprawka błędu oryginalnego silnika, własny wybór wroga OpenGothic działa inaczej); do ponownej oceny, jeśli pojawią się objawy
- [ ] `OCNPC__DROPUNCONSCIOUS_HOOK` (Q205, Q306: broń gracza po utracie przytomności); wymaga emulacji slotów rąk i `RemoveFromSlot`/`EquipWeapon`
- [ ] `OCNPCSTATES_CHANGEROUTINE_HOOK`, odświeżanie NPC po zmianie rutyny
- [ ] `OCAIHUMAN__PC_DIVING_HOOK` (zbieranie pod wodą): OpenGothic już pozwala podnosić przedmioty w `BS_DIVE` (`Npc::takeItem`); sprawdzić w grze, czy hook jest potrzebny
- [x] `SETDIALOGUETOLDPROPERTY` (`oCInfoManager::GetInformation` + `OCINFO.TOLD`): skrypt może oznaczyć dialog jako opowiedziany lub pozwolić go powtórzyć *(potwierdzone)*
- [x] `HANDLE_SETPROPERPICKLOCKSTRLEN`: długość kombinacji wytrycha skracana przez talent i bonus (`GETPICKLOCKSTRLEN`) *(potwierdzone)*
- [x] `SPELL_PICKLOCK_FOCUS` pokryty natywnym focusem czaru (`eventSpellFocus`), `ClearKeyBuffer` nie jest potrzebny
- [x] Zwój czaru otwierania zamków (`SPL_PICKLOCK`) jest zużywany tylko, gdy czar otworzył zamek; przy zbyt krótkim przytrzymaniu postęp zostaje w zamku, zwój zostaje *(potwierdzone)*
- [ ] `_FIXEQUIPBESTWEAPONS`, `TORCHLOADFIX_CHECKTORCH`
- [ ] `_EVT_QUIVER_EQUIP`/`_EVT_QUIVER_UNEQUIP` (kołczany)
- [ ] `CATINV_*` (kategorie ekwipunku: ok. 25 hooków)
- [ ] `CRAFTINGVIEW_UPDATE`, `OCINFORMATIONMANAGER__UPDATE_HOOK`, `ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK`, `OCITEMCONTAINER__DRAWITEMINFO_HOOK`
- [x] `OCAIHUMAN__MOVING_REPLACECHECKINGGLOBALCUTSCENE`: gracz nie porusza się podczas końcowych plansz (`ISINFINALBOARDS`); kamery scen (`ISINCAMERA`) blokuje sam OpenGothic *(niepotwierdzone)*
- [ ] `ZCVIEW__DIALOGMESSAGECXY_HOOK` (kolor i nazwa w tekstach dialogu podczas scen), `OCGAME__UPDATEPLAYERSTATUS_REPLACECHECKINGGLOBALCUTSCENE` (HUD podczas scen), `CGAMEMANAGER__MENUENABLED_REPLACECHECKINGGLOBALCUTSCENE` (menu w scenach)
- [ ] `INVTRANSPARENCYHACK_HOOK`, `APPLYSOMESETTINGS_HOOK`, `APPLYSOMESETTINGSEND_HOOK`, `UPDATELOADINGTEXTURE`
- [ ] `PATCHSAVEGAMEVERSION_HOOK`, `PATCHSAVEGAMEWORLD_HOOK`: tylko wpisują wersję gry i nazwę świata do slotu zapisu w menu oryginalnego silnika (kosmetyka listy zapisów), OpenGothic ma własną listę zapisów
- [ ] `TELEMETRY_ONTRADELEFTHOOK`, `TELEMETRY_ONTRADERIGHTHOOK`, `ZERROR_SETTARGET_HOOK`, `HANDLERESULTSTRING_MARVINMODE_REPLACEHOOK`
- [ ] `_CC_HOOK` (konsola CC), `_SPRITE_DORENDER`

## 5. Wywołania natywne Ikarusa (adresy silnika)

- [x] `TriggerChangeLevel`, `SetPositionWorld`, `GetModel`, `StartAnimation`, `CASTFROMINTF`/`TOINTF`
- [x] Obiekty `zCInput` i zdarzenia klawiszy
- [x] `zSinCosApprox`
- [x] `UPDATE_MENU_ITEM` (teksty menu statusu)
- [x] `zCModel::IsAnimationActive` (`ANIISACTIVE`)
- [x] `oCNpc::CloseInventory`, `zCEventMessage::SetDeleted` (używane przez hooki)
- [x] `RemoveVob`, `SetVobName`, `ApplySomeSettings` (obecnie puste wywołania bez skutku)
- [ ] `GetSlotItem` (zwraca 0), `EquipWeapon` (nop), `PutInSlot`, `RemoveFromSlot`, `DropFromSlot`, `Equip`, `UnequipItem`, `EquipBestWeapon`, `DropAllInHand`
- [ ] `OCNPC_ISSPELLACTIVE`, `SPELL_CAST_SLOWDOWN`
- [ ] `STOPALLACTIVESOUNDS`, `STOPALLSOUNDS`
- [ ] `0x688a70` (Q204 `ARAXOSJOINTEST_COLLISION`, Q304 `TRAPCUTSCENE_SETCHASMDETECTION`): ustalić semantykę natywną
- [ ] `zCVob::SetColl` (`ZCVOB_SETCOLL`)
- [ ] `GETPARMVALUE`, `DIACAM_UPDATE`, `ZCVISUAL_LOADVISUAL`
- [ ] `CLOSEINVENTORY` jako pełna operacja silnika (dziś tylko sygnał zamknięcia menu)
- [ ] Renderer: `ZRND_*`, `ZCRENDERER_DRAWTILE`, `OCITEM_RENDER`, `PRINT_GETFONTNAME`
- [ ] `APPLYHOUSEWALLTEXTURE` (3 adresy), `zCTexture::Load` (zwraca 0)
- [ ] `Console::Register` (`CC_AUTOCOMPLETE`), schowek (`OpenClipboard` i pokrewne), `GlobalAlloc`/`GlobalLock`
- [ ] `IsBindedToggled`, `GetState` (`CATINV_DELAYMOBCAMERA`)

## 6. Zewnętrzne funkcje skryptowe (externale)

- [x] `AI_WaitTillEnd` (znaczniki synchronizacji w kolejkach, wzajemne czekanie, `clear()` zwalnia czekających)
- [x] `AI_DropItem`, `AI_AimAt`, `AI_ShootAt`, `AI_StopAim`, `AI_Snd_Play`, `AI_StopFX`
- [x] `Mob_CreateItems`, `Wld_RemoveItem` (zwraca `int`)
- [x] `Wld_SetObjectRoutine`
- [x] `AI_Function` (LeGo): parser `CALL ...` (I/N/S/SS/NSII), kontekst `SELF`, wynik zdjęty ze stosu
- [x] `DaedalusVm::invoke_function` w ZenKit (lokalny patch)
- [ ] `Wld_InsertObject` (jedyny wywoływany nieobsłużony external)
- [ ] 76 deklaracji bez rozpoznanych wywołań (nie są listą blockerów: `NPC_MemoryEntry`, `Mis_*`, `Doc_*`, `TA_*` i inne; ocenić potrzebę dla CoM)
- [ ] `Log_MoveToTop` (nadal logowany jako niezaimplementowany), `_Render_Init`, `Mdl_ApplyRandomAni*` (celowe puste)

## 7. Sloty, ręce, wyposażenie

- [ ] Sloty: `PutInSlot`, `RemoveFromSlot`, `DropFromSlot`, `GetSlotItem` (pochodnia)
- [ ] Pochodnie: zajętość rąk, odtworzenie po wczytaniu
- [ ] Kołczany (`INIT_QUIVERS_ALWAYS` świadomie wyłączone) i hooki equip/unequip
- [ ] Wybór najlepszej broni, rzeczy po utracie przytomności
- [ ] Zajętość rąk przy sprzedaży, upuszczeniu, magii

## 8. Walka, magia, zamki

- [x] Czar otwierania zamków z Kronik: focus na skrzyni (`Focus_Magic`, `syncFocusMage`)
- [x] Strzelanie: `AI_AimAt`/`AI_ShootAt`/`AI_StopAim`, pocisk powstaje i zużywa amunicję (log: `PROJECTILE CONFIRMED`; trafienia i widoczność nie mierzone)
- [x] `OnDamage`/`OnDamageScript`, obrażenia od upadku
- [ ] Deskryptor obrażeń jest niepełny; `FDAMAGETOTAL`, pancerz, penetracja, pierścienie, strzały, magia, nieśmiertelność
- [ ] Skracanie kodu zamka przez talent/bonus, aktualny mob, klucze, wymagania
- [ ] Telekineza, nurkowanie, spowolnienie (`OCNPC_ISSPELLACTIVE`)
- [ ] Wybór przeciwnika dla towarzyszy (patrz 4)

## 9. Dialogi, UI, wpisywany tekst

- [x] Wpisywanie odpowiedzi (`?Nazwa`, `INFOMANAGERANSWER`) z klawiatury fizycznej i ekranowej; tylko w grach z Ikarus/LeGo *(niepotwierdzone po ostatnich poprawkach)*
- [x] Nakładka UI (widoki, teksty, czcionki, tekstury LeGo) rysowana nad grą
- [x] Napisy na ekranie, wpisy dziennika
- [x] Ekran statusu postaci z tekstami CoM
- [x] Wybory dialogu czekają na kolejki akcji postaci (limit 10 s); tylko w grach z Ikarus/LeGo
- [x] `player.openTrade(npc)` w modach JS i `comcheck trader`: okno handlu bez dialogu
- [ ] Polskie znaki w polu tekstowym (dziś ASCII 32–126, limit 24 bajtów) z właściwym kodowaniem fontów/VM
- [ ] Crafting: widok receptur, warunki, składniki, wynik, liczba sztuk, anulowanie
- [ ] `CATINV_*`: kategorie ekwipunku, strony, przenoszenie i stosy, reset po śmierci
- [ ] Informacje o przedmiotach, boss UI, status, dziennik (`LOG_MOVETOTOP`)
- [ ] Rozmiar ekranu 800x600 stały: zdarzenie rozdzielczości tylko raz (zmiana okna, skali, HUD)
- [ ] Sprite, `Render/Draw3D`, zagnieżdżone widoki, animacje okien, alpha i kolor tekstu
- [ ] `GetFontName`, przezroczystość ekwipunku, tekstury ekranu ładowania, skalowanie widzenia
- [ ] Konsola CC, autouzupełnianie, schowek

## 10. Sceny, kamera, dźwięk

- [x] Hooki kamer scen filmowych (`OnTrigger`/`OnUntrigger`)
- [x] Scena palaczy (Q-scena `smokers`, `comcheck scene smokers`) potwierdzona przez użytkownika
- [x] Zmiana poziomu i teleport z poziomu skryptów
- [x] `AI_PlayAni` z `CALL ...` i parametrami (pętla zdarzeń kolejki)
- [ ] Sceny trzech i więcej NPC z `TRIA_WAIT`, przedmiotem i ruchem; zagnieżdżone `AI_Function`; anulowanie i przerwanie sceny
- [ ] `StartDialogCam`, sprawdzanie globalnej cutscenki przez HUD, ruch i menu; powrót kamery
- [ ] Sceny do odbioru: Q204, Q205, Q304, Q306, Q406, Q602/GQ001; `Wld_RemoveItem` w KQ402/SQ202; `AI_StopFX` w SQ509; naturalne `AI_Snd_Play`
- [ ] Aktywne animacje, ładowanie visuali, zatrzymywanie aktywnych dźwięków

## 11. KmLib, muzyka, usługi

- [ ] `LoadLibrary`/`GetProcAddress` dają pozorne uchwyty: adapter używanych funkcji KmLib
- [ ] `InitializeMenu`, `InitializeGamestart`, `InitializeAlways`
- [ ] `MusicSystem_OverrideTrack`, `MusicSystem_DisableTrackOverride`, przywrócenie muzyki po scenie i wczytaniu
- [ ] `GameServices_UnlockAchievement`, `GetStat`, `IncrementStat` (deterministyczny wynik i trwałość)

## 12. Stabilność i diagnostyka

- [x] Crash przy zamykaniu gry (`RectAllocator::decref` Tempest): ikony dotykowe trzymają jedną referencję do końca procesu
- [x] Oddawanie sprite'ów ekwipunku i kontrolera przed niszczeniem okna
- [x] Log hooków (które hooki Kronik zostały osiągnięte, które nie są obsłużone)
- [x] Logi nakładek animacji (`Mdl_ApplyOverlayMds`/`RemoveOverlayMds`), kolejki AI dla animacji `T_PET*`, `T_NO`, `CALL ...`
- [x] Disasembler `tools/ikarus/disasm.py` do funkcji `GOTHIC.DAT`, których nie ma w dekompilacji
- [x] Analizator `tools/ikarus/analyze.py` (statyczny, częściowo nieaktualny)
- [x] Ostrzeżenia ZenKit "Accessing member ... without an instance set" zawierają teraz nazwę funkcji skryptu (patch w `patches/`)
- [x] Źródło ostrzeżeń na starcie świata ustalone z logu i uznane za nieszkodliwe: `RESTORENPCNAMES` czyta `AIVAR[79]` postaci, których nie ma jeszcze w świecie (0, tak jak w oryginale), `INIT_ITEMGROUPS_MEAT` zapisuje `HP` w instancjach grup przedmiotów (konstruktor instancji i tak ustawia to pole), `SPELL_TELEKINESIS_FOCUS_REMOVER` odpala się przed pojawieniem się bohatera
- [ ] `ApplyHouseWallTexture: Null ptr` na starcie świata: zmiana tekstury ścian domu gracza wymaga wymiany tekstury materiału w rendererze (`zCTexture::Load` i ponowne wczytanie danych); kosmetyka, bez wpływu na fabułę
- [ ] Symbole i offset w `crash.log` (dziś tylko nazwy funkcji)
- [ ] Przejrzeć 10-sekundowy limit oczekiwania dialogu na kolejki (arbitralny)

## 13. Narzędzia do ręcznych prób (`comcheck`, mod Developer tools)

- [x] `comcheck text`, `wait [s]`, `cancel`, `drop [przedmiot]`, `chest [vob] [przedmiot] [ilość]`
- [x] `comcheck ranged [bow|crossbow|aim|shoot|stop|status|clear]`
- [x] `comcheck scene smokers`
- [x] `comcheck time [stop]`, `comcheck inventory [off]`
- [x] `comcheck trader`, `comcheck party [off]`, `comcheck finish [flag]`, `comcheck lock [kod|off|items|talent N|bonus N]` (skrzynie, talent i bonus wytrycha, zwoje)
- [x] API modów JS: `debug.call`, `debug.setSymbol`, `debug.parkNpc`, `debug.placeOnGround`, `debug.setInfoKnown`, `npc.openTrade`, `npc.aiVar`/`setAiVar`, `npc.flags`/`setFlags`, `npc.rangedShots`
- [x] Spawn postaci i dodawanie przedmiotów w zakładce Debug (F10)
- [ ] Komendy przygotowujące sceny dla kolejnych zadań (Q204, Q205, Q304, Q306, Q406, Q602) bez zapisów odbiorowych

## 14. Zgodność z G1 i G2 oraz repozytorium

- [x] Dialogi: oczekiwanie na kolejki i odpowiedzi `?...` tylko w grach z warstwą Ikarus/LeGo
- [x] Hooki gry (upuszczanie, sprzedaż, ekwipunek, focus kompanów, dobijanie, czas świata) tylko po zarejestrowaniu hooka przez mod
- [x] Zmiany ZenKit trzymane jako patch w `patches/zenkit-opengothic.patch` (submoduł jest upstreamowy, brak forka): `invoke_function`, `pop_int` instancji, pola `compat_*` w `INpc`
- [x] Zapisy w wersji 56, nowe akcje AI dopisane na końcu (stare numery zachowane)
- [ ] Fork ZenKit z tymi zmianami (żeby submoduł wskazywał na commit dostępny w sieci)
- [ ] Przegląd zgodności zapisów G1/G2 po zmianach serializacji (nowe pola tylko z wersjonowaniem)
- [ ] Ręczna regresja czystego G1 i G2 (dialogi, handel, ekwipunek, walka)

## 15. Przechodniość Kronik (nadrzędny cel)

- [ ] Ręczna regresja wszystkich rozdziałów, dróg gildyjnych, zakończeń i zadań pobocznych
- [ ] Porównanie z oryginalną grą w tej samej wersji danych (`GAME_VERSION=23`, `BUILD_VERSION=142565`, Ikarus `10202`, LeGo `2.7.0`)
- [ ] Lista zapisów odbiorowych dla każdej mechaniki
- [ ] Brak znanych blockerów fabuły i utraty zapisów

Nie podajemy procentu ukończenia na podstawie liczby obsłużonych hooków i adresów: liczba miejsc wywołania w DAT nie mówi, ile razy dana funkcja zostanie wykonana w grze.
