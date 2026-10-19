# Handoff Claude — szczegółowy backlog ze statycznego audytu

Stan dokumentu: 20.10.2026. Czytaj razem z [handoffem głównym](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_CLAUDE.md). Tabele pochodzą z audytu 19.10.2026, nie z ponownego uruchomienia gry. Nie przedstawiają nowych wyników testów. Dziewięć externali już zaimplementowano; pozostałe historyczne statusy sprawdź w aktualnym kodzie przed zmianą.

Brak hooka lub adresu jest wskazaniem do zbadania semantyki. Dla natywnego UI można wdrożyć równoważne zachowanie bez emulowania oryginalnego renderera. Clipboard, telemetria i diagnostyka nie mają automatycznie priorytetu blockerów fabuły. Dynamiczne cele nie są w pełni rozstrzygnięte przez ten audyt.

## 1. Hooki bez mostu zdarzenia / deklarowane bez emisji

61 miejsc rejestracji, 60 różnych adresów. Zachowano powtórzenia, bo ten sam adres może mieć różne callbacki. W szczególności `0x660bb0` jest używany przez crafting i information manager. `_BR_SETSELECTEDSLOT` ma osobną drogę zastępczą ustawiania slotu — brak emisji callbacka nie dowodzi sam w sobie zepsutych zapisów.

