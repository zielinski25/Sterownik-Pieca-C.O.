// WYGENEROWANE przez narzedzia/wytrzymalosc217/build.py - nie edytowac.
// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp
//         + narzedzia/od-przycisku216/main_centrala-v3.55.8-to-v3.56.3.patch
//         (sha256 0a092380242787b8957c3aa66af796a5daed069806620d6ce66c305e4a3a5535)
// Ponizej PRAWDZIWY kod z pliku po patchu v3.56.3 - bez zadnych zmian.

// ---- oba wysylacze dokumentow Telegram (dodane w etapie 211) --------
bool sendTelegramDocumentHistKrotka() {
  if (!historiaDostepGotowy()) return false;
  if (!tgEnabled || tgBotToken.length() < 10 || tgChatId.length() < 3) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!littlefsReady) return false;

  historia211::Pin pin;
  {
    if (!logMutex || xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    const historia211::OpenCode zapinka =
        pin.open(historiaSys, historia211::kShortVfsPath, g_histShortSize, historia211::kMaxDocumentBytes);
    xSemaphoreGive(logMutex);
    if (zapinka != historia211::OpenCode::Ok) {
      DLOG(DC_TELEGRAM, DL_WARN, F("TG-DOC-HIST msg=\"brak zapinki na plik historii - nie wyslano\""));
      return false;
    }
  }
  const size_t totalLogSize = static_cast<size_t>(pin.length());

  if (!tgWezmijSendMutex("TG-DOC-HIST")) return false;

  bool ok = false;
  bool cialoOk = true;
  bool tgDocNetFailCounted = false;
  auto tgDocNetFail = [&]() {
    if (!tgDocNetFailCounted) { TG_NET_FAIL(); tgDocNetFailCounted = true; }
  };

  tgClient.stop();
  tgClientReady = false;

  if (tgEnsureConnected(tgClient, tgClientReady, tgClientLastUse)) {
    const String boundary = "ESP32TGBndHistS1";
    String caption = "Wykres krotki - ostatnie 24h (" + String((unsigned)(totalLogSize / 1024)) + " KB)";

    String partHead =
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
      tgChatId + "\r\n"
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
      caption + "\r\n"
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"document\"; filename=\"hist_short.csv\"\r\n"
      "Content-Type: text/csv\r\n\r\n";
    String partTail = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = partHead.length() + totalLogSize + partTail.length();

    serwisujTWDTJesliZarejestrowany();
    if (!tgWriteGuard(tgClient)) { tgDocNetFail(); goto tgDocHistSSendDone; }
    tgClient.print(String("POST /bot") + tgBotToken + "/sendDocument HTTP/1.1\r\n" +
                   "Host: " + TG_HOST + "\r\n" +
                   "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
                   "Content-Length: " + String((unsigned)totalLen) + "\r\n" +
                   "Connection: close\r\n\r\n");
    serwisujTWDTJesliZarejestrowany();
    if (!tgWriteGuard(tgClient)) { tgDocNetFail(); goto tgDocHistSSendDone; }
    tgClient.print(partHead);

    {
      const size_t DOC_CHUNK = 4096;
      uint8_t* chBuf = (uint8_t*) ps_malloc(DOC_CHUNK);
      if (!chBuf) chBuf = (uint8_t*) malloc(DOC_CHUNK);

      if (!chBuf) {
        DLOG(DC_TELEGRAM, DL_ERR, F("TG-DOC-HIST msg=\"brak pamieci na bufor 4KB\""));
        tgDocNetFail();
        cialoOk = false;
      } else {
        while (pin.sent() < pin.length()) {
          const historia211::ReadResult odczyt = pin.read(chBuf, DOC_CHUNK);
          if (odczyt.code != historia211::ReadCode::Data) {
            DLOG(DC_TELEGRAM, DL_ERR, F("TG-DOC-HIST msg=\"odczyt historii niepotwierdzony - zrywam polaczenie\""));
            tgDocNetFail();
            cialoOk = false;
            break;
          }
          if (!tgWriteGuard(tgClient)) { tgDocNetFail(); cialoOk = false; break; }
          size_t zapisano = 0;
          while (zapisano < odczyt.bytes) {
            const size_t w = tgClient.write(chBuf + zapisano, odczyt.bytes - zapisano);
            if (w == 0) break;
            zapisano += w;
          }
          if (zapisano != odczyt.bytes) {
            DLOG(DC_TELEGRAM, DL_ERR, F("TG-DOC-HIST msg=\"krotki zapis do gniazda - zrywam polaczenie\""));
            tgDocNetFail();
            cialoOk = false;
            break;
          }
          serwisujTWDTJesliZarejestrowany();
        }
        free(chBuf);
      }
    }

    if (!cialoOk) goto tgDocHistSSendDone;

    serwisujTWDTJesliZarejestrowany();
    if (!tgWriteGuard(tgClient)) { tgDocNetFail(); goto tgDocHistSSendDone; }
    tgClient.print(partTail);

    {
      String response;
      response.reserve(768);
      unsigned long readStart = millis();
      while ((tgClient.connected() || tgClient.available()) && millis() - readStart < 12000UL) {
        while (tgClient.available()) {
          char c = (char) tgClient.read();
          if (response.length() < 700) response += c;
          serwisujTWDTJesliZarejestrowany();
        }
        if (!tgClient.connected() && !tgClient.available()) break;
        serwisujTWDTJesliZarejestrowany();
        delay(5);
      }
      ok = response.indexOf("\"ok\":true") >= 0;
      Serial.printf("tag=TG-DOC-HIST zdarzenie=odpowiedz tresc=%s\n",
        response.length() > 0 ? response.c_str() : "(pusta)");
    }
  }

  tgDocHistSSendDone:
  tgClient.stop();
  tgClientReady = false;
  if (!pin.close()) DLOG(DC_TELEGRAM, DL_WARN, F("TG-DOC-HIST msg=\"zamkniecie zapinki niepotwierdzone\""));
  xSemaphoreGive(tgSendMutex);
  return ok;
}

