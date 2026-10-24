# Kroniki: szczegółowy pozostały zakres — 24.10.2026

Postęp: **24/27 grup**, nie procent zgodności gry. To pełny znany backlog z kodu,
aktualnego DAT i wcześniejszych raportów; nie dowód wykrycia każdego błędu kampanii.
Agent nie uruchamia gry, testów ani komend comcheck. Użytkownik zezwolił na okresowe
kompilacje przed STOP. Nowy pakiet wymaga odbioru w grze. Tylko muzyka menu ma
osobne potwierdzenie użytkownika.

## Priorytet: fabuła, awarie i zapis

- [x] Kod synchronizacji TRIA_FINISH i blokady przedwczesnego zamknięcia pustego menu Q102.
- [x] Kod DIACAM_UPDATE/StartDialogCam i odtworzenia kadru po kamerze scenki.
- [ ] Odbiór Q102: Jorn/Fabio, pomijanie, kolejna rozmowa, rutyny i dziennik.
- [x] Most OCNPC__GETNEXTENEMY_REPLACEHOOK: drużyna i widoczność, także bieżący cel.
- [x] Descriptor: wejściowe ARYDAMAGE, bonus pierścienia przed ochroną zamiast
  dodatkowej utraty HP z FDAMAGETOTAL. Dane zaklęcia i ponowny odczyt dontKill.
- [ ] Odbiór wyboru przeciwnika oraz obrażeń: pancerz/penetracja/pierścienie,
  strzały, magia, upadek i walki bez zabijania.
- [x] Nastawienie tymczasowe NPC działa w odczycie i decyzjach AI; permanentny
  odczyt pozostaje oddzielny. Odbiór Q205/Q306/Q602 i uspokojenia po walce.
- [ ] Kolejki AI przy długich scenach, anulowaniu, zniknięciu NPC i zmianie świata;
  limity awaryjne nie są dowodem prawidłowego wykonania sceny.
- [x] Kod fazy przed zapisem: _BW_SAVEGAME przed snapshotem świata/Daedalusa,
  TORCHLOADFIX_CHECKTORCH i końcowy zapis gotowych plików wirtualnych LeGo.
- [ ] PermMem/FF/destruktory, callbacki, napisy, widoki, referencje do NPC/przedmiotów.
- [ ] Restart procesu, wiele loadów, sloty/quicksave, zmiana świata i powrót.
- [ ] Aktywne efekty czasowe po zapisie/wczytaniu.
- [x] Stabilne referencje napisów memory_instance, walidacja pustych symboli
  i NPC teleportacji; timer świata synchronizowany przed callbackami FF.
- [x] ScreenFX wybiera klatkę modulo liczby tekstur, bez dostępu poza tablicę.
- [x] Zabezpieczenia SB_TOSTRING przed pustą strukturą i nieprawidłowym buforem;
  STR_FROMCHAR sprawdza granicę przed odczytem znaku.
- [ ] Dalszy przegląd dereferencji i synchronizacji surowych zapisów z silnikiem.

## Zaklęcia, zamki i postacie

- [x] MAG_BOOK/SetOwner dla oryginalnego SPELL_CAST_SLOWDOWN.
- [x] Zegar modelu time.slw dla animacji, root motion, zdarzeń i okien walki;
  weak owner efektu, przywrócenie skali, zapis zegara i aktywnych time.slw.
- [ ] Odbiór spowolnienia: koniec, ponowne rzucenie, load i Otis Q306, także
  zmiana świata/wyjście postaci; zwykły czar 60 s, Otis 2 s, efekt narkotyku 30 s.
- [x] Pozostały czas SPL_LIGHT zapisuje się osobno i odtwarza bez ponownego rzutu.
- [ ] IsSpellActive/światło Q501: przed, w trakcie, po wygaśnięciu i po loadzie;
  nadal brak ogólnej serializacji wszystkich efektów i faz pojedynczych cząstek.
- [x] Most SPELL_PICKLOCK_FOCUS i filtr TARGET_TYPE_MOB zgodny z callbackiem.
- [x] Postęp otwierania należy do skrzyni/drzwi, z opcjonalnym compat-lock w zapisie;
  obrót 256 tymczasowych slotów nie zeruje już odgadniętych znaków.