| Adres | Callback | Rejestracja | Status historyczny |
| --- | --- | --- | --- |
| 0x6cfdf8 | _CC_HOOK | LEGO_INITGAMESTART | no_event_bridge |
| 0x42a040 | _BR_SETSELECTEDSLOT | LEGO_INITGAMESTART | declared_but_never_fired |
| 0x64df20 | _SPRITE_DORENDER | LEGO_INITGAMESTART | no_event_bridge |
| 0x723ee2 | HANDLE_SETPROPERPICKLOCKSTRLEN | INIT_RANDOMIZEPICKLOCKS_GAMESTART | no_event_bridge |
| 0x745fa6 | _FIXEQUIPBESTWEAPONS | FIXEQUIPBESTWEAPONS_INIT | no_event_bridge |
| 0x7086d0 | CATINV_OPEN | CATINV_INIT | no_event_bridge |
| 0x708f4d | CATINV_CLOSE | CATINV_INIT | no_event_bridge |
| 0x709763 | CATINV_RIGHT | CATINV_INIT | no_event_bridge |
| 0x709b26 | CATINV_LEFT | CATINV_INIT | no_event_bridge |
| 0x70aa5f | CATINV_HANDLEEVENTEDI | CATINV_INIT | no_event_bridge |
| 0x70b8ad | CATINV_HANDLEEVENTEDI | CATINV_INIT | no_event_bridge |
| 0x70b228 | CATINV_HANDLEEVENTEDI | CATINV_INIT | no_event_bridge |
| 0x68b6ce | CATINV_HANDLEEVENTEBX | CATINV_INIT | no_event_bridge |
| 0x709487 | CATINV_CONTAINERREMOVE | CATINV_INIT | no_event_bridge |
| 0x70cfa8 | CATINV_NPCINVREMOVE | CATINV_INIT | no_event_bridge |
| 0x70cd22 | CATINV_NPCINVREMOVEBYPTR | CATINV_INIT | no_event_bridge |
| 0x70cc37 | CATINV_NPCINVREMOVEITEM | CATINV_INIT | no_event_bridge |
| 0x7093d6 | CATINV_CONTAINERINSERT | CATINV_INIT | no_event_bridge |
| 0x762a18 | CATINV_DEADRESETCATEGORY | CATINV_INIT | no_event_bridge |
| 0x706e03 | CATINV_DRAWCATEGORY | CATINV_INIT | no_event_bridge |
| 0x4276b0 | APPLYSOMESETTINGS_HOOK | CATINV_INIT | no_event_bridge |
| 0x7096e9 | CATINV_CLAMPCATEGORY | CATINV_INIT | no_event_bridge |
| 0x70af44 | CATINV_MANIPULATECREATELIST | CATINV_INIT | no_event_bridge |
| 0x70b6b5 | CATINV_MANIPULATECREATELIST | CATINV_INIT | no_event_bridge |
| 0x70b9bd | CATINV_PREVENTCLOSEDEADINV | CATINV_INIT | no_event_bridge |
| 0x70f251 | CATINV_HANDLEEVENTNPCINVENTORY | CATINV_INIT | no_event_bridge |
| 0x69d415 | CATINV_DELAYMOBCAMERA | CATINV_INIT | no_event_bridge |
| 0x76e0f3 | OCNPCSTATES_CHANGEROUTINE_HOOK | INIT_OCNPCSTATES_CHANGEROUTINE_ERROR | no_event_bridge |
| 0x734f4d | OCNPC__GETNEXTENEMY_REPLACEHOOK | INIT_GETNEXTENEMYFIX | no_event_bridge |
| 0x733eb7 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK | INIT_HIDEFOCUSFORPARTYMEMBERS | no_event_bridge |
| 0x733ea4 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK | INIT_HIDEFOCUSFORPARTYMEMBERS | no_event_bridge |
| 0x69b947 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK2 | INIT_HIDEFOCUSFORPARTYMEMBERS | no_event_bridge |
| 0x733837 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK3 | INIT_HIDEFOCUSFORPARTYMEMBERS | no_event_bridge |
| 0x6c36db | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK4 | INIT_HIDEFOCUSFORPARTYMEMBERS | no_event_bridge |
| 0x699c60 | OCAIHUMAN__PC_DIVING_HOOK | INIT_GATHERINGITEMSUNDERWATER | no_event_bridge |
| 0x437e55 | PATCHSAVEGAMEVERSION_HOOK | PATCHSAVEGAMEVERSION | no_event_bridge |
| 0x437d9f | PATCHSAVEGAMEWORLD_HOOK | PATCHSAVEGAMEWORLD | no_event_bridge |
| 0x68b9d0 | TELEMETRY_ONTRADELEFTHOOK | INIT_ITEMIMPORTANTFLAG | no_event_bridge |
| 0x68bba6 | TELEMETRY_ONTRADERIGHTHOOK | INIT_ITEMIMPORTANTFLAG | no_event_bridge |
| 0x68b840 | ITEMIMPORTANTFLAG_CHECKSELLINGITEMS | INIT_ITEMIMPORTANTFLAG | no_event_bridge |
| 0x44e056 | ZERROR_SETTARGET_HOOK | STARTLOGFILE | no_event_bridge |
| 0x42a2d0 | UPDATELOADINGTEXTURE | INITALTERNATIVELOADINGSCREENS | no_event_bridge |
| 0x7323c0 | _EVT_QUIVER_EQUIP | INIT_QUIVERS_GAMESTART | no_event_bridge |
| 0x7326c0 | _EVT_QUIVER_UNEQUIP | INIT_QUIVERS_GAMESTART | no_event_bridge |
| 0x7a9240 | ZCVIEW__DIALOGMESSAGECXY_HOOK | CUTSCENESYSTEM_INIT | no_event_bridge |
| 0x6c31d6 | OCGAME__UPDATEPLAYERSTATUS_REPLACECHECKINGGLOBALCUTSCENE | CUTSCENESYSTEM_INIT | no_event_bridge |
| 0x42abe7 | CGAMEMANAGER__MENUENABLED_REPLACECHECKINGGLOBALCUTSCENE | CUTSCENESYSTEM_INIT | no_event_bridge |
| 0x69b9f1 | OCAIHUMAN__MOVING_REPLACECHECKINGGLOBALCUTSCENE | CUTSCENESYSTEM_INIT | no_event_bridge |
| 0x7a6060 | INVTRANSPARENCYHACK_HOOK | INIT_INVTRANSPARENCYHACK | no_event_bridge |
| 0x6c5250 | TORCHLOADFIX_CHECKTORCH | INIT_FIXTORCHESAFTERLOAD_GAMESTART | no_event_bridge |
| 0x485fc4 | SPELL_PICKLOCK_FOCUS | SPELL_PICKLOCK_INIT_GAMESTART | no_event_bridge |
| 0x735fab | OCNPC__DROPUNCONSCIOUS_HOOK | INIT_DROPUNCONSCIOUS_HOOK | no_event_bridge |
| 0x42926a | APPLYSOMESETTINGSEND_HOOK | INIT_SIGHTSCALING | no_event_bridge |
| 0x660bb0 | CRAFTINGVIEW_UPDATE | INIT_CRAFTINGVIEW_GAMESTART | no_event_bridge |
| 0x762250 | LOCKINVENTORY | INIT_FEATURES_GAMESTART | no_event_bridge |
| 0x751af0 | EV_FINISHINGMOVE_HOOK | INIT_FEATURES_GAMESTART | no_event_bridge |
| 0x4344a3 | HANDLERESULTSTRING_MARVINMODE_REPLACEHOOK | INIT_FEATURES_GAMESTART | no_event_bridge |
| 0x7538c0 | OCNPC__EVDROPVOB_HOOK | INIT_FEATURES_GAMESTART | no_event_bridge |
| 0x660bb0 | OCINFORMATIONMANAGER__UPDATE_HOOK | INIT_FEATURES_GAMESTART | no_event_bridge |
| 0x68eba0 | ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK | INIT_FEATURES_GAMESTART | no_event_bridge |
| 0x706e40 | OCITEMCONTAINER__DRAWITEMINFO_HOOK | INIT_FEATURES_GAMESTART | no_event_bridge |