bool sendTelegramDocumentHistDluga() {
  if (!historiaDostepGotowy()) return false;
  if (!tgEnabled || tgBotToken.length() < 10 || tgChatId.length() < 3) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!littlefsReady) return false;

  historia211::Pin pin;
  {
    if (!logMutex || xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    const historia211::OpenCode zapinka =
        pin.open(historiaSys, historia211::kLongVfsPath, g_histLongSize, historia211::kMaxDocumentBytes);
    xSemaphoreGive(logMutex);
    if (zapinka != historia211::OpenCode::Ok) {
      DLOG(DC_TELEGRAM, DL_WARN, F("TG-DOC-HISTL msg=\"brak zapinki na plik historii - nie wyslano\""));
      return false;
    }
  }
  const size_t totalLogSize = static_cast<size_t>(pin.length());

  if (!tgWezmijSendMutex("TG-DOC-HISTL")) return false;

  bool ok = false;
  bool cialoOk = true;
  bool tgDocNetFailCounted = false;
  auto tgDocNetFail = [&]() {
    if (!tgDocNetFailCounted) { TG_NET_FAIL(); tgDocNetFailCounted = true; }
  };

  tgClient.stop();
  tgClientReady = false;

  if (tgEnsureConnected(tgClient, tgClientReady, tgClientLastUse)) {
    const String boundary = "ESP32TGBndHistL1";
    String caption = "Wykres dlugi - ostatnie 7 dni (" + String((unsigned)(totalLogSize / 1024)) + " KB)";

    String partHead =
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
      tgChatId + "\r\n"
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
      caption + "\r\n"
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"document\"; filename=\"hist_long.csv\"\r\n"
      "Content-Type: text/csv\r\n\r\n";
    String partTail = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = partHead.length() + totalLogSize + partTail.length();

    serwisujTWDTJesliZarejestrowany();
    if (!tgWriteGuard(tgClient)) { tgDocNetFail(); goto tgDocHistLSendDone; }
    tgClient.print(String("POST /bot") + tgBotToken + "/sendDocument HTTP/1.1\r\n" +
                   "Host: " + TG_HOST + "\r\n" +
                   "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
                   "Content-Length: " + String((unsigned)totalLen) + "\r\n" +
                   "Connection: close\r\n\r\n");
    serwisujTWDTJesliZarejestrowany();
    if (!tgWriteGuard(tgClient)) { tgDocNetFail(); goto tgDocHistLSendDone; }
    tgClient.print(partHead);

    {
      const size_t DOC_CHUNK = 4096;
      uint8_t* chBuf = (uint8_t*) ps_malloc(DOC_CHUNK);
      if (!chBuf) chBuf = (uint8_t*) malloc(DOC_CHUNK);

      if (!chBuf) {
        DLOG(DC_TELEGRAM, DL_ERR, F("TG-DOC-HISTL msg=\"brak pamieci na bufor 4KB\""));
        tgDocNetFail();
        cialoOk = false;
      } else {
        while (pin.sent() < pin.length()) {
          const historia211::ReadResult odczyt = pin.read(chBuf, DOC_CHUNK);
          if (odczyt.code != historia211::ReadCode::Data) {
            DLOG(DC_TELEGRAM, DL_ERR, F("TG-DOC-HISTL msg=\"odczyt historii niepotwierdzony - zrywam polaczenie\""));
            tgDocNetFail();
            cialoOk = false;
            break;
          }
          if (!tgWriteGuard(tgClient)) { tgDocNetFail(); cialoOk = false; break; }
          size_t zapisano = 0;
          while (zapisano < odczyt.bytes) {
            const size_t w = tgClient.write(chBuf + zapisano, odczyt.bytes - zapisano);
            if (w == 0) break;
            zapisano += w;
          }
          if (zapisano != odczyt.bytes) {
            DLOG(DC_TELEGRAM, DL_ERR, F("TG-DOC-HISTL msg=\"krotki zapis do gniazda - zrywam polaczenie\""));
            tgDocNetFail();
            cialoOk = false;
            break;
          }
          serwisujTWDTJesliZarejestrowany();
        }
        free(chBuf);
      }
    }

    if (!cialoOk) goto tgDocHistLSendDone;

    serwisujTWDTJesliZarejestrowany();
    if (!tgWriteGuard(tgClient)) { tgDocNetFail(); goto tgDocHistLSendDone; }
    tgClient.print(partTail);

    {
      String response;
      response.reserve(768);
      unsigned long readStart = millis();
      while ((tgClient.connected() || tgClient.available()) && millis() - readStart < 12000UL) {
        while (tgClient.available()) {
          char c = (char) tgClient.read();
          if (response.length() < 700) response += c;
          serwisujTWDTJesliZarejestrowany();
        }
        if (!tgClient.connected() && !tgClient.available()) break;
        serwisujTWDTJesliZarejestrowany();
        delay(5);
      }
      ok = response.indexOf("\"ok\":true") >= 0;
      Serial.printf("tag=TG-DOC-HISTL zdarzenie=odpowiedz tresc=%s\n",
        response.length() > 0 ? response.c_str() : "(pusta)");
    }
  }

  tgDocHistLSendDone:
  tgClient.stop();
  tgClientReady = false;
  if (!pin.close()) DLOG(DC_TELEGRAM, DL_WARN, F("TG-DOC-HISTL msg=\"zamkniecie zapinki niepotwierdzone\""));
  xSemaphoreGive(tgSendMutex);
  return ok;
}

