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