## 2. Adresy bez handlera i implementacje pozorne

### 2.1. Brak handlera

56 różnych adresów, 66 miejsc wywołania w DAT. Nie są to liczby wykonań podczas rozgrywki.

| Adres | Funkcje wywołujące | Miejsca | Szczegół |
| --- | --- | --- | --- |
| 0x465380 | GETPARMVALUE | 1 |  |
| 0x475a50 | SPELL_CAST_SLOWDOWN | 1 |  |
| 0x4b20f0 | DIACAM_UPDATE | 1 |  |
| 0x4ed7a0 | STOPALLACTIVESOUNDS, STOPALLSOUNDS | 2 |  |
| 0x576690 | ANIISACTIVE | 1 |  |
| 0x5a9ce0 | APPLYHOUSEWALLTEXTURE | 1 |  |
| 0x5d3cf0 | ZCRENDERER_DRAWTILE | 1 |  |
| 0x5f54d0 | APPLYHOUSEWALLTEXTURE | 1 |  |
| 0x5f5530 | APPLYHOUSEWALLTEXTURE | 1 |  |
| 0x606ad0 | ZCVISUAL_LOADVISUAL | 1 |  |
| 0x644ef0 | ZRND_XD3D_SETRENDERSTATE | 1 |  |
| 0x64ac30 | ZRND_D3D_DRAWPOLYSIMPLE | 1 |  |
| 0x64d8e0 | ZRND_D3D_DRAWLINE | 1 |  |
| 0x652610 | ZRND_D3D_SETALPHABLENDFUNC | 1 |  |
| 0x688a70 | Q204_ARAXOSJOINTEST_COLLISION, Q304_TRAPCUTSCENE_SETCHASMDETECTION_ONFRAME | 4 |  |
| 0x6b6490 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK2 | 1 |  |
| 0x6d0000 | ZCVOB_SETCOLL | 1 |  |
| 0x702910 | SETDIALOGUETOLDPROPERTY | 1 |  |
| 0x709630 | CATINV_DELETELISTSORTFROMPOOL | 1 |  |
| 0x70a150 | CATINV_SWITCHCONTAINER | 1 |  |
| 0x70ade0 | CATINV_RESET | 1 |  |
| 0x70b570 | CATINV_RESET | 1 |  |
| 0x7110a0 | CATINV_CONTAINERINSERT, CATINV_NPCINVINSERT | 2 |  |
| 0x711120 | CATINV_RIGHT, CATINV_SETMAXOFFSET | 2 |  |
| 0x713ac0 | OCITEM_RENDER | 1 |  |
| 0x7326c0 | OCNPC_UNEQUIPITEM | 1 |  |
| 0x7375e0 | OCNPC__DROPUNCONSCIOUS_HOOK | 1 |  |
| 0x739c90 | OCNPC_EQUIP, TORCH_EQUIP | 2 |  |
| 0x73d340 | OCNPC_ISSPELLACTIVE | 1 |  |
| 0x7449c0 | OCAIHUMAN__PC_DIVING_HOOK | 1 |  |
| 0x749cb0 | OCNPC_PUTINSLOT | 1 |  |
| 0x74a270 | OCNPC_REMOVEFROMSLOT | 1 |  |
| 0x74a590 | OCNPC_DROPFROMSLOT | 1 |  |
| 0x74ef30 | NPCEQUIPBESTWEAPONBYTYPE | 1 |  |
| 0x762410 | CLOSEINVENTORY | 1 |  |
| 0x762b40 | CATINV_PREVENTCLOSEDEADINV | 1 |  |
| 0x763110 | OCNPC__EVDROPVOB_HOOK | 1 |  |
| 0x7813f0 | CATINV_CONTAINERREMOVE, CATINV_NPCINVREMOVE | 2 |  |
| 0x7894a0 | PRINT_GETFONTNAME | 1 |  |
| 0x7a5e80 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7a6060 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7a6070 | CATINV_DRAWCATEGORY | 2 |  |
| 0x7a63c0 | CATINV_DRAWCATEGORY | 2 |  |
| 0x7a75b0 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7a9a10 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7aa030 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7aa140 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7abad0 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7abd10 | CATINV_DRAWCATEGORY | 1 |  |
| 0x7b4870 | GLOBALLOCK | 1 |  |
| 0x7b4876 | GLOBALALLOC | 1 |  |
| 0x7b4a5c | CLOSECLIPBOARD | 1 |  |
| 0x7b4a62 | SETCLIPBOARDDATA | 1 |  |
| 0x7b4a68 | EMPTYCLIPBOARD | 1 |  |
| 0x7b4a6e | OPENCLIPBOARD | 1 |  |
| 0x7d0f48 | CATINV_DELAYMOBCAMERA | 1 |  |

