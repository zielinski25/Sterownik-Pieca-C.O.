# 205 — plan przed zmianą kodu

NIE WGRYWAĆ. Offline, bez zmian głównego programu, SDK ani urządzenia.

Przeczytano przypięte WebRequest.cpp c47917d, _onData, _handleUploadByte, _parseMultipartPostByte, _parseLine, _runMiddlewareChain. Parser zgłasza final w BOUNDARY_OR_DATA zanim zweryfikuje sufiks boundary; w DASH3_OR_RETURN2 może zmniejszyć _contentLength. Samo wywołanie handleRequest po _parsedLength==_contentLength nie jest dowodem poprawnego formularza.

Zakres: opcjonalny rygorystyczny tryb jednego pliku (bez pól tekstowych i kolejnych części), włączany po sprawdzeniu nagłówków, przed pierwszym bajtem body. W trybie tym final dopiero po pełnym `--\r\n` zamknięcia i przy dokładnej zadeklarowanej długości; brak skracania Content-Length. Błąd przerywa żądanie i anuluje metadane przez 203/204. Dotychczasowy parser poza tym trybem ma pozostać niezmieniony.

Test: wykonać wyodrębnione rzeczywiste _onData/_handleUploadByte/_parseMultipartPostByte z modelem String/klienta/requestu. Surowe body multipart w wielu podziałach (w tym każdy bajt), błędne/pominięte zakończenia, fałszywe prefiksy boundary w danych, długości, drugi plik/pole, przekroczenie kolejki, rozłączenie, pracownik w trakcie odbioru. Połączyć rzeczywiste callbacki parsera z Adapter204 i Pump202. Udowodnić osobno zachowanie oryginału. Native/ASan/UBSan i mutacje; czysta aplikacja łącznego patcha 199+203+205.

Granica: nie pełne żądanie HTTP od socketu; nagłówki i routing pozostają modelem, String także. Bez kwalifikacji chunked, produkcyjnych callbacków autoryzacji/routingu, całej biblioteki i ESP. Nie zmieniać zamrożonych poprzednich etapów.
