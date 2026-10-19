# Kroniki Myrtany — ręczne sprawdzanie zmian

Ta instrukcja dotyczy pierwszego pakietu zmian po audycie. Nie jest raportem z wykonanych testów. Agent kompiluje projekt i czyta kod, ale nie uruchamia gry, testów automatycznych ani opisanych niżej poleceń.

Najnowszy wynik użytkownika z 20.10.2026: **„oba działają”**, w kontekście prób G (strzelanie) i H (scena po poprawce lądowania). Zachowany log potwierdza utworzenie jednego pocisku kuszy i zużycie jednego bełtu. To nie potwierdza wszystkich odmian broni, trafień ani całej kampanii. Pełny stan i dalsza praca: [ARCHOLOS_HANDOFF_CLAUDE.md](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_CLAUDE.md). A/B oraz ponowna próba imienia psa pozostają bez osobnego jednoznacznego potwierdzenia.

Wyniki przekazane przez użytkownika 20.10.2026: C–F działają; B blokuje sterowanie na około cztery sekundy i odblokowuje, ale bez widocznego napisu; A dało niepewny, niepowtarzalny wynik. G i H nie były wykonane z powodu braku przygotowanej sceny/NPC. W komendach A–C poprawiono pozycję napisów z `100,100` (poza widocznym obszarem) na wyśrodkowaną pozycję na 35% wysokości, wydłużono czas wyświetlania do 10 sekund i dodano numer próby. Nie jest to potwierdzenie ponownych prób A/B.

**Build komend G/H i poprawki napisów: zakończony powodzeniem 20.10.2026.** Zlinkowane `Gothic2Notr.exe` i `Spacer.exe`, zaktualizowany wbudowany mod Developer tools. Gry, komend kontrolnych i testów automatycznych nie uruchamiano.

Kolejna poprawka po zgłoszeniu: `Cannot position BAU_13324_SMOKER` / `BAU_13323_SMOKER` wynikało z błędnej interpretacji wyniku `Npc::setPosition`: `false` oznaczało również „już stoi dokładnie tutaj”. API modów `setPosition` i `teleport` uznaje teraz taki przypadek za powodzenie. Postać utworzona przez komendę była już w docelowym miejscu, więc próba H przerywała przygotowanie. Sam brak ukończonego intra nie wyjaśnia tego błędu; komenda sprawdza obecność punktu w aktualnym świecie i tworzy brakujących aktorów.

**Build tej poprawki i licznika pocisków również zakończony powodzeniem 20.10.2026** (`Gothic2Notr.exe`, `Spacer.exe`). Próby w grze pozostają do wykonania przez użytkownika.

Po kolejnym zgłoszeniu H: komunikat `Prepared...` pojawiał się, ale bohater wpadał pod mapę. Dodano `debug.placeOnGround`: wyznaczanie podłoża pod każdym przesuniętym miejscem, sprawdzanie nachylenia i wolnego miejsca, wyzerowanie prędkości spadania oraz reset kamery. Brak bezpiecznego podłoża przerywa teleport zamiast używać surowej wysokości waypointa. **Build poprawki lądowania zakończony powodzeniem 20.10.2026**, zlinkowane oba EXE; agent nie uruchamiał gry ani prób. Ponowną próbę H wykonaj na pierwotnym zapisie sprzed wpadnięcia pod mapę.

**Build pierwszego pakietu: zakończony powodzeniem, kod wyjścia 0.** Zlinkowane `Gothic2Notr.exe` i `Spacer.exe` w `build-touch/opengothic`. Stan rozgrywki: do ręcznego sprawdzenia przez użytkownika.

Poprawka po zgłoszeniu powrotu z ładowania do menu: `Wld_RemoveItem` zwraca teraz `int`, zgodnie z deklaracją w DAT (1 po usunięciu luźnego przedmiotu, 0 jeśli go nie usunięto). Poprzednia rejestracja jako `void` przerywała inicjalizację VM komunikatem `external WLD_REMOVEITEM has illegal return type 'void', expected 'int'`. Po tej poprawce najpierw sprawdź samo rozpoczęcie nowej gry; dopiero po wejściu do świata wykonuj kolejne próby.

