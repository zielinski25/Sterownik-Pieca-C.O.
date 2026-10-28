
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
        // [v3.56.3] Trasa musi byc ta sama, dla ktorej wydano bilet:
        // /history podaje ZAWSZE krotka, /history/long - dluga. Bilet
        // na dluga na trasie krotkiej nie pasuje i dawal 401.
        var trasa = (strumien === 'long') ? '/history/long' : '/history';
        window.location.href = trasa + '?k=' + encodeURIComponent(bilet);
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
