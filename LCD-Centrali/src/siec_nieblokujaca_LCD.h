// siec_nieblokujaca_LCD.h
//
// Rozbudowana, nieblokujaca warstwa sieciowa dla main.cpp (LCD, cienki
// klient HTTP centrali Piec CO). Zastepuje WSZYSTKIE synchroniczne
// wywolania HTTPClient (httpGet, httpPost) uzywane dzis do:
//   - GET  /api/status          (co STATUSINTERWALMS)
//   - GET  /api/ustawienia-get  (co USTAWIENIAINTERWALMS)
//   - POST /api/*-manual, /api/ustawienia-set  (na dotkniecie usera)
//
// PROBLEM ktory to rozwiazuje (pelny opis w rozmowie / pliku .md):
//   1. HTTPClient::GET()/POST() sa ZAWSZE blokujace - gdy centrala
//      chwilowo zaburza wlasny kanal SoftAP (proba STA do domowego
//      routera), TCP handshake z LCD sie nie domyka i CALY loop() na
//      LCD stoi w miejscu do HTTPTIMEOUTMS (1500ms), blokujac tez
//      obsluzDotyk() - ekran "przywiesza sie".
//   2. To samo dotyczy POST-ow wysylanych z dotkniecia (suwak servo,
//      przycisk pompy, zmiana ustawienia) - jesli user akurat trafi
//      dotknieciem w moment zaburzenia kanalu, POST tez zamrozi ekran.
//
// ROZWIAZANIE:
//   Gole gniazda BSD (lwip/sockets.h) w trybie O_NONBLOCK. connect()
//   zwraca sie natychmiast (EINPROGRESS), gotowosc sprawdzamy przez
//   select() z zerowym czekaniem w KAZDEJ iteracji loop() - ten sam
//   wzorzec maszyny stanow co wifiPolaczSTAKrok na centrali. Zero
//   delay(), zero blokowania.
//
// NOWOSCI wzgledem pierwszej wersji (status_fetch_nonblocking.h):
//   - Jedna klasa NonBlockingHttp obsluguje ZAROWNO GET jak i POST,
//     zamiast osobnego mechanizmu tylko dla apistatus.
//   - Trzy gotowe globalne instancje: statusFetch, ustawieniaFetch,
//     komendaFetch (POST-y z dotkniecia).
//   - Wskaznik SWIEZOSCI danych (WskaznikSwiezosci) - do pokazania na
//     ekranie "dane sprzed X sekund" zamiast cichego zamrazania.
//   - Prosty log diagnostyczny w stylu WIFI-DIAG juz uzywanym w projekcie.
//   - Rozbudowany szkic integracji na koncu pliku, obejmujacy OBIE
//     galezie main.cpp (LVGLPOCTEST=1 i tryb bez LVGL) oraz obsluge
//     POST-ow z touch handlerow.

#pragma once

#include <Arduino.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>
#include <fcntl.h>  // F_GETFL/F_SETFL/O_NONBLOCK (uzywane w startWewn() nizej) - NIE zawsze dociagniete transytywnie przez lwip/sockets.h, jawny include jest bezpieczniejszy
#include <utility>  // std::move (zwolnienie bufora odpowiedzi przy braku pamieci, v1.2.2)

// =======================================================================
// KONFIGURACJA - podmien pod swoje wartosci (te same co dzis w main.cpp)
// =======================================================================
namespace SiecCfg {
    // MUSZA byc identyczne z CENTRALA_HOST/CENTRALA_PORT/HTTP_TIMEOUT_MS w
    // main.cpp - zdublowane celowo jako stale zamiast wspolnego naglowka,
    // bo main.cpp include'uje TEN plik PRZED wlasnymi deklaracjami tych
    // stalych (kolejnosc w pliku) - ten sam wzorzec co OTA_HASLO_PANEL w
    // main.cpp (patrz komentarz tam). Zmieniajac jedno, zmien i drugie.
    static const char*    HOST                 = "192.168.4.1"; // = CENTRALA_HOST
    static const uint16_t PORT                 = 80;             // = CENTRALA_PORT
    static const uint32_t CONNECT_TIMEOUT_MS   = 400;   // limit fazy laczenia
    static const uint32_t CALKOWITY_TIMEOUT_MS = 1500;  // = HTTP_TIMEOUT_MS
    static const size_t   BUFOR_ROZMIAR        = 512;   // kawalek na jeden recv()
    // [v1.2.0 LOG-POZIOM] Poziom logu warstwy sieciowej (bylo: bool
    // LOG_DIAGNOSTYCZNY = true, czyli odpowiednik poziomu 2):
    //   0 = tylko bledy ("blad", "odrzucono-zajete", "blad-socket"...),
    //   1 = bledy + WOLNE zadania (dluzsze niz PROG_WOLNEGO_ZADANIA_MS),
    //   2 = wszystko jak w v1.1.0: start/wyslano/sukces przy KAZDYM zadaniu,
    //       czyli 3 linie co 2 s - wlaczaj tylko na czas szukania problemu
    //       z sama siecia.
    // Powod zmiany: log z 2026-11-10 to 33 kB samych linii "sukces" z 3 minut
    // pracy i ani jednej informacji o tym, co faktycznie trafilo na ekran.
    // Stan polaczenia i danych pokazuje teraz okresowa linia "tag=PANEL"
    // (co 30 s, patrz panelDiagOkresowo() w main.cpp), a kazda NIEUDANA
    // odpowiedz jest logowana z przyczyna.
    static const uint8_t  POZIOM_LOGU             = 1;
    // Prog "wolnego" zadania. W logu z 2026-11-10 /api/ustawienia-get trwa
    // stale 235-270 ms, a /api/status 35-75 ms - prog musi byc wyraznie
    // powyzej tych wartosci, zeby zwykla praca nie byla zglaszana.
    // Prawdopodobne wyjasnienie stalych ~250 ms (z lektury zrodel obu stron,
    // NIEZMIERZONE): ESPAsyncWebServer centrali zamyka polaczenie dopiero po
    // potwierdzeniu TCP od panelu, a stos lwIP panelu potwierdza od razu
    // tylko co drugi segment - odpowiedz mieszczaca sie w jednym segmencie
    // (ustawienia) czeka na zegar opoznionego potwierdzenia (250 ms), a
    // dluzsza (status) nie.
    static const uint32_t PROG_WOLNEGO_ZADANIA_MS = 700;
}