## Poprawka wpisywania imienia psa — sprawdź teraz

**Build poprawki: zakończony powodzeniem.** Zlinkowane `Gothic2Notr.exe` i `Spacer.exe`; gry ani testów nie uruchamiano.

Poprawka zachowuje oczekujące zwolnienia klawiszy przy kolejnych zmianach układu sterowania dotykowego. Wcześniej mogło zginąć zwolnienie przycisku akcji (Ctrl), przez co pole tekstowe odrzucało litery. Klawisze dotykowe mają teraz osobne identyfikatory od fizycznych. Zamknięcie rozmowy czyści też stan pola tekstowego.

1. Po udanym buildzie uruchom grę od nowa i wczytaj zapis sprzed nadania imienia. Restart jest potrzebny, żeby nie przenieść zablokowanego stanu Ctrl z poprzedniego uruchomienia.
2. Rozpocznij rozmowę z psem przyciskiem akcji i wybierz wpisywanie imienia. Zwolnij fizyczny Ctrl, jeśli go używałeś.
3. Na fizycznej klawiaturze wpisz `Rek`, naciśnij Backspace i dopisz `x`. Pole powinno pokazywać kolejno `Rek`, `Re`, `Rex`. Zatwierdź Enterem. Rozmowa powinna przejść dalej, a skrypt dostać wpisane imię.
4. Na osobnej próbie (ponownie wczytaj zapis) wpisz `rex` klawiaturą ekranową. Powinno pojawić się `Rex`. Sprawdź ekranowe DEL i OK.
5. Na osobnej próbie wpisz kilka liter i naciśnij Escape. Otwórz pole jeszcze raz: powinno być puste i przyjmować litery.
6. Po zamknięciu rozmowy sprawdź chodzenie, przycisk akcji i otwarcie ekwipunku. Żaden klawisz nie powinien pozostać wciśnięty.

Jeśli litery nadal się nie pojawiają, podaj, czy problem dotyczy klawiatury fizycznej, ekranowej czy obu, i dołącz fragment `build-touch/opengothic/log.txt` od `DialogMenu: text prompt` do ostatniego `DialogMenu: prompt key`. Wpisy zawierają teraz również maskę modyfikatorów i długość tekstu przed obsługą klawisza.

## Zakres pierwszego pakietu

- Dziewięć externali: `AI_WaitTillEnd`, `Mob_CreateItems`, `Wld_RemoveItem`, `AI_AimAt`, `AI_ShootAt`, `AI_StopAim`, `AI_DropItem`, `AI_Snd_Play`, `AI_StopFX`.
- Oczekiwanie na znacznik końca poleceń już znajdujących się w kolejce drugiego NPC. Wzajemne oczekiwanie nie wymaga opróżnienia całej kolejki drugiej postaci; anulowanie jej poleceń zwalnia oczekujących.
- `AI_Function`: właściwy wykonujący NPC w `SELF`, wyzerowane `OTHER`/`ITEM`, odtworzenie poprzednich instancji po wywołaniu, odczyt parametrów `I/N/S`, pustych ciągów i sekwencji `STR_Escape`, wywołania externali i nadpisanych funkcji.
- `debug.call` wywołuje również externale i funkcje nadpisane. `debug.symbol` udostępnia indeks symbolu, a `debug.containers` nazwy pobliskich skrzyń.
- Polecenia `comcheck` służą wyłącznie do ręcznych prób. Samo uruchomienie moda nie wykonuje prób i nie zmienia stanu gry.

**Zapis ma wersję 56.** Dodane są liczniki synchronizacji kolejek. Odczyt starych zapisów pozostaje w kodzie, ale nowego zapisu z kolejkami nie należy otwierać starszym EXE. Korzystaj z oddzielnego slotu przeznaczonego na te próby.

