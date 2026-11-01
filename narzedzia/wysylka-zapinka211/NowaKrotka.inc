// [PLAN_HISTORIA_I_USTAWIENIA_TELEGRAM.md, Czesc 7] Wysylka hist_short.csv
// jako dokument Telegram.
// [v3.55.9 HISTORIA-PIN] Zamiast wlasnej petli open()/read() po nazwie pliku
// i dlugosci z cache - zapinka historia211::Pin. Dlugosc jest ustalana RAZ,
// pod logMutexem (ta sama blokada, ktora bierze historiaFlushDoFS()), jako
// min(cache, rozmiar fizyczny); plik jest otwierany RAZ i czytany tylko do
// tej dlugosci. Skutki:
//   * ogloszony Content-Length zawsze zgadza sie z cialem,
//   * krotszy plik, blad odczytu i krotki zapis do gniazda koncza sie
//     zerwaniem polaczenia, a nie "udanym" wyslaniem ucietego dokumentu,
//   * logMutex jest trzymany wylacznie przy otwieraniu zapinki, nigdy
//     w trakcie wysylki - zapis historii nie czeka minutami.
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