// =======================================================================
// LOG DIAGNOSTYCZNY - ten sam styl co tagWIFI-DIAG w projekcie centrali,
// zeby dalo sie porownywac znaczniki czasu obu urzadzen w Serialu.
// =======================================================================
// `poziom`: najnizszy SiecCfg::POZIOM_LOGU, przy ktorym wpis sie pojawia
// (0 = blad, zawsze; 1 = wolne zadanie; 2 = zwykly przebieg zadania).
inline void siecDiagLog(const char* etap, const char* sciezka, const char* info = "", uint8_t poziom = 2) {
    if (poziom > SiecCfg::POZIOM_LOGU) return;
    Serial.printf("[NET-DIAG] etap=%s sciezka=%s info=%s ms=%lu\n",
                  etap, sciezka, info, (unsigned long)millis());
}

// [v1.2.1] Nazwa numeru bledu gniazda (errno / SO_ERROR) do logu. Same numery
// roznia sie miedzy systemami (na ESP32: EHOSTUNREACH = 118, ENOMEM = 12,
// ENOBUFS = 105), dlatego obok liczby idzie nazwa. Co znacza przy laczeniu z
// centrala:
//   EHOSTUNREACH / ENETUNREACH - nie ma drogi do centrali: panel nie jest
//                                polaczony z jej siecia WiFi albo nie ma adresu,
//   ENOMEM / ENOBUFS           - zabraklo pamieci w panelu,
//   ECONNREFUSED / ECONNRESET  - centrala odrzucila albo zerwala polaczenie.
inline const char* siecNazwaBledu(int numer) {
    switch (numer) {
        case EHOSTUNREACH: return "EHOSTUNREACH";
        case ENETUNREACH:  return "ENETUNREACH";
        case ENOMEM:       return "ENOMEM";
        case ENOBUFS:      return "ENOBUFS";
        case ECONNREFUSED: return "ECONNREFUSED";
        case ECONNRESET:   return "ECONNRESET";
        case ECONNABORTED: return "ECONNABORTED";
        case ETIMEDOUT:    return "ETIMEDOUT";
        case ENOTCONN:     return "ENOTCONN";
        case EADDRINUSE:   return "EADDRINUSE";
        default:           return "?";
    }
}

// =======================================================================
// Wskaznik swiezosci danych - do pokazania na dashboardzie, ze wartosci
// na ekranie moga byc nieaktualne, zamiast cichego "zamrozenia" bez
// zadnej informacji dla uzytkownika.
// =======================================================================
struct WskaznikSwiezosci {
    unsigned long ostatniSukcesMs = 0;
    bool byloKiedykolwiek = false;

    void odnotujSukces() {
        ostatniSukcesMs = millis();
        byloKiedykolwiek = true;
    }

    // true, gdy od ostatniej udanej odpowiedzi minelo wiecej niz progMs,
    // albo gdy jeszcze NIGDY nie bylo udanej odpowiedzi (np. tuz po starcie).
    bool saNieaktualne(uint32_t progMs) const {
        if (!byloKiedykolwiek) return true;
        return (millis() - ostatniSukcesMs) > progMs;
    }

    // Wiek ostatnich danych w sekundach, do wyswietlenia np. "12s temu".
    // Zwraca 0, jesli jeszcze nigdy nie bylo sukcesu (wywolujacy powinien
    // wtedy pokazac "brak danych", nie "0s temu").
    uint32_t wiekSekund() const {
        if (!byloKiedykolwiek) return 0;
        return (uint32_t)((millis() - ostatniSukcesMs) / 1000UL);
    }
};