- [ ] Odbiór zaklęcia zamka, ClearKeyBuffer, wytrychy i magia: talent/bonus,
  klucze, kod, zapis kodu, cel i poprawne zużycie zwoju.
- [x] Kod wodnego podnoszenia i DoTakeVob dla przedmiotów.
- [x] BUFF_DIVE10_TICK: limit i pozostały tlen 30000/31000 ms, faktyczne obrażenia,
  HUD, regeneracja poza wodą i opcjonalny wpis zapisu compat-breath.
- [ ] Odbiór podnoszenia i buffa pod wodą, wygaśnięcia i zapisu/wczytania.
- [x] BODY_VISUALNAME/HEAD_VISUALNAME oraz osobne BITFIELD z numerami tekstur dla
  NPC_CHANGEFACE i B_REFRESHATINSERT; TELEPORTHEROTOPOS zmienia realną pozycję.
- [x] SPELL_LOGIC_TELEKINESIS, NEW_TELEKINESIS, SPELL_CAST_TELEKINESIS oraz
  SPELL_TELEKINESIS_FOCUS_REMOVER: rzeczywisty przedmiot, mana, ruch i fizyka.
- [ ] Odbiór telekinezy: przedmiot, koszt, dokończenie/anulowanie, zapis i load.
- [x] Kody klawiszy Ikarusa: poprawna konwersja z ustawień INI, cyfry/F1–F15,
  mysz, puszczenie klawiszy przy utracie fokusu i ClearKeyBuffer.
- [ ] Odbiór map LeGo (Escape/Enter/PPM), zaklęcia zamka i skrótów CoM.

## Pozostała semantyka bibliotek

- [x] QSORT_G2: domyślny komparator unsigned pierwszego pola i całe rekordy,
  w tym osiem bajtów tabeli offset/symbol. Nieznany komparator zgłasza brak obsługi.
- [ ] Używane niestandardowe komparatory: udowodnić cele i zaimplementować potrzebne.
- [ ] Finalizacja/destruktory LeGo podczas delete/clear/load, bez mylenia z FINAL.
  FINAL jest optymalizatorem bytecode i ma zero bezpośrednich wywołań w tym DAT.
- [ ] Dynamiczne adresy/callbacki: ustalić konkretne cele, argumenty, wyniki i kontekst.
- [ ] Faktycznie potrzebne instrukcje x86 i kontrakty flag/rejestrów/stosu.
- [ ] Alias VM kontra surowa pamięć: poprawny layout, odczyt i zapis do realnego obiektu.
- [x] SetPositionWorld sprawdza cały bufor vec3, a zapisy pozycji odrzucają NaN/Inf.
- [x] Usuwanie starego przedmiotu nie zeruje symbolu przepiętego na nowy obiekt;
  brak symbolu dla przejściowej instancji nie powoduje dereferencji nullptr.
- [x] CHANGEVOBCOLLISION zmienia również fizykę drzwi/obiektów interaktywnych;
  odtworzona fizyka zachowuje właściciela obiektu. Odbiór przejść fabularnych.
- [x] OCNPC.BITFIELD oddzielone od C_NPC.MISSION; zgodne maski tekstur.
- [ ] Pozostałe flagi OCNPC.BITFIELD: skutki zapisów dla realnego obiektu i trwałość.
- [ ] Potrzebne ścieżki waynetu i list vobów; nie pełna emulacja silnika na zapas.
- [ ] KmLib InitializeMenu/Gamestart/Always i eksporty usług gry; LoadLibrary/
  GetProcAddress są zastępcze. GetStat/IncrementStat/UnlockAchievement oraz trwałość.

## Natywny interfejs, prezentacja i odbiór

- [ ] CATINV: kategorie/strony, stosy/ostatnia sztuka, kontenery, zwłoki, handel,
  blokady i decyzje skryptowe. Wykazać pokrycie natywnym UI dla każdego starego hooka.
- [ ] Crafting: warunki, grupy składników, wynik, liczby, koszty, brak zasobów,
  ilości/Wszystko, zakładki, anulowanie i normalne wykonanie.