`Wld_InsertObject`, stabilizacja pamięci obiektów oraz kolejne mechaniki z audytu nie należą jeszcze do tego pakietu. Historyczne CSV z audytu opisują stan sprzed zmian.

## Uruchomienie

1. Zapisz dotychczasową grę i całkowicie zamknij `Gothic2Notr.exe`, aby linker mógł podmienić plik.
2. Po potwierdzonym udanym buildzie uruchom z katalogu `C:/Users/Ernest/Desktop/opengothic/build-touch/opengothic`:

   ```powershell
   .\Gothic2Notr.exe -g "C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana" -game:TheChroniclesOfMyrtana.ini
   ```

3. Wczytaj kopię zapisu do prób albo zacznij nową grę. Dla pierwszego sprawdzenia wybierz zwykłą grę solo, bez aktywnego połączenia co-op.
4. Naciśnij **F10 → Console**, wpisz `comcheck`. Powinna pojawić się lista poleceń. Jeśli konsola nie zna polecenia, sprawdź, czy uruchomiłeś nowy EXE z katalogu `build-touch/opengothic` i czy mod **Developer tools** jest włączony. Po udanym linkowaniu build kopiuje też aktualny katalog `mods`.
5. Log bieżącego uruchomienia: `C:/Users/Ernest/Desktop/opengothic/build-touch/opengothic/log.txt`. Możesz też oglądać go przez **F10 → Debug → Log**.

## A. Parametry i zewnętrzna funkcja uruchomiona przez AI

1. Stań na ziemi, zamknij dialog i ekwipunek.
2. W **F10 → Console** wpisz:

   ```text
   comcheck text
   ```

3. Zamknij F10 i poczekaj około sekundy czasu gry.

**Oczekiwane:** na środku ekranu, na 35% jego wysokości, pojawia się `COM text with spaces: OK #N`, ze spacjami zamiast `\_`, na około dziesięć sekund. Postać może następnie normalnie chodzić. Wykonaj polecenie drugi raz: powinien pojawić się nowy numer. Próba sprawdza kolejkę, argumenty różnych typów, odkodowanie tekstu i wywołanie externala `AI_PrintScreen` przez `AI_Function`.

**Zgłoś błąd**, jeśli tekstu nie ma, występują dosłowne `\_`, postać blokuje się lub pojawia się błąd VM. Ten wynik nie potwierdza jeszcze wszystkich scen używających `SELF` — do tego służy próba H.

## B. Wzajemne oczekiwanie dwóch postaci

1. Wybierz spokojną okolicę z żywym, bezczynnym NPC w odległości poniżej 15 metrów. Nie uruchamiaj próby podczas walki, rozmowy ani pracy NPC przy obiekcie. Polecenie wybiera najbliższą żywą postać i czyści jej bieżącą kolejkę.
2. Wpisz:

   ```text
   comcheck wait
   ```

3. Konsola poda imię wybranej postaci. Zamknij F10.

**Oczekiwane:** `COM_WAIT_DONE #N` pojawia się na środku, na 35% wysokości, po około czterech sekundach **upływającego czasu gry** i jest widoczny przez dziesięć sekund. Nie powinien pojawić się od razu. Po pięciu–sześciu sekundach aktywnej gry obie postacie powinny być odblokowane. Czas spędzony w pauzie nie jest czasem gry.

Polecenie układa sekwencję `bohater → NPC`, `NPC → bohater`, `bohater → NPC`. To sprawdza cykl oczekiwań stosowany przez `TRIA_WAIT`.

## C. Anulowanie oczekiwanych poleceń

1. Pozostań przy spokojnym NPC.
2. Wpisz `comcheck cancel` i zamknij F10.

