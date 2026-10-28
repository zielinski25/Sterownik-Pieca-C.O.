// WYGENEROWANE przez narzedzia/panel-pobrania213/build.py - nie edytowac.
// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp
//         + main_centrala-v3.56.0-to-v3.56.1.patch (sha256 8faa7404841e55a0ecf289e2f1c1ba37061adff584650e25b4b4bbcd1a05ea83)
// Ponizej: prawdziwy kod z pliku po patchu v3.56.1.

static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(esp_random() & 0xFFu);
}
static std::uint32_t historiaZegar213() { return static_cast<std::uint32_t>(millis()); }

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

// Tekst panelu (przycisk + JS), dokladnie taki, jaki trafia do WWW.
static const char *PANEL_213 = R"PANEL213(
<!-- [v3.56.1 HISTORIA-POBRANIE] Pobranie plikow historii (CSV) na komputer/telefon.
     Staly token z secrets.h NIE jest tu wklejony: przycisk prosi centrale o
     jednorazowy bilet (GET /history/link), a potem otwiera /history?k=<bilet>.
     Bilet wygasa po 5 minutach i dziala tylko raz. -->
<button id="histBtn" type="button" style="position:fixed;left:12px;bottom:12px;z-index:60;padding:10px 16px;border:0;border-radius:999px;background:#1f6feb;color:#fff;font-weight:700;font-size:14px;box-shadow:0 6px 18px rgba(0,0,0,.35);cursor:pointer">&#11015; HISTORIA CSV</button>
<div id="histPanel" style="display:none;position:fixed;left:12px;bottom:66px;z-index:60;width:290px;background:#111827;color:#e5e7eb;border:1px solid #374151;border-radius:14px;padding:14px;box-shadow:0 12px 32px rgba(0,0,0,.5)">
  <div style="font-weight:700;margin-bottom:10px">Pobierz historię</div>
  <button id="histShort" type="button" style="display:block;width:100%;margin-bottom:8px;padding:10px;border:0;border-radius:10px;background:#2563eb;color:#fff;font-weight:600;cursor:pointer">Krótka (24 h)</button>
  <button id="histLong" type="button" style="display:block;width:100%;padding:10px;border:0;border-radius:10px;background:#374151;color:#fff;font-weight:600;cursor:pointer">Długa (archiwum)</button>
  <div id="histInfo" role="status" style="margin-top:10px;font-size:12px;line-height:1.35;opacity:.85">Link jest jednorazowy i wygasa po 5 minutach. Jeśli pobieranie się nie zacznie, kliknij jeszcze raz.</div>
</div>
<script>
/* [v3.56.1 HISTORIA-POBRANIE] Obsluga przycisku pobrania. Celowo bez zaleznosci
   od reszty panelu: jesli ktorys element nie istnieje, nic sie nie dzieje. */
(function () {
  var przycisk = document.getElementById('histBtn');
  var panel = document.getElementById('histPanel');
  var info = document.getElementById('histInfo');
  if (!przycisk || !panel) return;
  function komunikat(t) { if (info) info.textContent = t; }
  function pobierz(strumien) {
    komunikat('Przygotowuję link…');
    fetch('/history/link?stream=' + strumien, { cache: 'no-store' })
      .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
      .then(function (bilet) {
        bilet = (bilet || '').trim();
        if (bilet.length !== 32) throw new Error('centrala nie dała biletu');
        window.location.href = '/history?k=' + encodeURIComponent(bilet) + '&s=' + strumien;
      })
      .catch(function (e) { komunikat('Nie udało się: ' + e.message); });
  }
  przycisk.addEventListener('click', function () {
    panel.style.display = (panel.style.display === 'none' ? 'block' : 'none');
  });
  var krotka = document.getElementById('histShort');
  var dluga = document.getElementById('histLong');
  if (krotka) krotka.addEventListener('click', function () { pobierz('short'); });
  if (dluga) dluga.addEventListener('click', function () { pobierz('long'); });
})();
</script>
)PANEL213";