### 2.2. Zarejestrowane, ale bez rzeczywistej operacji

7 różnych adresów, 9 miejsc wywołania w DAT. Nie są to liczby wykonań podczas rozgrywki.

| Adres | Funkcje wywołujące | Miejsca | Szczegół |
| --- | --- | --- | --- |
| 0x4cc470 | CATINV_KEYBINDINGISTOGGLED | 1 | IsBindedToggled -> 0 |
| 0x4d4d30 | CATINV_DELAYMOBCAMERA | 1 | GetState -> 0 |
| 0x4d55d0 | SPELL_PICKLOCK_CLEARKEYBUFFER | 1 | ClearKeyBuffer nop |
| 0x5f36a0 | ZCTEXTURE_LOAD | 1 | zCTexture::Load -> 0 |
| 0x731f90 | TORCH_GETSLOTITEM | 1 | GetSlotItem -> 0 |
| 0x73a030 | EQUIPWEAPON, OCNPC__DROPUNCONSCIOUS_HOOK | 3 | EquipWeapon nop |
| 0x782ae0 | CC_AUTOCOMPLETE | 1 | Console::Register nop |

## 3. Wszystkie historycznie niepowiązane externale

Z 86 deklaracji 10 ma bezpośrednie wywołania w bytecode; dziewięć z tych dziesięciu ma już implementacje. Pozostałe 76 deklaracji bez rozpoznanych wywołań BE nie jest listą 76 potwierdzonych blockerów. Nie wyklucza to wywołań pośrednich. Dla dziewięciu wykonanych funkcji zakres ręcznej weryfikacji znajduje się w handoffie głównym.

