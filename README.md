# operating-systems-project
# Hala widowiskowo-sportowa – opis problemu do symulacji

W hali widowiskowo-sportowej o pojemności **K** kibiców zostaje rozegrany mecz finałowy siatkówki. Kibice mogą być rozmieszczeni w **8 sektorach**, z których każdy ma pojemność **K/8**. Dodatkowo w hali znajduje się **sektor VIP**.

---

## 1. Sprzedaż biletów

Zakup biletów odbywa się bezpośrednio przed zawodami. W hali funkcjonuje łącznie **10 kas biletowych**. Zasady ich działania są następujące:

- Zawsze działają **co najmniej 2 stanowiska kasowe**.
- Na każde **K/10 kibiców** znajdujących się w kolejce do kasy powinno przypadać **minimum 1 czynne stanowisko kasowe**.
- Jeżeli liczba kibiców w kolejce jest **mniejsza niż (K/10)\*(N-1)**, gdzie **N** oznacza liczbę czynnych kas, wówczas **jedna kasa zostaje zamknięta**.
- Jeden kibic może kupić **maksymalnie 2 bilety w tym samym sektorze**.
- Bilety do poszczególnych sektorów są **sprzedawane losowo przez wszystkie kasy**.
- Kasy są **automatycznie zamykane po sprzedaży wszystkich miejsc**.
- Kibice VIP (mniej niż **0,3% \* K**) **kupują bilety z pominięciem kolejki**.

Kibice przychodzą do kas w **losowych momentach czasu**, również po rozpoczęciu spotkania. Mecz rozpoczyna się o godzinie **Tp**.

---

## 2. Zasady bezpieczeństwa i wejścia na halę

Z uwagi na rangę imprezy obowiązują rygorystyczne zasady bezpieczeństwa:

- Do każdego z **8 sektorów** prowadzi **osobne wejście**.
- Wejście na halę możliwe jest **wyłącznie po przejściu kontroli bezpieczeństwa**.
- Kontrola przy każdym wejściu odbywa się **równolegle na 2 stanowiskach**, przy czym:
  - Na jednym stanowisku może jednocześnie przebywać **maksymalnie 3 osoby**.
  - Jeżeli kontrolowanych jest **więcej niż 1 osoba** na stanowisku, muszą one należeć do **tej samej drużyny**.
- Kibic oczekujący na kontrolę może **przepuścić w kolejce maksymalnie 5 innych kibiców**. Dłuższe oczekiwanie powoduje frustrację i może prowadzić do agresywnych zachowań, których należy unikać.
- Istnieje **niewielka liczba kibiców VIP** (mniej niż **0,3% \* K**), którzy:
  - wchodzą i wychodzą z hali **osobnym wejściem**,
  - **nie przechodzą kontroli bezpieczeństwa**,
  - muszą posiadać **bilet zakupiony w kasie**.
- **Dzieci poniżej 15 roku życia** mogą wejść na stadion wyłącznie **pod opieką osoby dorosłej**.

---

## 3. Sygnały kierownika hali

Kierownik obiektu wydaje następujące polecenia:

- **sygnał 1** – pracownik techniczny **wstrzymuje wpuszczanie kibiców do danego sektora**,
- **sygnał 2** – pracownik techniczny **wznawia wpuszczanie kibiców do sektora**,
- **sygnał 3** – wszyscy kibice **opuszczają stadion (ewakuacja)**.

Po opuszczeniu sektora przez wszystkich kibiców **pracownik techniczny przesyła informację do kierownika**, że sektor został opróżniony.

---

## 4. Cel zadania

Należy napisać oddzielne programy dla:

- **kierownika hali**,  
- **kasjera**,  
- **pracownika technicznego**,  
- **kibica**.

Przebieg symulacji należy **rejestrować w pliku lub plikach tekstowych** w postaci szczegółowego raportu.

## testy symulacji

### 1. Test dynamicznego sterowania kasami
Sprawdzenie poprawności automatycznego **otwierania i zamykania kas** w zależności od liczby kibiców w kolejce zgodnie z zasadą K/10 oraz minimalnej liczby 2 kas.

### 2. Test kontroli bezpieczeństwa
Weryfikacja poprawnego działania **stanowisk kontroli**, limitu **3 osób na stanowisku** oraz warunku, że przy jednoczesnej kontroli kilku osób muszą one należeć do **tej samej drużyny**.

### 3. Test ewakuacji – sygnał 3
Sprawdzenie, czy po wysłaniu sygnału 3 następuje **opuszczenie stadionu przez wszystkich kibiców** oraz czy pracownicy techniczni wysyłają **potwierdzenie opróżnienia sektorów do kierownika**.