- [ ] InformationManager: imię psa/nazwa wina, polskie znaki, kasowanie/Escape/powrót;
  pełny prywatny widget Gothica nie jest emulowany.
- [ ] Scenki: menu/HUD/sterowanie/napisy podczas kamery i plansz końcowych.
- [ ] Sprite/Render: gradienty, dodatkowe blend modes, zagnieżdżone widoki,
  animacje okien, kolor/alpha tekstu, rotacja modeli i kolejność warstw 2D/3D.
- [ ] Muzyka świata: strefy, dzień/noc/walka, nadpisania, powrót po scenie i loadzie.
- [ ] Dziennik: naturalne ukończenie zadań, w tym W gorącej wodzie kąpany, i load.
- [ ] Wyposażenie: warunki, idempotencja, kołczany, pochodnie, ręce, transfer/ostatnia sztuka.
- [ ] Dom: wzory ścian i odtworzenie po loadzie.
- [ ] Ustawienia zasięgu/skalowania, alternatywne loadingi i metadane zapisów.
- [ ] Diagnostyka/konsola/schowek/telemetria; niższy priorytet od fabuły i zapisu.
- [x] Okresowe buildy i poprawki kompilatora; oba EXE zlinkowane, logi zachowane.
- [ ] Po STOP końcowy build aktualnego stanu i lista odbioru.
- [ ] Odbiór scen Q204/Q205/Q304/Q306/Q406/Q501/Q602/GQ001, przedmiotów
  KQ402/SQ202 oraz efektów SQ509.
- [ ] Przejście rozdziałów, dróg gildyjnych i zakończeń.

## Wszystkie rozpoznane pozostałe adresy

Poniższe tabele opisują brak ogólnego handlera/mostu albo atrapę. Zamienniki natywne
mogą już pokrywać część zachowania: crafting, wejście tekstowe, nurkowanie, focus,
CATINV i HUD. Wymagają przeglądu semantyki. Liczby są statyczne, nie pomiarami gry.

### Adresy bez handlera: 30

| Adres | Wywołujący/rejestrujący | Callback | Miejsca wywołań wrappera |
| --- | --- | --- | --- |
| 0x5d3cf0 | ZCRENDERER_DRAWTILE |  | 0 |
| 0x606ad0 | ZCVISUAL_LOADVISUAL |  | 1 |
| 0x64d8e0 | ZRND_D3D_DRAWLINE |  | 0 |
| 0x6b6490 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK2 |  | 0 |
| 0x6d0000 | ZCVOB_SETCOLL |  | 2 |
| 0x709630 | CATINV_DELETELISTSORTFROMPOOL |  | 4 |
| 0x70a150 | CATINV_SWITCHCONTAINER |  | 2 |
| 0x70ade0 | CATINV_RESET |  | 2 |
| 0x70b570 | CATINV_RESET |  | 2 |
| 0x7110a0 | CATINV_CONTAINERINSERT;CATINV_NPCINVINSERT |  | 0 |
| 0x711120 | CATINV_RIGHT;CATINV_SETMAXOFFSET |  | 1 |
| 0x762b40 | CATINV_PREVENTCLOSEDEADINV |  | 0 |
| 0x7813f0 | CATINV_CONTAINERREMOVE;CATINV_NPCINVREMOVE |  | 2 |
| 0x7a5e80 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a6060 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a6070 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a63c0 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a75b0 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a9a10 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7aa030 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7aa140 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7abad0 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7abd10 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7b4870 | GLOBALLOCK |  | 2 |
| 0x7b4876 | GLOBALALLOC |  | 1 |
| 0x7b4a5c | CLOSECLIPBOARD |  | 2 |
| 0x7b4a62 | SETCLIPBOARDDATA |  | 1 |
| 0x7b4a68 | EMPTYCLIPBOARD |  | 1 |
| 0x7b4a6e | OPENCLIPBOARD |  | 2 |
| 0x7d0f48 | CATINV_DELAYMOBCAMERA |  | 0 |

### Hooki bez ogólnego mostu: 43

