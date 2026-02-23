### Hala widowiskowo-sportowa 

## 1. Ogólny opis projektu

Projekt polega na symulacji systemu obsługi kibiców na stadionie, działającego w środowisku Linux/UNIX z wykorzystaniem mechanizmów komunikacji międzyprocesowej.

System składa się z kilku współpracujących typów procesów: managera, kasjerów, techników oraz kibiców, które komunikują się ze sobą przy użyciu pamięci współdzielonej, kolejek komunikatów, semaforów oraz sygnałów systemowych.

Każdy kibic funkcjonuje jako niezależny proces, realizując cykl: wejście do systemu → zakup biletu → kontrola przy bramce → wejście na sektor → opuszczenie stadionu podczas ewakuacji. Przebieg tego cyklu zależy od dostępności miejsc w sektorach, liczby aktywnych kas, przynależności do drużyny, statusu VIP oraz losowych zdarzeń.

Liczba kasjerów jest dynamiczna i zależna od aktualnego obciążenia systemu oraz długości kolejek. Ograniczenia systemowe obejmują m.in. maksymalną pojemność sektorów, ograniczoną przepustowość bramek, możliwość blokowania sektorów oraz konieczność przeprowadzenia ewakuacji w sytuacjach awaryjnych.

Manager zarządza globalnym stanem systemu poprzez uruchamianie i zamykanie procesów kasjerów, wysyłanie sygnałów do pozostałych procesów oraz inicjowanie procedury ewakuacji. Technicy odpowiadają za kontrolę wejść do sektorów, obsługę kolejek przy bramkach oraz potwierdzanie opróżnienia sektorów. Kasjerzy realizują sprzedaż biletów i przydzielanie miejsc, natomiast kibice symulują zachowanie uczestników wydarzenia.

Cała symulacja generuje raport tekstowy zapisywany do pliku `raport.txt`, dokumentujący przebieg działania systemu, istotne zdarzenia oraz aktualne stany procesów.

Projekt ma na celu praktyczne wykorzystanie i zaprezentowanie mechanizmów zarządzania procesami, synchronizacji, komunikacji międzyprocesowej, obsługi sygnałów oraz pracy na zasobach systemowych w środowisku Linux/UNIX.

## 2. Ogólny opis kodu

Projekt został podzielony na kilka logicznie rozdzielonych plików źródłowych, z których każdy odpowiada za odrębny element symulowanego systemu. Współpraca pomiędzy modułami odbywa się za pośrednictwem mechanizmów IPC oraz wspólnych struktur danych zdefiniowanych w common.h.

Każdy plik realizuje określoną funkcję w strukturze programu i odpowiada za obsługę wybranego typu procesu lub mechanizmu systemowego. Współpraca pomiędzy modułami odbywa się za pośrednictwem mechanizmów IPC oraz wspólnych struktur danych.

Podział kodu na osobne pliki umożliwia niezależne rozwijanie poszczególnych elementów systemu, ogranicza powstawanie błędów wynikających z nadmiernych zależności oraz ułatwia analizę działania programu.

Takie podejście pozwala na zachowanie modularności projektu oraz zapewnia większą przejrzystość implementacji.

## 2.1 Struktura projektu

Projekt składa się z następujących plików źródłowych:

- `manager.c`  
  Główny proces systemu. Odpowiada za inicjalizację zasobów IPC, uruchamianie pozostałych procesów, zarządzanie kasami oraz nadzorowanie przebiegu symulacji i ewakuacji.

- `cashier.c`  
  Implementuje proces kasjera. Obsługuje sprzedaż biletów, sprawdza dostępność miejsc w sektorach oraz komunikuje się z kibicami za pomocą kolejek komunikatów.

- `tech.c`  
  Realizuje proces technika obsługującego bramki. Kontroluje wejścia do sektorów, zarządza kolejkami przy bramkach oraz nadzoruje opróżnianie sektorów podczas ewakuacji.

- `fan.c`  
  Implementuje proces kibica. Symuluje zachowanie uczestnika wydarzenia, w tym zakup biletu, wejście na stadion oraz reakcję na sytuacje awaryjne.

- `common.c`  
  Zawiera funkcje pomocnicze wykorzystywane przez wszystkie moduły, m.in. obsługę logowania, inicjalizację pamięci współdzielonej, kolejek komunikatów oraz semaforów.

- `common.h`  
  Plik nagłówkowy zawierający definicje struktur danych, stałych, typów komunikatów oraz deklaracje funkcji wspólnych dla całego projektu.

## 2.2 Zastosowane rozwiązania zwiększające wydajność i stabilność