// ---- adaptery losowosci i zegara (213) -----------------------------
static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(esp_random() & 0xFFu);
}
static std::uint32_t historiaZegar213() { return static_cast<std::uint32_t>(millis()); }

// ---- bramka i obiekt trasy (212/213) -------------------------------
struct HistoriaBramka212 : historia212::Bramka {
  history193::Calls &io() override { return historiaSys; }
  bool magazynGotowy() override { return historiaDostepGotowy() && littlefsReady; }
  std::uint64_t potwierdzony(historia212::Strumien s) override {
    return (s == historia212::Strumien::Krotki) ? static_cast<std::uint64_t>(g_histShortSize)
                                                : static_cast<std::uint64_t>(g_histLongSize);
  }
  const char *sciezka(historia212::Strumien s) override {
    return (s == historia212::Strumien::Krotki) ? historia211::kShortVfsPath
                                                : historia211::kLongVfsPath;
  }
  bool zamknij() override {
    if (!logMutex) return false;
    return xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) == pdTRUE;
  }
  void otworz() override { xSemaphoreGive(logMutex); }
};
static HistoriaBramka212 historiaBramka;
static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);

// /history (ANY) - tresc lambdy z main_centrala.cpp
static void obslugaHistory(AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);
}

// /history/long (ANY) - tresc lambdy z main_centrala.cpp
static void obslugaHistoryLong(AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);
}

// /history/link (GET) - tresc lambdy z main_centrala.cpp
static void obslugaHistoryLink(AsyncWebServerRequest *request) {
    historiaPobranie.link(request);
}

// Sciezka -> obsluga, w kolejnosci rejestracji w main_centrala.cpp.
struct Trasa217 {
  const char *sciezka;
  void (*obsluz)(AsyncWebServerRequest *);
};
static const Trasa217 TRASY_OBSLUGI217[] = {
  {"/history", obslugaHistory},
  {"/history/long", obslugaHistoryLong},
  {"/history/link", obslugaHistoryLink},
};
static const int TRASY_OBSLUGI217_N = 3;