| Adres | Wywołujący/rejestrujący | Callback | Miejsca wywołań wrappera |
| --- | --- | --- | --- |
| 0x4276b0 | CATINV_INIT | APPLYSOMESETTINGS_HOOK | 1 |
| 0x42926a | INIT_SIGHTSCALING | APPLYSOMESETTINGSEND_HOOK | 1 |
| 0x42a2d0 | INITALTERNATIVELOADINGSCREENS | UPDATELOADINGTEXTURE | 1 |
| 0x42abe7 | CUTSCENESYSTEM_INIT | CGAMEMANAGER__MENUENABLED_REPLACECHECKINGGLOBALCUTSCENE | 1 |
| 0x4344a3 | INIT_FEATURES_GAMESTART | HANDLERESULTSTRING_MARVINMODE_REPLACEHOOK | 1 |
| 0x437d9f | PATCHSAVEGAMEWORLD | PATCHSAVEGAMEWORLD_HOOK | 1 |
| 0x437e55 | PATCHSAVEGAMEVERSION | PATCHSAVEGAMEVERSION_HOOK | 1 |
| 0x44e056 | STARTLOGFILE | ZERROR_SETTARGET_HOOK | 3 |
| 0x660bb0 | INIT_CRAFTINGVIEW_GAMESTART;INIT_FEATURES_GAMESTART | CRAFTINGVIEW_UPDATE;OCINFORMATIONMANAGER__UPDATE_HOOK | 2 |
| 0x68b6ce | CATINV_INIT | CATINV_HANDLEEVENTEBX | 1 |
| 0x68b9d0 | INIT_ITEMIMPORTANTFLAG | TELEMETRY_ONTRADELEFTHOOK | 1 |
| 0x68bba6 | INIT_ITEMIMPORTANTFLAG | TELEMETRY_ONTRADERIGHTHOOK | 1 |
| 0x68eba0 | INIT_FEATURES_GAMESTART | ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK | 1 |
| 0x699c60 | INIT_GATHERINGITEMSUNDERWATER | OCAIHUMAN__PC_DIVING_HOOK | 1 |
| 0x69b947 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK2 | 1 |
| 0x69d415 | CATINV_INIT | CATINV_DELAYMOBCAMERA | 1 |
| 0x6c31d6 | CUTSCENESYSTEM_INIT | OCGAME__UPDATEPLAYERSTATUS_REPLACECHECKINGGLOBALCUTSCENE | 1 |
| 0x6cfdf8 | LEGO_INITGAMESTART | _CC_HOOK | 1 |
| 0x706e03 | CATINV_INIT | CATINV_DRAWCATEGORY | 1 |
| 0x7086d0 | CATINV_INIT | CATINV_OPEN | 1 |
| 0x708f4d | CATINV_INIT | CATINV_CLOSE | 1 |
| 0x7093d6 | CATINV_INIT | CATINV_CONTAINERINSERT | 1 |
| 0x709487 | CATINV_INIT | CATINV_CONTAINERREMOVE | 1 |
| 0x7096e9 | CATINV_INIT | CATINV_CLAMPCATEGORY | 1 |
| 0x709763 | CATINV_INIT | CATINV_RIGHT | 1 |
| 0x709b26 | CATINV_INIT | CATINV_LEFT | 1 |
| 0x70aa5f | CATINV_INIT | CATINV_HANDLEEVENTEDI | 1 |
| 0x70af44 | CATINV_INIT | CATINV_MANIPULATECREATELIST | 1 |
| 0x70b228 | CATINV_INIT | CATINV_HANDLEEVENTEDI | 1 |
| 0x70b6b5 | CATINV_INIT | CATINV_MANIPULATECREATELIST | 1 |
| 0x70b8ad | CATINV_INIT | CATINV_HANDLEEVENTEDI | 1 |
| 0x70b9bd | CATINV_INIT | CATINV_PREVENTCLOSEDEADINV | 1 |
| 0x70cc37 | CATINV_INIT | CATINV_NPCINVREMOVEITEM | 1 |
| 0x70cd22 | CATINV_INIT | CATINV_NPCINVREMOVEBYPTR | 1 |
| 0x70cfa8 | CATINV_INIT | CATINV_NPCINVREMOVE | 1 |
| 0x70f251 | CATINV_INIT | CATINV_HANDLEEVENTNPCINVENTORY | 1 |
| 0x733837 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK3 | 1 |
| 0x733ea4 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK | 1 |
| 0x733eb7 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK | 1 |
| 0x762a18 | CATINV_INIT | CATINV_DEADRESETCATEGORY | 1 |
| 0x76e0f3 | INIT_OCNPCSTATES_CHANGEROUTINE_ERROR | OCNPCSTATES_CHANGEROUTINE_HOOK | 0 |
| 0x7a6060 | INIT_INVTRANSPARENCYHACK | INVTRANSPARENCYHACK_HOOK | 1 |
| 0x7a9240 | CUTSCENESYSTEM_INIT | ZCVIEW__DIALOGMESSAGECXY_HOOK | 1 |