**Oczekiwane:** `COM_CANCEL_DONE` pojawia się niemal natychmiast, po kilku klatkach. Nie czekasz 30 sekund na anulowane polecenie NPC. Brak komunikatu lub trwała blokada oznaczają błąd zwalniania znacznika.

## D. Zapis w trakcie oczekiwania i pełny restart

1. Przy spokojnym NPC wpisz:

   ```text
   comcheck wait 20
   ```

2. Zamknij F10, odczekaj dwie–trzy sekundy i zapisz grę w oddzielnym slocie.
3. Zamknij cały proces gry. Uruchom ten sam nowy EXE i wczytaj ten zapis.
4. Pozwól grze działać przez pozostały czas, maksymalnie około 20 sekund.

**Oczekiwane:** oczekiwanie kończy się, pojawia się `COM_WAIT_DONE`, a sterowanie wraca. Nie powinno kończyć się natychmiast tylko dlatego, że licznik został zgubiony przy zapisie. Po załadowaniu dawny NPC ma pozostać celem synchronizacji, nawet jeśli kolejność postaci w świecie jest inna.

Osobno wczytaj zapis zrobiony **przed** tym pakietem zmian i wykonaj A–C. To sprawdza odczyt poprzedniego formatu.

## E. Przedmiot wyrzucany przez AI

1. Stań na płaskiej ziemi, bez wyciągniętej broni i bez interakcji z obiektem.
2. Sprawdź liczbę jabłek w ekwipunku.
3. Wpisz `comcheck drop`, zamknij F10 i poczekaj na animację.

**Oczekiwane:** polecenie dodaje jedno `ITFO_APPLE`, następnie bohater wyrzuca dokładnie jeden przedmiot na ziemię. W ekwipunku zostaje początkowa liczba jabłek. Podnieś przedmiot i sprawdź, że przybywa jedno jabłko.

Możesz wskazać inną znaną instancję: `comcheck drop ITFO_APPLE`. Błąd `Unknown symbol` oznacza inną nazwę instancji w danych gry; nie dowodzi błędu samego wyrzucania.

## F. Tworzenie przedmiotów w skrzyni

1. Podejdź do skrzyni, którą możesz otworzyć, na odległość poniżej pięciu metrów. Jeśli obok jest kilka skrzyń, polecenie wybierze najbliższą według jej pozycji w świecie.
2. Otwórz ją i zanotuj liczbę jabłek; zamknij okno skrzyni.
3. Wpisz `comcheck chest`.
4. W konsoli zobaczysz nazwę voba oraz `before=N, after=M; expected increase=3`.
5. Zamknij F10 i ponownie otwórz właściwą skrzynię.

**Oczekiwane:** `M=N+3`, w środku są trzy dodatkowe jabłka. Zapisz i wczytaj grę; jabłka mają pozostać w skrzyni.

Pełna postać polecenia, jeśli znasz nazwę voba:

```text
comcheck chest NAZWA_SKRZYNI ITFO_APPLE 3
```

Komunikat `container not found` / brak pobliskiej skrzyni oznacza, że nie znaleziono wskazanego obiektu — podaj nazwę z komunikatu i miejsce w świecie.

## G. Celowanie, strzał, zatrzymanie celowania

Wykonuj na kopii zapisu, na otwartej, płaskiej ziemi, poza dialogiem. Nie potrzebujesz szukać strzelca. Wszystkie polecenia dotyczą jednego zapamiętanego NPC, a nie najbliższego przechodnia.