W celu zapewnienia poprawnego, stabilnego i wydajnego działania systemu zastosowano następujące rozwiązania:

- **Synchronizacja dostępu do danych**  
  Do kontroli dostępu do pamięci współdzielonej wykorzystano semafory System V, co zapobiega jednoczesnej modyfikacji wspólnych struktur danych przez wiele procesów.

- **Asynchroniczna komunikacja międzyprocesowa**  
  Zastosowanie kolejek komunikatów umożliwia wymianę informacji pomiędzy procesami bez konieczności ich bezpośredniej synchronizacji.

- **Nieblokujące operacje IPC**  
  Wykorzystanie trybu `IPC_NOWAIT` zapobiega trwałemu blokowaniu procesów i zwiększa płynność działania symulacji.

- **Obsługa sygnałów systemowych**  
  Mechanizmy obsługi sygnałów pozwalają na bezpieczne reagowanie na zdarzenia awaryjne, takie jak zakończenie programu, blokada sektorów czy ewakuacja.

- **Dynamiczne zarządzanie zasobami**  
  Liczba aktywnych kasjerów jest dostosowywana do aktualnego obciążenia systemu, co pozwala na efektywne wykorzystanie dostępnych zasobów.

- **Kontrola przepustowości bramek**  
  Ograniczenie liczby jednocześnie obsługiwanych kibiców na bramkach zmniejsza ryzyko przeciążenia systemu.

- **Mechanizm priorytetów**  
  Wprowadzenie priorytetu dla wybranych kibiców zapobiega długiemu oczekiwaniu i poprawia realizm symulacji.

- **Rejestrowanie zdarzeń**  
  Wszystkie istotne operacje są zapisywane w pliku `raport.txt`, co ułatwia analizę działania systemu oraz wykrywanie błędów.

- **Kontrola poprawności działania procesów**  
  Zastosowanie obsługi sygnału `SIGCHLD` umożliwia usuwanie procesów zombie i poprawia stabilność systemu.

## 3. Zrealizowane elementy projektu

- **Kasy biletowe**
  
  - Zawsze działają minimum 2 stanowiska kasowe  
  - Dynamiczne otwieranie kas, gdy kolejka przekracza próg `(K/10) * N`  
  - Dynamiczne zamykanie kas, gdy kolejka spada poniżej `(K/10) * (N - 1)`  
  - Jeden kibic może kupić maksymalnie 2 bilety w tym samym sektorze  
  - Kasy automatycznie zamykane po sprzedaży wszystkich biletów  
  - Obsługa klientów VIP z pominięciem kolejki (limit `0,3% * K`)  
  - Bilety sprzedawane losowo przez wszystkie kasy  

- **Sektory i pojemność**

  - 8 sektorów dla kibiców o równej pojemności  
  - Dodatkowy sektor VIP  
  - Monitorowanie i blokowanie sprzedaży po osiągnięciu limitu sektora  

- **Kontrola bezpieczeństwa przy bramkach**

  - Osobne wejście do każdego z 8 sektorów  
  - 2 stanowiska kontrolne na każdym wejściu  
  - Maksymalnie 3 osoby jednocześnie na stanowisku  
  - Gwarancja, że kibice kontrolowani równocześnie należą do tej samej drużyny  
  - Mechanizm priorytetów — kibic może zostać przepuszczony maksymalnie 5 razy, po czym otrzymuje pierwszeństwo  
  - Osobne wejście dla VIP-ów bez kontroli bezpieczeństwa  
  - Dzieci poniżej 15. roku życia wpuszczane tylko pod opieką osoby dorosłej  
  - Wykrywanie i usuwanie kibiców z racami  

- **Sygnały i ewakuacja**
  
  - `SIGUSR1` — wstrzymanie wpuszczania kibiców do sektorów  
  - `SIGUSR2` — wznowienie wpuszczania kibiców  
  - `SIGTERM` — ewakuacja wszystkich kibiców ze stadionu  
  - Pracownik techniczny wysyła potwierdzenie do kierownika po opróżnieniu sektora  

- **Kierownik (manager)**
  
  - Inicjalizacja wszystkich zasobów IPC  
  - Uruchamianie i zamykanie procesów kasjerów, techników i kibiców  
  - Obsługa sygnałów i nadzorowanie przebiegu całej symulacji  
  - Automatyczna ewakuacja po zakończeniu meczu lub przekroczeniu pojemności  

- **Raport**
  
  - Zapis przebiegu symulacji do pliku `raport.txt`  
  - Synchronizowane logowanie z użyciem semaforów  

## 4. Napotkane problemy i trudności