### Proste atrapy: 4

| Adres | Wywołujący/rejestrujący | Callback | Miejsca wywołań wrappera |
| --- | --- | --- | --- |
| 0x44df50 | STARTLOGFILE |  | 3 |
| 0x4cc470 | CATINV_KEYBINDINGISTOGGLED |  | 2 |
| 0x4d4d30 | CATINV_DELAYMOBCAMERA |  | 0 |
| 0x7375e0 | OCNPC__DROPUNCONSCIOUS_HOOK |  | 0 |

## Pola natywnych obiektów: 20 kandydatów do przeglądu

Brak jawnego powiązania nie jest samodzielnym dowodem awarii: kontekst może być
transient memory_instance albo cały wywołujący może mieć działający override.

| Pole | Odniesienia operandów w DAT | Wywołujący |
| --- | --- | --- |
| OCITEM._ZCVOB_TRAFOOBJTOWORLD | 11 | NEW_TELEKINESIS;SPELL_LOGIC_TELEKINESIS |
| OCITEM._ZCVOB_BBOX3D_MINS | 9 | NEW_TELEKINESIS;SPELL_LOGIC_TELEKINESIS |
| OCITEM._ZCVOB_BBOX3D_MAXS | 9 | NEW_TELEKINESIS;SPELL_LOGIC_TELEKINESIS |
| OCNPC._ZCVOB_BITFIELD | 6 | MAKEHEROGHOST;_TRIA_COPY |
| OCNPC._ZCVOB_TRAFOOBJTOWORLD | 6 | SPELL_LOGIC_TELEKINESIS;TELEPORTHEROTOPOS |
| OCITEM._ZCVOB_BITFIELD | 4 | SPELL_CAST_TELEKINESIS |
| OCNPC._ZCVOB_HOMEWORLD | 3 | MEM_INSERTVOB |
| OCNPC._ZCVOB_VISUALALPHA | 3 | MAKEHEROGHOST;_TRIA_COPY |
| OCNPC.ENEMY | 2 | MEMINT_OLDWRITEINT;MEMINT_PREPAREASSIGNMENTS |
| OCNPC.MODEL_SCALE | 2 | _TRIA_COPY |
| OCNPC.MDS_NAME | 2 | _TRIA_COPY |
| OCNPC.INSTANZ | 2 | TELEMETRY_ONTRADELEFTHOOK;TELEMETRY_ONTRADERIGHTHOOK |
| OCITEM.INSTANZ | 2 | TELEMETRY_ONTRADELEFTHOOK;TELEMETRY_ONTRADERIGHTHOOK |
| OCNPC.INVENTORY2_VTBL | 1 | CATINV_OPEN |
| OCNPC.INVENTORY2_INVENTORY_COMPARE | 1 | CATINV_NPCCONTAINERISEMPTY |
| OCNPC._VTBL | 1 | PARSERSYMBOLISNPC |
| OCNPC.INVENTORY2_OCITEMCONTAINER_CONTENTS | 1 | ITEMIMPORTANTFLAG_CHECKSELLINGITEMS |
| OCNPC.INVENTORY2_OCITEMCONTAINER_SELECTEDITEM | 1 | ITEMIMPORTANTFLAG_CHECKSELLINGITEMS |
| OCNPC.DIVETIME | 1 | BUFF_DIVE10_TICK |
| OCNPC.DIVECTR | 1 | BUFF_DIVE10_TICK |