| External | Miejsca BE | Bieżąca interpretacja | Przykładowe funkcje |
| --- | --- | --- | --- |
| AI_WAITTILLEND | 1577 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | TRIA_WAIT;TRIA_WAIT;TRIA_WAIT;_TRIA_INITNPC |
| MOB_CREATEITEMS | 123 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | B_TRANSFERALLINVENTORYTOMOB;USE_KQ406_HUXLAYNOTE;USE_KQ406_HUXLAYNOTE;SQ509_SPAWNSCAVENGERS |
| WLD_REMOVEITEM | 12 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | KQ402_PREPARELENNART;KQ402_PREPARELENNART;SQ202_EXCHANGEGOODS;SQ202_EXCHANGEGOODS |
| AI_AIMAT | 10 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | DIA_SAMBOR_Q602_STOP_INFO;GQ001_GONZALODEATHCUTSCENE_START;GQ001_GONZALODEATHCUTSCENE_START;Q205_TILLCROSSBOWATTACK_START |
| AI_SHOOTAT | 10 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | DIA_THIAGO_HEYYOU_KILLPLAYER;GQ001_GONZALODEATHCUTSCENE_START;Q205_TILLCROSSBOWATTACK_START;Q304_KILLGONZALO |
| AI_SND_PLAY | 5 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | B_GIVEINVITEMS;Q514_UNLOCKLADDER_FADESCREEN;Q514_UNLOCKLADDER_FADESCREEN;DIA_RUUD_SCARED_MAGE |
| AI_DROPITEM | 3 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | DIA_VOLKERGUARD_YOUNG_SQ505_STOP_OLD_GOLD_TAKEIT;DIA_WOLFSON_EVT_TRIAL;SQ304_FINISHCUTSCENE_START |
| AI_STOPAIM | 2 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | Q406_KESSELSHOOT;Q406_KESSELSHOOT |
| WLD_INSERTOBJECT | 2 | DO ZROBIENIA — wywoływany external | MEM_INSERTVOB;MEMINT_VERSIONERROR |
| AI_STOPFX | 1 | ZAIMPLEMENTOWANE w bieżącym checkoutcie | USE_SQ509_PLANT |
| PRINTMULTI | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| SETPERCENTDONE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_MEMORYENTRY | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_MEMORYENTRYGUILD | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_HASNEWS | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GETNEWSWITNESS | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GETNEWSOFFENDER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GETNEWSVICTIM | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISNEWSGOSSIP | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_DELETENEWS | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GETGUILDATTITUDE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| WLD_DETECTPLAYER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| WLD_DETECTNPCEXATT | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_CREATESPELL | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_LEARNSPELL | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GIVEITEM | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_HASDETECTEDNPC | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GETCOMRADES | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISNEAR | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_KNOWSPLAYER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_SETKNOWSPLAYER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISAIMING | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_STARTITEMREACTMODULES | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_HASOFFERED | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_PLAYCUTSCENE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_CANSEENPC | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_GOTOSOUND | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_AREWESTRONGER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISNEXTTARGETAVAILABLE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISVOICEACTIVE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GIVEINFO | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| TAL_CONFIGURE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISWAYBLOCKED | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_WHIRLAROUNDTOSOURCE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_TURNTOSOUND | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_QUICKLOOK | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_DEFEND | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_COMBATREACTTODAMAGE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_ASK | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_ASKTEXT | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_WAITFORQUESTION | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_SND_PLAY3D | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| SND_GETDISTTOSOURCE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| SND_ISSOURCENPC | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| SND_ISSOURCEITEM | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| MIS_GETSTATUS | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| MIS_SETSTATUS | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| MIS_ONTIME | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| MIS_ADDMISSIONENTRY | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| MIS_REMOVEMISSION | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_CHECKAVAILABLEMISSION | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_CHECKRUNNINGMISSION | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_CHECKOFFERMISSION | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_PLAYFX | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| WLD_INSERTNPCANDRESPAWN | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_OWNEDBYGUILD | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_TAKEMOB | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| AI_DROPMOB | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| DOC_OPEN | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| DOC_FONT | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| DOC_PRINT | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| DOC_MAPCOORDINATES | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| TA | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| TA_CS | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| TA_BEGINOVERLAY | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| TA_ENDOVERLAY | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| TA_REMOVEOVERLAY | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| RTN_EXCHANGE | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| WLD_ASSIGNROOMTONPC | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| WLD_GETPLAYERPORTALOWNER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| WLD_GETFORMERPLAYERPORTALOWNER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_ISPLAYERINMYROOM | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_WASPLAYERINMYROOM | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| NPC_GETPORTALOWNER | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| HLP_CUTSCENEPLAYED | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |
| GAME_INITENGINTL | 0 | Brak bezpośrednich BE; ustalić potrzebę przed implementacją |  |

## 4. Czego te tabele nie rozstrzygają

- 40 miejsc CALL__* z dynamicznym wyrażeniem oraz 4 dynamiczne/wewnętrzne rejestracje hooków wymagają analizy wykonania lub dokładniejszej analizy przepływu danych; w tej sesji nie wykonywano ich automatycznie.
- W źródłowych CSV istnieją także działające lub zastąpione wywołania. Nie usuwaj ich mostów na podstawie samego nieznanego adresu spotkanego w starej dekompilacji.
- Aliasy pól, stabilność uchwytów, czas, bezpieczeństwo Mem32, kontrakty stosu, FF/PermMem, pełne zapisy i regresja kampanii to oddzielne zadania opisane w handoffie głównym, nie tylko brakujące adresy.
- Puste handler-y RemoveVob/SetVobName/ApplySomeSettings są również opisane w audycie głównym; nie wszystkie muszą występować w rozpoznanych bezpośrednich CALL__* tego DAT.
- Przechodniość wszystkich rozdziałów, gildii, zakończeń i pobocznych questów nie została potwierdzona. Liczby z CSV nie dają procentu grywalności.

Źródła: `docs/archolos-audit/dat-hooks.csv`, `dat-native-calls.csv`, `unbound-externals.csv`, `source-hooks.csv`, `summary.json` w workspace `C:/Users/Ernest/Desktop/opengothic`.