// =======================================================================
// Glowna klasa - jedno zadanie GET lub POST na raz, w pelni nieblokujace.
// =======================================================================
enum class SiecState : uint8_t {
    IDLE,        // gotowe do startu nowego zadania
    CONNECTING,  // czekamy az connect() sie domknie (select() na zapis)
    READING,     // wyslano zadanie, czytamy odpowiedz kawalkami
    DONE,        // odpowiedz kompletna - czeka na odebranie przez pobierzOdpowiedz()
    ERROR_       // porazka - polaczenie, timeout, blad gniazda
};

class NonBlockingHttp {
public:
    // GET na podanej sciezce, np. "/api/status".
    bool startGet(const char* sciezka) {
        return startWewn(sciezka, "GET", "", "");
    }

    // POST z cialem form-urlencoded, np. body="stan=wl",
    // sciezka np. "/api/pompa-manual". Dokladnie ten format, ktorego
    // dzis uzywa httpPost() w main.cpp.
    bool startPostFormUrlEncoded(const char* sciezka, const String& body) {
        return startWewn(sciezka, "POST", body, "application/x-www-form-urlencoded");
    }

    // Wywolywane w KAZDEJ iteracji loop(). Nigdy nie blokuje dluzej niz
    // pojedyncze mikrosekundy (select() z timeoutem {0,0}).
    void poll() {
        if (stan_ == SiecState::IDLE || stan_ == SiecState::DONE || stan_ == SiecState::ERROR_) {
            return;
        }

        unsigned long teraz = millis();
        if (teraz - calkowityStartMs_ > SiecCfg::CALKOWITY_TIMEOUT_MS) {
            zamknijZBledem("timeout calkowity", 'c');
            return;
        }

        if (stan_ == SiecState::CONNECTING) {
            if (teraz - krokStartMs_ > SiecCfg::CONNECT_TIMEOUT_MS) {
                zamknijZBledem("timeout laczenia", 'l');
                return;
            }
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sock_, &wfds);
            struct timeval tv = {0, 0}; // zero-wait - tylko sprawdz, nie czekaj
            int sel = select(sock_ + 1, nullptr, &wfds, nullptr, &tv);
            if (sel > 0 && FD_ISSET(sock_, &wfds)) {
                int soErr = 0;
                socklen_t len = sizeof(soErr);
                getsockopt(sock_, SOL_SOCKET, SO_ERROR, &soErr, &len);
                if (soErr != 0) {
                    zamknijZBledem("connect() blad asynchroniczny", 'a', soErr);
                    return;
                }
                // Polaczono - zloz i wyslij zadanie HTTP/1.0.
                // Connection: close, zeby serwer sam zamknal gniazdo po
                // odpowiedzi - prosty sygnal konca danych bez potrzeby
                // parsowania Content-Length recznie.
                String zadanie = String(metoda_) + " " + sciezka_ + " HTTP/1.0\r\n" +
                                  "Host: " + SiecCfg::HOST + "\r\n" +
                                  "Connection: close\r\n";
                if (metoda_ == "POST") {
                    zadanie += "Content-Type: " + contentType_ + "\r\n";
                    zadanie += "Content-Length: " + String(cialo_.length()) + "\r\n";
                }
                zadanie += "\r\n";
                if (metoda_ == "POST") {
                    zadanie += cialo_;
                }
                int wyslano = send(sock_, zadanie.c_str(), zadanie.length(), 0);
                if (wyslano < 0) {
                    // [FIX-SEND-EWOULDBLOCK, ta sesja] Na nieblokujacym
                    // gniezdzie send() moze zwrocic -1/EWOULDBLOCK-EAGAIN,
                    // gdy bufor wysylkowy jest CHWILOWO pelny (normalne przy
                    // slabym sygnale WiFi) - to NIE jest prawdziwy blad,
                    // tylko "sprobuj ponownie za chwile", dokladnie tak samo
                    // jak recv() kilka linii nizej juz to traktuje. Bez tego
                    // rozroznienia KAZDY chwilowy zator wysylkowy konczyl
                    // sie od razu jako "send() nieudane", mimo ze polaczenie
                    // bylo w pelni sprawne. Zostajemy w CONNECTING (stan_
                    // bez zmian) i probujemy wyslac to samo zadanie ponownie
                    // w kolejnym poll() - istniejacy limit czasu tej fazy
                    // (CONNECT_TIMEOUT_MS) i tak ogranicza, jak dlugo to
                    // moze trwac, zanim faktycznie zglosimy blad.
                    if (errno == EWOULDBLOCK || errno == EAGAIN) {
                        return; // sprobuj wyslac ponownie w nastepnej iteracji poll()
                    }
                    zamknijZBledem("send() nieudane", 's', errno);
                    return;
                }
                siecDiagLog("wyslano", sciezka_.c_str());
                stan_ = SiecState::READING;
                krokStartMs_ = teraz;
            }
            // sel == 0: jeszcze nie gotowe - sprobuj w kolejnej iteracji loop()
            return;
        }