## Wszystkie dynamiczne miejsca DAT: 41

| Wywołujący | Mechanizm | Adres bytecode |
| --- | --- | --- |
| CALL__THISCALL | CALL__STDCALL | 0x242d |
| CALL__FASTCALL | CALL__THISCALL | 0x2482 |
| MEM_SWAPBYTES | CALL__CDECL | 0x25d7 |
| MEM_CLEAR | CALL__CDECL | 0x26a3 |
| LOADLIBRARY | CALL__STDCALL | 0x2973 |
| GETPROCADDRESS | CALL__STDCALL | 0x2a13 |
| VIRTUALPROTECT | CALL__STDCALL | 0x2af4 |
| MEM_MESSAGEBOX | CALL__STDCALL | 0x2b81 |
| MEMINT_QSORT | CALL__CDECL | 0x32a0 |
| STR_FROMCHAR | CALL__THISCALL | 0x35fe |
| STR_COMPARE | CALL__CDECL | 0x390a |
| STR_LOWER | CALL__THISCALL | 0x40cc |
| MEM_FINDPARSERSYMBOL | CALL__THISCALL | 0x415b |
| MEM_GETBUFFERCRC32 | CALL__CDECL | 0x66b4 |
| MEM_SEARCHVOBBYNAME | CALL__THISCALL | 0x678d |
| MEM_SEARCHALLVOBSBYNAME | CALL__THISCALL | 0x6826 |
| MEM_RENAMEVOB | CALL__THISCALL | 0x68af |
| MEMINT_VOBGETEM | CALL__FASTCALL | 0x6907 |
| MEM_TRIGGERVOB | CALL__THISCALL | 0x699b |
| MEM_UNTRIGGERVOB | CALL__THISCALL | 0x6a28 |
| MEM_APPLYGOTHOPT | CALL__THISCALL | 0x6fc1 |
| MEM_GETSYSTEMTIME | CALL__CDECL | 0x7473 |
| MEM_GETPERFORMANCECOUNTER | CALL__STDCALL | 0x74d2 |
| MEMINT_SENDTOSPY_IMPLEMENTATION | CALL__THISCALL | 0x789e |
| MEM_ALLOC_ | CALL__CDECL | 0x84ab |
| MEM_FREE_ | CALL__CDECL | 0x8521 |
| _FLT_BUILD_FUNC | CALL__CDECL | 0x874d |
| HOOKENGINEF | HOOKENGINEI | 0xd675 |
| HOOKENGINE | HOOKENGINEI | 0xd6a6 |
| HOOKENGINES | HOOKENGINE | 0xd6cd |
| REPLACEENGINEFUNCI | HOOKENGINEI | 0xd9fa |
| KMLIB_INITIALIZEMENU | CALL__CDECL | 0x200a5 |
| KMLIB_INITIALIZEGAMESTART | CALL__CDECL | 0x200c0 |
| KMLIB_INITIALIZEALWAYS | CALL__CDECL | 0x200db |
| PC_SCOPE | CALL__CDECL | 0x2012d |
| GETCLIPBOARDDATA | CALL__STDCALL | 0x23f4b |
| GETKEYSTATE | CALL__STDCALL | 0x24613 |
| OCINFOMANAGER_GETINFOUNIMPORTANT | CALL__THISCALL | 0x25a7b |
| GAMESERVICES_UNLOCKACHIEVEMENT | CALL__CDECL | 0x297f0 |
| GAMESERVICES_GETSTAT | CALL__CDECL | 0x2981f |
| GAMESERVICES_INCREMENTSTAT | CALL__CDECL | 0x29864 |

## Deklaracje externali bez bezpośrednich wywołań: 76

Nie są automatycznie wymaganymi implementacjami Kronik. Potrzeba musi wynikać
z rozpoznanego wywołania pośredniego; brak krawędzi BE nie wyklucza takiego użycia.