1. Wpisz `comcheck ranged`, zamknij F10 na dwie–trzy sekundy. Powstaje `Tester Animacji` (`NONE_34813481_TREETESTER`) około 4,5 metra od bohatera, z kuszą `ITRW_MIL_CROSSBOW` i 20 bełtami. Komenda wyłącza rutynę i percepcje wyłącznie tej postaci, przygotowuje atrybuty i włącza nieśmiertelność gracza. NPC pozostaje zwykłą postacią silnika, która wykonuje kolejkę AI.
2. Wpisz `comcheck ranged status`. Oczekiwane: `weaponState=5` (kusza), `ranged=ITRW_MIL_CROSSBOW`, `ammo=20`, `projectiles=0`. Jeśli broń nie jest jeszcze wyciągnięta, zamknij F10 i odczekaj.
3. Wpisz `comcheck ranged aim`, zamknij F10. NPC powinien obrócić się i wycelować bez strzału. Ponowne `comcheck ranged status` ma pokazać nadal `ammo=20`.
4. Wpisz `comcheck ranged shoot`, zamknij F10 i odczekaj trzy sekundy bez wpisywania innych komend. Oczekiwany jeden strzał; `comcheck ranged status` ma pokazać `ammo=19`, `projectiles=1` oraz `Last shoot: created=1, ammo spent=1. PROJECTILE CONFIRMED`. Licznik rośnie dopiero po utworzeniu obiektu pocisku przez silnik, niezależnie od odejmowania amunicji. Nie potwierdza trafienia ani widoczności modelu pocisku. Przy odległości 4,5 m i prędkości pocisku 30 m/s lot trwa około 0,15 s, więc łatwo go przeoczyć. Kolejne wywołanie strzału powinno dać `projectiles=2`, `ammo=18`, a wynik ostatniej próby nadal `created=1, ammo spent=1`. Nowe aim/shoot/stop czyści poprzednią kolejkę: nie wykonuj ich, zanim strzał zdąży się odbyć.
5. Wpisz `comcheck ranged stop`, zamknij F10. NPC ma opuścić celowanie, zachowując wyciągniętą broń; liczba amunicji pozostaje bez zmian.
6. Wpisz `comcheck ranged clear`. NPC znika, a nieśmiertelność wraca do ustawienia sprzed próby.
7. Powtórz z `comcheck ranged bow`: łuk `ITRW_BOW_L_01`, 20 strzał, `weaponState=4`. Kroki aim/shoot/stop/status/clear są takie same.

Po aim/shoot komenda dodaje 120 sekund czekania w kolejce NPC, żeby zwykła walka nie dodała samoczynnych strzałów. Następne polecenie czyści to oczekiwanie. Zakończ próbę przez `clear`; po wczytaniu zapisu przygotuj strzelca od nowa. Podczas przeglądu kodu poprawiono odwrócony warunek końca obrotu w `AI_AimAt`/`AI_ShootAt` oraz wyczyszczenie celu w `AI_StopAim`.

Licznik `rangedShots` jest diagnostyką bieżącego obiektu NPC, nie jest zapisywany w savie i nie korzysta z pobieranej przez co-op listy `world.takeShots()`.

## H. Naturalne dialogi i questy

Przygotowana próba: **dwaj palacze w Silbach**, dialog `DIA_13324_SMOKER_OHFUCK`. Skrypt ma warunek `TRUE`; po kwestii i oczekiwaniu zamyka rozmowę, a `AI_Function` wywołuje `DIA_13324_SMOKER_RUN`. Funkcja korzysta z `SELF`, zmienia mu rutynę na `RUN1` i zmienia rutynę drugiego palacza. To sprawdza wykonanie opóźnionej funkcji na właściwym NPC, po zamknięciu rozmowy.

