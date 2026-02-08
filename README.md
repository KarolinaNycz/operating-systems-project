## Hala widowiskowo-sportowa 

# 1. Ogólny opis projektu

Projekt polega na symulacji systemu obsługi kibiców na stadionie, działającego w środowisku Linux/UNIX z wykorzystaniem mechanizmów komunikacji międzyprocesowej.

System składa się z kilku współpracujących typów procesów: managera, kasjerów, techników oraz kibiców, które komunikują się ze sobą przy użyciu pamięci współdzielonej, kolejek komunikatów, semaforów oraz sygnałów systemowych.

Każdy kibic funkcjonuje jako niezależny proces, realizując cykl: wejście do systemu → zakup biletu → kontrola przy bramce → wejście na sektor → opuszczenie stadionu podczas ewakuacji. Przebieg tego cyklu zależy od dostępności miejsc w sektorach, liczby aktywnych kas, przynależności do drużyny, statusu VIP oraz losowych zdarzeń.

Liczba kasjerów jest dynamiczna i zależna od aktualnego obciążenia systemu oraz długości kolejek. Ograniczenia systemowe obejmują m.in. maksymalną pojemność sektorów, ograniczoną przepustowość bramek, możliwość blokowania sektorów oraz konieczność przeprowadzenia ewakuacji w sytuacjach awaryjnych.

Manager zarządza globalnym stanem systemu poprzez uruchamianie i zamykanie procesów kasjerów, wysyłanie sygnałów do pozostałych procesów oraz inicjowanie procedury ewakuacji. Technicy odpowiadają za kontrolę wejść do sektorów, obsługę kolejek przy bramkach oraz potwierdzanie opróżnienia sektorów. Kasjerzy realizują sprzedaż biletów i przydzielanie miejsc, natomiast kibice symulują zachowanie uczestników wydarzenia.

Cała symulacja generuje raport tekstowy zapisywany do pliku `raport.txt`, dokumentujący przebieg działania systemu, istotne zdarzenia oraz aktualne stany procesów.

Projekt ma na celu praktyczne wykorzystanie i zaprezentowanie mechanizmów zarządzania procesami, synchronizacji, komunikacji międzyprocesowej, obsługi sygnałów oraz pracy na zasobach systemowych w środowisku Linux/UNIX.

# 2. Ogólny opis kodu

Projekt został podzielony na kilka logicznie rozdzielonych plików źródłowych, z których każdy odpowiada za odrębny element symulowanego systemu. Taki podział ułatwia rozwój projektu, zwiększa czytelność kodu oraz pozwala na łatwiejsze testowanie i debugowanie poszczególnych komponentów.

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

# 3. Zrealizowane funkcjonalności

W ramach projektu zaimplementowano następujące funkcjonalności systemu:

- **Zarządzanie procesami**  
  Tworzenie i kontrola procesów managera, kasjerów, techników oraz kibiców.

- **Inicjalizacja mechanizmów IPC**  
  Tworzenie i konfiguracja pamięci współdzielonej, kolejek komunikatów oraz semaforów System V.

- **Sprzedaż biletów**  
  Obsługa sprzedaży biletów przez kasjerów z uwzględnieniem dostępności sektorów oraz statusu VIP.

- **Obsługa klientów VIP**  
  Priorytetowa obsługa kibiców VIP w kolejkach do kas.

- **Kontrola wejścia na stadion**  
  Weryfikacja kibiców na bramkach przez techników oraz kierowanie ich do odpowiednich sektorów.

- **Zarządzanie pojemnością sektorów**  
  Monitorowanie liczby zajętych miejsc i blokowanie sprzedaży po osiągnięciu limitu.

- **System kolejek do bramek**  
  Implementacja kolejek wejściowych dla poszczególnych sektorów.

- **Wykrywanie niebezpiecznych zachowań**  
  Identyfikacja kibiców posiadających materiały pirotechniczne i usuwanie ich z systemu.

- **Mechanizm priorytetu przy bramkach**  
  Przyznawanie priorytetu kibicom długo oczekującym w kolejce.

- **Blokowanie i odblokowywanie sektorów**  
  Możliwość czasowego zamykania wybranych sektorów za pomocą sygnałów systemowych.

- **Procedura ewakuacji**  
  Automatyczne oraz ręczne uruchamianie ewakuacji i nadzorowanie opróżniania sektorów.

- **Dynamiczne zarządzanie kasami**  
  Otwieranie i zamykanie kas w zależności od obciążenia systemu.

- **Obsługa sytuacji wyjątkowych**  
  Reakcja na błędy IPC, sygnały systemowe oraz nieoczekiwane zakończenie procesów.

- **Rejestrowanie przebiegu symulacji**  
  Zapisywanie zdarzeń i komunikatów do pliku `raport.txt`.

- **Bezpieczne zamykanie systemu**  
  Zwolnienie zasobów IPC oraz poprawne zakończenie wszystkich procesów.

# 4. Napotkane problemy i trudności

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

# 5. Wyróżniające się elementy specjalne

Projekt zawiera kilka istotnych rozwiązań, które zwiększają jego funkcjonalność oraz poziom zaawansowania.

- **Dynamiczne zarządzanie kasjerami**  
  Automatyczne otwieranie i zamykanie kas w zależności od liczby oczekujących kibiców.

- **Obsługa klientów VIP**  
  Priorytetowa obsługa wybranych kibiców podczas zakupu biletów.

- **System priorytetów przy bramkach**  
  Przyznawanie pierwszeństwa kibicom długo oczekującym w kolejce.

- **Posiadanie materiałów pirotechnicznych (race)**
  umożliwia wykrycie niebezpiecznych kibiców i ich usunięcie z systemu.

- **Automatyczna procedura ewakuacji**  
  Samoczynne uruchamianie ewakuacji po wykryciu zagrożenia lub przekroczeniu pojemności stadionu.

- **Synchronizowane logowanie zdarzeń**  
  Zabezpieczenie zapisu do pliku `raport.txt` przy użyciu semaforów.

  # 6. Opis semaforów

W projekcie zastosowano zestaw semaforów System V w celu synchronizacji dostępu do zasobów współdzielonych oraz zapewnienia poprawnej współpracy pomiędzy procesami.

Wykorzystywane semafory pełnią następujące funkcje:

- **Semafor 0 – synchronizacja logowania**  
  Odpowiada za kontrolę dostępu do funkcji zapisujących dane do pliku `raport.txt`, zapobiegając nakładaniu się komunikatów z różnych procesów.

- **Semafor 1 – synchronizacja kolejek bramek**  
  Chroni struktury kolejek wejściowych do sektorów przed jednoczesną modyfikacją przez wiele procesów.

- **Semafor 2 – synchronizacja danych globalnych**  
  Zabezpiecza dostęp do głównych zmiennych w pamięci współdzielonej, takich jak liczba aktywnych kasjerów, stan ewakuacji czy kolejki do kas.

- **Semafory 3+ – synchronizacja sektorów**  
  Każdy sektor posiada własny semafor, który kontroluje dostęp do danych związanych z zajętością miejsc, bramkami oraz blokadą sektorów.

Zastosowanie oddzielnych semaforów dla poszczególnych obszarów systemu pozwala na ograniczenie liczby blokad, zwiększenie równoległości działania oraz poprawę wydajności symulacji.