| Deklaracja | Miejsca wywołania |
| --- | --- |
| PRINTMULTI | 0 |
| SETPERCENTDONE | 0 |
| NPC_MEMORYENTRY | 0 |
| NPC_MEMORYENTRYGUILD | 0 |
| NPC_HASNEWS | 0 |
| NPC_GETNEWSWITNESS | 0 |
| NPC_GETNEWSOFFENDER | 0 |
| NPC_GETNEWSVICTIM | 0 |
| NPC_ISNEWSGOSSIP | 0 |
| NPC_DELETENEWS | 0 |
| NPC_GETGUILDATTITUDE | 0 |
| WLD_DETECTPLAYER | 0 |
| WLD_DETECTNPCEXATT | 0 |
| NPC_CREATESPELL | 0 |
| NPC_LEARNSPELL | 0 |
| NPC_GIVEITEM | 0 |
| NPC_HASDETECTEDNPC | 0 |
| NPC_GETCOMRADES | 0 |
| NPC_ISNEAR | 0 |
| NPC_KNOWSPLAYER | 0 |
| NPC_SETKNOWSPLAYER | 0 |
| NPC_ISAIMING | 0 |
| NPC_STARTITEMREACTMODULES | 0 |
| NPC_HASOFFERED | 0 |
| AI_PLAYCUTSCENE | 0 |
| AI_CANSEENPC | 0 |
| AI_GOTOSOUND | 0 |
| NPC_AREWESTRONGER | 0 |
| NPC_ISNEXTTARGETAVAILABLE | 0 |
| NPC_ISVOICEACTIVE | 0 |
| NPC_GIVEINFO | 0 |
| TAL_CONFIGURE | 0 |
| NPC_ISWAYBLOCKED | 0 |
| AI_WHIRLAROUNDTOSOURCE | 0 |
| AI_TURNTOSOUND | 0 |
| AI_QUICKLOOK | 0 |
| AI_DEFEND | 0 |
| AI_COMBATREACTTODAMAGE | 0 |
| AI_ASK | 0 |
| AI_ASKTEXT | 0 |
| AI_WAITFORQUESTION | 0 |
| AI_SND_PLAY3D | 0 |
| SND_GETDISTTOSOURCE | 0 |
| SND_ISSOURCENPC | 0 |
| SND_ISSOURCEITEM | 0 |
| MIS_GETSTATUS | 0 |
| MIS_SETSTATUS | 0 |
| MIS_ONTIME | 0 |
| MIS_ADDMISSIONENTRY | 0 |
| MIS_REMOVEMISSION | 0 |
| NPC_CHECKAVAILABLEMISSION | 0 |
| NPC_CHECKRUNNINGMISSION | 0 |
| NPC_CHECKOFFERMISSION | 0 |
| AI_PLAYFX | 0 |
| WLD_INSERTNPCANDRESPAWN | 0 |
| NPC_OWNEDBYGUILD | 0 |
| AI_TAKEMOB | 0 |
| AI_DROPMOB | 0 |
| DOC_OPEN | 0 |
| DOC_FONT | 0 |
| DOC_PRINT | 0 |
| DOC_MAPCOORDINATES | 0 |
| TA | 0 |
| TA_CS | 0 |
| TA_BEGINOVERLAY | 0 |
| TA_ENDOVERLAY | 0 |
| TA_REMOVEOVERLAY | 0 |
| RTN_EXCHANGE | 0 |
| WLD_ASSIGNROOMTONPC | 0 |
| WLD_GETPLAYERPORTALOWNER | 0 |
| WLD_GETFORMERPLAYERPORTALOWNER | 0 |
| NPC_ISPLAYERINMYROOM | 0 |
| NPC_WASPLAYERINMYROOM | 0 |
| NPC_GETPORTALOWNER | 0 |
| HLP_CUTSCENEPLAYED | 0 |
| GAME_INITENGINTL | 0 |

## Dane i ograniczenia

DAT SHA256: `7fba1af724ae528836a2537573b0d36024cd8a92d493e15a11c8e159ef44d54e`. Przejrzano 206709 symboli
i 240563 statycznych wywołań BL/BE. Nierozpoznane bezpośrednie
cele bytecode: 0. To nie rozstrzyga semantyki handlerów
ani przechodniości gry. Szczegóły wykonanych zmian: ARCHOLOS_AUDIT_2026-10-24.md.