        if (stan_ == SiecState::READING) {
            char bufor[SiecCfg::BUFOR_ROZMIAR];
            int n = recv(sock_, bufor, sizeof(bufor) - 1, 0);
            if (n > 0) {
                bufor[n] = 0;
                // [v1.2.2 PAMIEC] String z Arduino przy braku pamieci nie
                // zglasza bledu - po prostu NIE dopisuje tekstu. Odpowiedz
                // bylaby wtedy ucieta i wygladala na blad centrali ("zly
                // JSON"). concat() zwraca false, gdy pamieci zabraklo.
                if (!odpowiedz_.concat(bufor)) {
                    // Oddaj to, co juz odebrano: String sam bufora nie
                    // zwalnia (przypisanie "" zostawia go na pozniej), a tu
                    // kazdy bajt jest potrzebny reszcie programu.
                    { String wyrzuc(std::move(odpowiedz_)); }
                    zamknijZBledem("brak pamieci na odpowiedz", 'm', ENOMEM);
                    return;
                }
                krokStartMs_ = teraz; // reset timeoutu przy kazdym odebranym kawalku
            } else if (n == 0) {
                zakonczSukcesem();
            } else {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    return; // brak danych teraz - sprobuj w nastepnej iteracji
                }
                zamknijZBledem("recv() blad", 'r', errno);
            }
        }
    }

    bool wTrakcie() const {
        return stan_ == SiecState::CONNECTING || stan_ == SiecState::READING;
    }

    bool gotowyWynik() const {
        return stan_ == SiecState::DONE || stan_ == SiecState::ERROR_;
    }

    bool bylSukces() const {
        return stan_ == SiecState::DONE;
    }

    // Zwraca surowa odpowiedz HTTP i resetuje stan do IDLE. Wywolaj
    // TYLKO gdy gotowyWynik() == true.
    String pobierzOdpowiedz() {
        String wynik = odpowiedz_;
        odpowiedz_ = "";
        stan_ = SiecState::IDLE;
        return wynik;
    }

    // [v1.2.2 PAMIEC] Odpowiedz BEZ kopiowania. pobierzOdpowiedz() zwraca
    // kopie - przy /api/status to dodatkowe ok. 1,8 kB w jednym kawalku na
    // kazde odpytanie. odpowiedz() daje odwolanie do wewnetrznego bufora,
    // wazne do wywolania zwolnijOdpowiedz() albo do startu nastepnego
    // zadania. Tylko gdy bylSukces() == true.
    const String& odpowiedz() const { return odpowiedz_; }
    // Koniec pracy z odpowiedzia: stan wraca do IDLE (to samo robilo
    // pobierzOdpowiedz()). Gdy w miedzyczasie ruszylo juz nastepne zadanie,
    // nic nie robi - nie wolno mu zabrac stanu ani bufora.
    void zwolnijOdpowiedz() {
        if (stan_ != SiecState::DONE) return;
        odpowiedz_ = "";
        stan_ = SiecState::IDLE;
    }

    String ostatniBlad() const { return blad_; }

    // [v1.2.1] Ostatni blad w postaci do krotkiego zapisu: NA KTORYM ETAPIE
    // zadanie padlo i z jakim numerem bledu (0, gdy to limit czasu):
    //   'g' socket()            'n' connect() - blad od razu
    //   'l' limit czasu laczenia (centrala nie odpowiada na probe polaczenia)
    //   'a' connect() - blad po chwili           's' send()
    //   'c' limit czasu calosci (polaczono, brak odpowiedzi)   'r' recv()
    //   'm' zabraklo pamieci na odebrana odpowiedz (numer bledu: ENOMEM)
    // '-' = od ostatniego startu zadania bledu nie bylo.
    char etapBledu() const { return etapBledu_; }
    int numerBledu() const { return numerBledu_; }

    // [FIX-LOG-SPAM, ta sesja] Jak pobierzOdpowiedz(), ale bez zwracania
    // niczego - wywolaj PO odczytaniu/zalogowaniu bledu z ostatniBlad(),
    // zeby zwolnic stan z powrotem do IDLE. Bez tego gotowyWynik() zostaje
    // TRWALE true po kazdym niepowodzeniu (ERROR_ nie znika samo), wiec
    // kod w main.cpp, ktory sprawdza gotowyWynik() na KAZDEJ iteracji
    // loop(), zglaszalby/logowal TEN SAM blad setki-tysiace razy na
    // sekunde, az do nastepnej proby startGet() - to byl bezposredni
    // wspolsprawca "burzy" identycznych linii w logu tuz przed
    // zawieszeniem watchdoga (patrz HISTORIA ZMIAN main.cpp).
    void skwitujBlad() {
        stan_ = SiecState::IDLE;
    }

    // Odcina naglowki HTTP, zwraca samo cialo (JSON albo "oktrue"/"okfalse" itp).
    static String wyodrebnijCialo(const String& surowaOdpowiedz) {
        int idx = surowaOdpowiedz.indexOf("\r\n\r\n");
        if (idx < 0) return "";
        return surowaOdpowiedz.substring(idx + 4);
    }