1. Wczytaj kopię zapisu w głównym świecie Archolos (zawierającym Silbach). Zamknij aktualny dialog.
2. W F10 → Console wpisz `comcheck scene` (równoważnie `comcheck scene smokers`).
3. Komenda przenosi bohatera obok `VILLAGE_POORPLACE_18`, ustawia 22:00, umieszcza obu palaczy obok, usuwa ich stare stany AI i odtwarza rutynę `START`. Wysokość każdego lądowania wyznacza osobno z geometrii świata, sprawdza nachylenie i kolizje, zeruje prędkość spadania oraz ustawia kamerę przy bohaterze. Nie kopiuje wysokości waypointa na przesunięte miejsce. Gdy nie znajdzie miejsca, zgłasza `No safe ground` i pozostawia daną postać tam, gdzie była przed próbą jej teleportu. W konsoli wypisuje `Ground landing` z wysokościami lądowania bohatera, palaczy i waypointa. Ustawia punkt startu drugiego palacza (`RNG_TRAP20_WAYPOINT`), resetuje pamięć trzech dialogów i flagi `TRAP20_VANISH1/2`, wyłącza blokadę rozmów `FMQ001_BLOCKDIALOGUE`. Brakującą lub martwą postać tworzy ponownie. Na końcu kolejkuje wejście pierwszego palacza w oryginalny stan `ZS_TALK` z bohaterem jako rozmówcą.
4. Zamknij F10. Poczekaj na kwestię palacza i zamknięcie rozmowy. **Obaj palacze mają zacząć uciekać**, każdy zgodnie z własną rutyną: pierwszy w kierunku `PART12_PATH_94`, drugi w kierunku `PART10_PATH_03`. Bohater ma odzyskać sterowanie. Nie ścigaj ich i nie rozpoczynaj kolejnego dialogu, dopóki obaj nie zaczną biec.
5. Jeśli próba zawiedzie, podaj: czy po zamknięciu F10 bohater i obaj NPC stoją na ziemi, czy dialog się rozpoczął, czy dobiegł końca, który palacz ruszył, czy ruszył bohater zamiast NPC oraz fragment logu z `Ground landing`, `No safe ground`, `AI_PlayAni: CALL`/`DIA_13324_SMOKER_RUN`.
6. Po próbie wczytaj pierwotny zapis. Komenda zmienia godzinę, historię rozmów, pozycje i rutyny obu postaci; nie odtwarza całego stanu świata poleceniem cofania. Możesz ją powtórzyć na kolejnej kopii zapisu.

Źródła sceny w dostarczonej dekompilacji: `Source/DIA_13324_SMOKER_EXIT.d`, `Source/BAU_13324_SMOKER.d`, `Source/BAU_13323_SMOKER.d`. Używane są nazwy symboli z bieżącego DAT, nie stałe numery z dekompilacji.

W dalszej kolejności, na odpowiednich zapisach, pozostają do sprawdzenia:

- rozpoczęcie nowej gry, pierwsze rozmowy i scenę z kilkoma rozmówcami;
- dialog, w którym po kwestii NPC ma przejść, odwrócić się albo wykonać animację — czynność powinna wykonać właściwa postać;
- scenę przekazania przedmiotów do skrzyni i jej późniejsze otwarcie;
- gdy masz odpowiednie zapisy: sceny strzałów Q205, Q304 i Q406 oraz questowe usuwanie przedmiotów KQ402/SQ202;
- scenę z `AI_Snd_Play` i zakończenie efektu SQ509, jeśli masz taki zapis.

Pozostałe braki z audytu nadal mogą wpływać na te sceny. Dlatego przy zgłoszeniu podaj konkretny krok, który się rozjechał, zamiast jedynie „quest nie działa”.

## Co przekazać po próbach

Wystarczy taka lista:

```text
A tekst: OK / opis
B czekanie: komunikat po ... s / nie pojawił się
C anulowanie: OK / opis
D zapis i restart: OK / opis
E wyrzucanie: OK / opis
F skrzynia: nazwa voba, before=..., after=..., stan po wczytaniu
G łuk/kusza: imię NPC, broń, co zadziałało
H scena: nazwa zadania, ostatnia kwestia i błędna czynność
```

Przy błędzie dołącz odpowiedni fragment `log.txt`, w szczególności linie zawierające `Ikarus`, `AI_Function`, `not implemented`, `mem_`, `CALL STACK` lub nazwę nowej funkcji. Nie trzeba wykonywać wszystkich questowych prób, żeby zgłosić wcześniejszy, powtarzalny problem w A–F.