Podczas realizacji projektu napotkano szereg problemów związanych z programowaniem współbieżnym oraz komunikacją międzyprocesową.

- **Ryzyko występowania zakleszczeń (deadlocków)**  
  Niewłaściwe użycie semaforów mogło prowadzić do sytuacji, w których procesy wzajemnie się blokowały. Problem ten został rozwiązany poprzez ujednolicenie kolejności blokowania zasobów.

- **Obsługa sygnałów systemowych**  
  Trudnością było zapewnienie poprawnej reakcji procesów na sygnały ewakuacji i zakończenia pracy bez utraty danych oraz pozostawienia zasobów w niepoprawnym stanie.

- **Zarządzanie zasobami IPC**  
  Konieczne było pilnowanie poprawnego tworzenia i usuwania pamięci współdzielonej, kolejek komunikatów oraz semaforów, aby uniknąć wycieków zasobów.

- **Synchronizacja kolejek przy bramkach**  
  Implementacja kolejek wejściowych wymagała dodatkowej kontroli, aby zapobiec utracie danych oraz niepoprawnej kolejności obsługi kibiców.

Napotkane trudności pozwoliły na zdobycie praktycznego doświadczenia w programowaniu współbieżnym oraz lepsze zrozumienie mechanizmów systemów operacyjnych.

## 5. Wyróżniające się elementy specjalne

Projekt zawiera kilka istotnych rozwiązań, które zwiększają jego funkcjonalność oraz poziom zaawansowania.

- **Dynamiczne zarządzanie kasjerami**  
  Automatyczne otwieranie i zamykanie kas w zależności od liczby oczekujących kibiców.

- **Obsługa klientów VIP**  
  Priorytetowa obsługa wybranych kibiców podczas zakupu biletów.

- **System priorytetów przy bramkach**  
  Przyznawanie pierwszeństwa kibicom długo oczekującym w kolejce.

- **Wykrywanie i usuwanie kibiców z racami**
  umożliwia wykrycie niebezpiecznych kibiców i ich usunięcie z systemu.

- **Obsługa kibiców niepełnoletnich**
  dzieci poniżej 15. roku życia mogą wejść wyłącznie pod opieką dorosłego, co jest weryfikowane przez kasjera

- **Synchronizowane logowanie zdarzeń**  
  Zabezpieczenie zapisu do pliku `raport.txt` przy użyciu semaforów.

- **Automatyczne proponowanie alternatywnego sektora**  
  Jeśli wybrany przez kibica sektor jest pełny, kasjer automatycznie wyszukuje wolne miejsce w innym sektorze tej samej drużyny i przydziela bilet tam,          zamiast odmawiać sprzedaży.
  
  ## 6. Opis semaforów

W projekcie zastosowano zestaw semaforów System V w celu synchronizacji dostępu do zasobów współdzielonych oraz zapewnienia poprawnej współpracy pomiędzy procesami.

Wykorzystywane semafory pełnią następujące funkcje:

- **Semafor 0 – synchronizacja logowania**  
  Odpowiada za kontrolę dostępu do funkcji zapisujących dane do pliku `raport.txt`, zapobiegając nakładaniu się komunikatów z różnych procesów.

- **Semafor 1 – kolejki bramkowe**  
  Chroni dostęp do struktur `gate_queue` przechowywanych w pamięci współdzielonej.  
  Używany przez techników i kibiców przy dodawaniu i usuwaniu elementów z kolejki.

- **Semafor 2 – ewakuacja i blokady sektorów**  
  Zabezpiecza odczyt i zapis flagi `evacuation` oraz tablicy `sector_blocked`.  
  Używany przez managera przy inicjowaniu ewakuacji oraz obsłudze sygnałów `SIGUSR1` / `SIGUSR2`.

- **Semafor 3 – dane globalne**  
  Chroni wszystkie liczniki ogólnosystemowe, takie jak:
  - liczba sprzedanych biletów,
  - liczba kibiców na stadionie,
  - liczba aktywnych kasjerów,
  - rozmiary kolejek do kas.

- **Semafory 4 + i – dane sektorów**  
  Każdy sektor posiada własny semafor, który chroni dane konkretnego sektora i (sector_taken, sector_tickets_sold, gate_count), umożliwiając jednoczesne         operacje na różnych sektorach bez wzajemnego blokowania procesów.
  
Zastosowanie oddzielnych semaforów dla poszczególnych obszarów systemu pozwala na ograniczenie liczby blokad, zwiększenie równoległości działania oraz poprawę wydajności symulacji.

  ## 7. Przeprowadzone testy

  # 7.1 Proponowanie wolnego sektora
  ![](test11.jpg)
  ![](test12.jpg)

  