private:
    bool startWewn(const char* sciezka, const char* metoda, const String& cialo, const String& contentType) {
        if (stan_ != SiecState::IDLE && stan_ != SiecState::DONE && stan_ != SiecState::ERROR_) {
            siecDiagLog("odrzucono-zajete", sciezka, "", 0);
            return false; // poprzednie zadanie jeszcze w toku
        }

        sciezka_ = sciezka;
        metoda_ = metoda;
        cialo_ = cialo;
        contentType_ = contentType;
        odpowiedz_ = "";
        blad_ = "";
        etapBledu_ = '-';
        numerBledu_ = 0;

        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0) {
            etapBledu_ = 'g';
            numerBledu_ = errno;
            blad_ = String("socket() nieudane errno=") + String(numerBledu_) + " " + siecNazwaBledu(numerBledu_);
            stan_ = SiecState::ERROR_;
            siecDiagLog("blad-socket", sciezka, "", 0);
            return false;
        }

        int flags = fcntl(sock_, F_GETFL, 0);
        fcntl(sock_, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in adres;
        memset(&adres, 0, sizeof(adres));
        adres.sin_family = AF_INET;
        adres.sin_port = htons(SiecCfg::PORT);
        adres.sin_addr.s_addr = inet_addr(SiecCfg::HOST);

        int r = connect(sock_, (struct sockaddr*)&adres, sizeof(adres));
        if (r < 0 && errno != EINPROGRESS) {
            // [v1.2.1] Numer i nazwa bledu do logu. W logu z 2026-11-11
            // (fragment sprzed resetu panelu) tym bledem konczyla sie kazda
            // proba i nie bylo widac, CZEGO zabraklo - patrz siecNazwaBledu().
            const int numerBledu = errno;
            close(sock_);
            sock_ = -1;
            etapBledu_ = 'n';
            numerBledu_ = numerBledu;
            blad_ = String("connect() natychmiastowy blad errno=") + String(numerBledu) + " " + siecNazwaBledu(numerBledu);
            stan_ = SiecState::ERROR_;
            siecDiagLog("blad-connect-natychmiast", sciezka, blad_.c_str(), 0);
            return false;
        }

        stan_ = SiecState::CONNECTING;
        krokStartMs_ = millis();
        calkowityStartMs_ = millis();
        siecDiagLog("start", sciezka, metoda);
        return true;
    }

    // UWAGA: "sukces" znaczy tu WYLACZNIE "serwer zamknal gniazdo po
    // odpowiedzi". Nie mowi nic o kodzie HTTP ani o tym, czy tresc da sie
    // odczytac - to sprawdza dopiero wolajacy (obslugaStatusFetch() itd. w
    // main.cpp, przez diag::sprawdzOdpowiedz() + ArduinoJson) i TAM loguje
    // ewentualny blad.
    void zakonczSukcesem() {
        close(sock_);
        sock_ = -1;
        stan_ = SiecState::DONE;
        // [v1.2.0 LOG-POZIOM] Zadanie zakonczone, ale nietypowo dlugie - warte
        // jednej linii takze przy POZIOM_LOGU = 1 (patrz PROG_WOLNEGO_ZADANIA_MS).
        const uint32_t trwaloMs = (uint32_t)(millis() - calkowityStartMs_);
        if (trwaloMs > SiecCfg::PROG_WOLNEGO_ZADANIA_MS) {
            char info[24];
            snprintf(info, sizeof(info), "%lums", (unsigned long)trwaloMs);
            siecDiagLog("wolne", sciezka_.c_str(), info, 1);
        }
        siecDiagLog("sukces", sciezka_.c_str());
    }

    // `etap`: patrz etapBledu(). `numerBledu` (errno albo SO_ERROR), gdy jest
    // znany - trafia do tekstu bledu, a wiec i do linii "[STATUS] GET
    // nieudany: ..." w main.cpp.
    void zamknijZBledem(const char* powod, char etap, int numerBledu = 0) {
        etapBledu_ = etap;
        numerBledu_ = numerBledu;
        blad_ = powod;
        if (numerBledu != 0) blad_ += String(" errno=") + String(numerBledu) + " " + siecNazwaBledu(numerBledu);
        if (sock_ >= 0) {
            close(sock_);
            sock_ = -1;
        }
        stan_ = SiecState::ERROR_;
        siecDiagLog("blad", sciezka_.c_str(), blad_.c_str(), 0);
    }

    SiecState stan_ = SiecState::IDLE;
    int sock_ = -1;
    String sciezka_;
    String metoda_;
    String cialo_;
    String contentType_;
    String odpowiedz_;
    String blad_;
    char etapBledu_ = '-';
    int numerBledu_ = 0;
    unsigned long krokStartMs_ = 0;
    unsigned long calkowityStartMs_ = 0;
};

