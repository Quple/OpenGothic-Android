# Safe GPU Mode, shader chmur na Mali i diagnostyka zapisów

## Crash `clouds_lut.comp` na Mali-G57

`vkCreateComputePipelines` kończyło się `SIGSEGV` w `libGLES_mali.so` przy `clouds_lut.comp.sprv`.
Oryginalny shader wywołuje `textureQueryLevels()` na samplerach przekazanych do funkcji jako argumenty
(`in sampler2D`). To jedyne zapytanie o stan samplera w tym shaderze i najbardziej prawdopodobny winowajca; **nie
potwierdzono tego na urządzeniu** (to wymaga Mali-G57), więc dodano obejście niezależne od potwierdzenia przyczyny.

- `shader/sky/clouds_lut_safe.comp` ma te same bindingi (0, 5, 6, 7, 8), ten sam rozmiar grupy i ten sam wynik.
  `textureQueryLevels()` zwraca liczbę poziomów, więc `textureLod(..., liczba_poziomów)` zawsze trafia poza ostatni
  poziom i sampler ogranicza go do ostatniego. Stała `20.0` (więcej poziomów niż ma jakakolwiek tekstura) daje ten sam poziom.
  Samplery nie są przekazywane do funkcji.
- `Shaders::cloudsLutShader()` wybiera wariant: każdy GPU z „Mali” w nazwie oraz tryb Safe GPU używają wariantu
  bezpiecznego, pozostałe GPU zostają przy oryginale. Jeśli sterownik odrzuci oryginał wyjątkiem, używany jest wariant bezpieczny.
- `try/catch` nie łapie `SIGSEGV`, dlatego crash sterownika jest zapamiętywany przez pliki (niżej).

## Safe GPU Mode (`common/graphics/safegpu.*`)

Pliki w katalogu roboczym gry (na Androidzie `getFilesDir()`, obok `log.txt` i `crash.log`):

| plik | znaczenie |
| --- | --- |
| `safegpu.lock` | istnieje, gdy sterownik kompiluje shadery; zawiera nazwę GPU i **nazwę ostatniego shadera** przekazanego sterownikowi |
| `safegpu.mode` | poziom trybu (`level=`), GPU (`gpu=`) i shader, który zabił poprzedni start (`shader=`) |

Przebieg:

1. Start: `SafeGpu::init()` czyta oba pliki. Jeśli `safegpu.lock` został z poprzedniego startu dla tego samego GPU, poprzedni
   start zginął w kompilatorze sterownika: poziom rośnie z 0 do 1 (warianty bezpieczne) i jest zapisywany w `safegpu.mode`.
2. Poziom 2 (`Minimal`): kolejny crash przy włączonym poziomie 1, potwierdzony świeżym `crash.log`, wyłącza też opcjonalne
   pipeline'y (bindless, ray tracing, mesh shadery, virtual shadows, software shadows).
3. Po udanej kompilacji wszystkich shaderów `safegpu.lock` jest usuwany. Kompilacja pipeline'ów materiałów w trakcie gry
   także zakłada blokadę.
4. Tryb jest przypisany do nazwy GPU. Inny GPU startuje w trybie normalnym.

Uwaga: zabicie aplikacji przez użytkownika w trakcie pierwszej kompilacji też zostawia blokadę i włącza poziom 1. Poziom 1 nie
zmienia obrazu (wariant `clouds_lut_safe` daje ten sam wynik), a poziom 2 wymaga potwierdzenia przez `crash.log`.

Parametr `-safegpu auto|off|on|minimal` (domyślnie `auto`). `off` usuwa oba pliki. Ręcznie: usuń `safegpu.mode`.

## Zapisy: diagnostyka i walidacja

Komunikat `[zenkit] 1 bytes overflowed in section f590` **nie dotyczy zapisów**. `f590` to `MODEL_TAG` w binarnym skrypcie
modelu (`.MSB`) czytanym przez ZenKit (`ModelScript.cc`); parser zeruje kursor na koniec sekcji, więc to ostrzeżenie
jest nieszkodliwe. Taki sam wiersz jest w `docs/archolos-audit/runtime-2026-10-20/log.txt`, z grą działającą poprawnie.

`unable to read save-game file` oznaczało, że odczyt wyszedł poza dane bieżącego wpisu ZIP albo wpisu w ogóle nie było.
Stary kod nie sprawdzał wyniku `mz_zip_reader_extract_file_to_mem` (uszkodzony wpis zamieniał się w zera lub w
niezainicjowany bufor), a brak wpisu dawał pusty bufor i anonimowy wyjątek. Teraz:

- `Serialize::implSetEntry()` sprawdza rozmiar, rozpakowanie i CRC; błąd zawiera nazwę wpisu, indeks, rozmiar,
  rozmiar spakowany i błąd miniz;
- `readBytes()` i walidacja liczników (`checkCount`) podają etap (`setStage`), wpis, offset, ile bajtów żądano i ile zostało,
  a zapis loguje to jako `save-game: ...`;
- `requireEntry()` dla wpisów, które zawsze są zapisywane (`header`, `game/session`, `game/quests`, `game/daedalus`,
  `game/camera`, `worlds/<świat>/{world,version,items,triggerEvents,routines}`, `npc/N/{data,visual}`);
- wersja zapisu spoza `Serialize::MinVersion..Current` jest odrzucana z komunikatem (zapis z nowszego programu);
- błąd ładowania trafia do `Gothic::loadError()`, a okno pokazuje go na ErrorScreen i wraca do menu głównego;
  wątek ładowania łapie także wyjątki spoza `std::exception`.

## Zamknięcie gry (`SIGABRT` w `MainWindow::~MainWindow`)

`setupUi()` dodaje `errorScreen` (składowa okna, nie obiekt na stercie) jako dziecko, a destruktor nie odpinał go przed
`removeAllWidgets()`. Ta funkcja woła `delete` na każdym pozostałym dziecku, więc `delete` na składowej kończył się
`SIGABRT` (nieprawidłowe zwolnienie pamięci). Destruktor odpina teraz `errorScreen`, jak pozostałe widgety.