// [v1.2.2 PAMIEC] Odpowiedz zadania na czas jednej funkcji, bez kopiowania:
//     OdpowiedzDoKonca odp(statusFetch);
//     const String& surowa = odp.tekst();
// Przy wyjsciu z zakresu (kazdym - takze przez `return` w srodku) zadanie
// wraca do stanu IDLE. Uzywac tylko, gdy zadanie.bylSukces() == true.
class OdpowiedzDoKonca {
public:
    explicit OdpowiedzDoKonca(NonBlockingHttp& zadanie) : zadanie_(zadanie) {}
    ~OdpowiedzDoKonca() { zadanie_.zwolnijOdpowiedz(); }
    OdpowiedzDoKonca(const OdpowiedzDoKonca&) = delete;
    OdpowiedzDoKonca& operator=(const OdpowiedzDoKonca&) = delete;
    const String& tekst() const { return zadanie_.odpowiedz(); }

private:
    NonBlockingHttp& zadanie_;
};

// =======================================================================
// GOTOWE GLOBALNE INSTANCJE - po jednej na kazdy rodzaj ruchu sieciowego.
// Osobne instancje, zeby jedna wolna operacja (np. dlugi POST) nie
// blokowala mozliwosci wystartowania innej (np. kolejnego GET statusu).
// =======================================================================
extern NonBlockingHttp statusFetch;      // GET /api/status
extern NonBlockingHttp ustawieniaFetch;  // GET /api/ustawienia-get
extern NonBlockingHttp komendaFetch;     // POST z dotkniecia (pompa/servo/ustawienia-set)

extern WskaznikSwiezosci swiezoscStatusu;
extern WskaznikSwiezosci swiezoscUstawien;

/*
=================================================================================
[ZREALIZOWANE] Ponizszy szkic zostal wdrozony do main.cpp w tej sesji -
zachowany tutaj jako dokumentacja PIERWOTNEGO zamyslu (ten sam zwyczaj co
inne wpisy [FIX-*]/[ROZSZERZENIE] w main.cpp). Rzeczywista integracja w
paru miejscach odbiega od tego szkicu, celowo:
  - KROK 1/2: statusFetch.startGet() w main.cpp dostaje sciezke Z QUERY
    (?panel_fw=...&reset_powod=...), nie sama "/api/status" jak tutaj -
    startGet() bierze const char*, wiec wywolanie robi .c_str() na
    zbudowanym Stringu (bezpieczne - startWewn() od razu kopiuje go do
    wlasnego pola).
  - KROK 4: szkic ZREZYGNOWAL z natychmiastowego odswiezenia po udanej
    komendzie ("kolejny apistatus i tak odswiezy sie w normalnym cyklu") -
    w main.cpp jednak PRZYWROCONO natychmiastowe odswiezenie (flaga
    pendingPoKomendzie + obslugaKomendaFetch() wola pobierzStatus()/
    pobierzUstawienia() zaraz po zakonczeniu komendy), bo bez tego kazda
    zmiana z dotyku odswiezalaby sie na ekranie dopiero po
    STATUS_INTERWAL_MS/USTAWIENIA_INTERWAL_MS (2-5s) zamiast praktycznie
    natychmiast, jak dzis - realny krok wstecz w odczuwalnej responsywnosci.
  - KROK 6 (wskaznik "dane nieaktualne"): WDROZONE tylko na poziomie
    sledzenia (odnotujSukces() wolane z obslugaStatusFetch()/
    obslugaUstawieniaFetch() w main.cpp) - podpiecie pod faktyczne
    rysowanie (rysujNaglowek()/dashboard_lvgl_poc.*) NIE zrobione w tej
    sesji, patrz HISTORIA ZMIAN w main.cpp.
    [v1.2.0] Podpiete w galezi LVGL: po PROG_NIEAKTUALNYCH_DANYCH_MS bez
    poprawnie odczytanego statusu kafelki przechodza na "--" (patrz
    panelOznaczDaneJakoNieaktualne() w main.cpp), a odnotujSukces() jest
    wolane dopiero po sprawdzeniu kodu HTTP i JSON-a, nie po samym
    zamknieciu gniazda.
=================================================================================
ROZBUDOWANY SZKIC INTEGRACJI Z main.cpp (PIERWOTNY, patrz uwagi wyzej)
=================================================================================

--------------------------------------------------------------------
KROK 1 - definicje globalne (raz, na gorze main.cpp, po include)
--------------------------------------------------------------------
#include "siec_nieblokujaca_LCD.h"

NonBlockingHttp statusFetch;
NonBlockingHttp ustawieniaFetch;
NonBlockingHttp komendaFetch;
WskaznikSwiezosci swiezoscStatusu;
WskaznikSwiezosci swiezoscUstawien;

// Prog, po ktorym dashboard pokazuje "dane nieaktualne" - dobierz wg gustu,
// np. 3x STATUSINTERWALMS, zeby nie migac przy pojedynczym, normalnym
// opoznieniu, tylko przy realnym, dluzszym problemie.
static const uint32_t PROG_NIEAKTUALNYCH_DANYCH_MS = 3 * STATUSINTERWALMS; // 6000ms

--------------------------------------------------------------------
KROK 2 - odbior wyniku GET /api/status (zastepuje cialo pobierzStatus())
--------------------------------------------------------------------
// Wywolywane w loop() zamiast bezposredniego httpGet("/api/status").
void obslugaStatusFetch() {
    statusFetch.poll();

    if (statusFetch.gotowyWynik()) {
        if (statusFetch.bylSukces()) {
            String surowa = statusFetch.pobierzOdpowiedz();
            String json = NonBlockingHttp::wyodrebnijCialo(surowa);

            JsonDocument doc;
            if (deserializeJson(doc, json) == DeserializationError::Ok) {
                // -- WKLEJ TU DOKLADNIE to, co dzis jest w pobierzStatus()
                //    zaraz po deserializeJson(doc, odp) - cale przepisywanie
                //    pol z doc do struktury stan, bez zmian --
                stan.polaczono = true;
                swiezoscStatusu.odnotujSukces();
            }
        } else {
            // Porazka - NIC nie zerujemy w stan, stare wartosci zostaja
            // widoczne na ekranie (to juz jest dzisiejsze zachowanie).
            Serial.printf("[STATUS] blad pobierania: %s\n", statusFetch.ostatniBlad().c_str());
            stan.polaczono = false;
        }
    }
}

// W loop(), startowanie nowego zapytania TYLKO gdy poprzednie juz sie
// skonczylo (start*() i tak by odrzucil wywolanie w trakcie, ale
// sprawdzenie tutaj unika niepotrzebnego liczenia interwalu):
unsigned long teraz = millis();
if (teraz - ostatniStatus >= STATUSINTERWALMS && !statusFetch.wTrakcie()) {
    ostatniStatus = teraz;
    statusFetch.startGet("/api/status");
}
obslugaStatusFetch(); // wywolywane co iteracje - samo poll() jest tanie

--------------------------------------------------------------------
KROK 3 - analogicznie dla GET /api/ustawienia-get
--------------------------------------------------------------------
void obslugaUstawieniaFetch() {
    ustawieniaFetch.poll();
    if (ustawieniaFetch.gotowyWynik()) {
        if (ustawieniaFetch.bylSukces()) {
            String json = NonBlockingHttp::wyodrebnijCialo(ustawieniaFetch.pobierzOdpowiedz());
            JsonDocument doc;
            if (deserializeJson(doc, json) == DeserializationError::Ok) {
                // -- WKLEJ TU cale dotychczasowe cialo pobierzUstawienia()
                //    zaraz po deserializeJson --
                ustawieniaWczytane = true;
                swiezoscUstawien.odnotujSukces();
            }
        } else {
            Serial.printf("[USTAWIENIA] blad: %s\n", ustawieniaFetch.ostatniBlad().c_str());
        }
    }
}

if (teraz - ostatnieUstawienia >= USTAWIENIAINTERWALMS && !ustawieniaFetch.wTrakcie()
    && aktualnyEkran != EKRANUSTAWIENIA) { // ten sam warunek co dzisiaj - nie nadpisuj w trakcie edycji
    ostatnieUstawienia = teraz;
    ustawieniaFetch.startGet("/api/ustawienia-get");
}
obslugaUstawieniaFetch();

--------------------------------------------------------------------
KROK 4 - POST-y z dotkniecia (pompaManualWyslij, wyslijServoManualny,
wyslijUstawienie) - WSZYSTKIE przez jedna wspolna instancje komendaFetch,
bo user i tak dotyka jednej rzeczy na raz.
--------------------------------------------------------------------
// Zamiast:
//   bool ok = httpPost("/api/pompa-manual", "stan=" + stanTekst, blad);
// rob:
void pompaManualWyslij(const char* stanTekst) {
    if (komendaFetch.wTrakcie()) {
        Serial.println("[KOMENDA] poprzednia jeszcze w toku, pomijam");
        return; // opcjonalnie: pokaz na ekranie krotki toast "poczekaj..."
    }
    String body = String("stan=") + stanTekst;
    komendaFetch.startPostFormUrlEncoded("/api/pompa-manual", body);
    // UWAGA: usuniete natychmiastowe wywolanie pobierzStatus() po sukcesie -
    // wynik POST-a i tak przyjdzie nieblokujaco (patrz obslugaKomendaFetch
    // nizej), a kolejny apistatus i tak odswiezy sie w normalnym cyklu.
}

// Analogicznie przebuduj wyslijServoManualny(...) i wyslijUstawienie(...) -
// ten sam wzorzec: sprawdz komendaFetch.wTrakcie(), zloz body, startPostFormUrlEncoded().

// Odbior wyniku - wywolywane w loop() co iteracje (tanie):
void obslugaKomendaFetch() {
    komendaFetch.poll();
    if (komendaFetch.gotowyWynik()) {
        if (komendaFetch.bylSukces()) {
            String odp = NonBlockingHttp::wyodrebnijCialo(komendaFetch.pobierzOdpowiedz());
            // centrala odpowiada "oktrue,..." albo "okfalse,error=..." - ten sam
            // format co dzis parsuje httpPost() na koncu swojego ciala.
            JsonDocument doc;
            if (deserializeJson(doc, odp) == DeserializationError::Ok && doc["ok"].as<bool>()) {
                Serial.println("[KOMENDA] potwierdzona przez centrale");
                // NIC nie zmieniamy lokalnie w UI - kolejny apistatus i tak
                // pokaze prawdziwy stan (dokladnie ta sama zasada co dzis).
            } else {
                Serial.println("[KOMENDA] centrala odrzucila / blad parsowania");
            }
        } else {
            Serial.printf("[KOMENDA] nieudana: %s\n", komendaFetch.ostatniBlad().c_str());
        }
    }
}
// Wolaj obslugaKomendaFetch() w tym samym miejscu loop() co pozostale poll().

--------------------------------------------------------------------
KROK 5 - obsluzDotyk() zostaje BEZ ZMIAN, ale teraz faktycznie zawsze
dostaje szanse wykonac sie w tej samej iteracji - bo zaden z powyzszych
kroku (poll/start) nigdy nie blokuje.
--------------------------------------------------------------------
obsluzDotyk();

--------------------------------------------------------------------
KROK 6 - wskaznik "dane nieaktualne" na dashboardzie (opcjonalny,
ale rekomendowany - user widzi, ze cos jest nie tak, zamiast martwej ciszy)
--------------------------------------------------------------------
// W rysujNaglowek() albo rysujDashboard(), obok istniejacego tekstu statusu:
if (swiezoscStatusu.saNieaktualne(PROG_NIEAKTUALNYCH_DANYCH_MS)) {
    // Prosta opcja: dopisz maly tekst albo zmien kolor tla kafelkow na
    // przygaszony (np. domieszaj KOLTEKSTDIM zamiast normalnego koloru
    // wartosci w tresciKafelka), plus tekst w naglowku:
    String wiek = swiezoscStatusu.byloKiedykolwiek
        ? ("dane sprzed " + String(swiezoscStatusu.wiekSekund()) + "s")
        : "brak danych";
    // narysuj `wiek` w tym samym miejscu, gdzie dzis jest tekst
    // "polaczono"/"brak WiFi" w rysujNaglowek()
}

=================================================================================
UWAGI KONCOWE
=================================================================================
- Trzy osobne instancje (statusFetch/ustawieniaFetch/komendaFetch) NIE
  wspoldziela gniazd - moga teoretycznie dzialac rownolegle (np. POST
  komendy w trakcie oczekiwania na GET statusu). To celowe - user nie
  powinien czekac na zakonczenie okresowego odpytywania, zeby moc
  cokolwiek kliknac.
- Ta warstwa NIE eliminuje samego zaburzenia kanalu AP/STA po stronie
  centrali (patrz osobny plik .md z przegladem opcji) - eliminuje
  TYLKO efekt uboczny w postaci zamrozonego dotyku i "martwego" ekranu
  bez informacji. Oba tematy warto wdrozyc razem, dla pelnego efektu.
- CENTRALAHOST w main.cpp jest dzis "const char*" (adres IP jako tekst) -
  inet_addr() dziala na nim wprost. Jesli kiedys zmienisz go na String,
  uzyj CENTRALAHOST.c_str() w SiecCfg::HOST.
- Wymaga testu na sprzecie: (1) normalna praca - upewnij sie, ze dashboard
  nadal odswieza sie plynnie jak dzis; (2) test docelowy - fizycznie
  wylaczony domowy router, obserwacja czy dotyk zostaje responsywny
  podczas cyklicznych prob STA centrali; (3) dluzszy test wielogodzinny,
  zeby wykluczyc powolny wyciek deskryptorow gniazd przy jakiejs
  nieprzewidzianej sciezce bledu (kazdy zamknijZBledem/zakonczSukcesem
  jest napisany tak, by zawsze wolac close(sock_), ale warto to
  zweryfikowac np. logujac liczbe otwartych gniazd co jakis czas).
- Log [NET-DIAG] mozna wylaczyc ustawiajac SiecCfg::LOG_DIAGNOSTYCZNY
  na false po zakonczeniu testow, zeby nie zasmiecac Seriala w
  normalnej pracy.
  [v1.2.0] Zrobione inaczej: zamiast wlacz/wylacz jest SiecCfg::POZIOM_LOGU
  (0/1/2, domyslnie 1 = same bledy i wolne zadania) - patrz komentarz przy
  tej stalej na gorze pliku.
*/
