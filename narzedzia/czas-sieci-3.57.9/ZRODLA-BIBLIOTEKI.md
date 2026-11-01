# Dowód źródłowy z arduino-esp32 2.0.17

Data: 2026-11-01. Pliki pobrane z `github.com/espressif/arduino-esp32`, tag **2.0.17**
(gałąź 2.0.x — ta sama, z której jest `framework-arduinoespressif32` w PlatformIO
dla ESP32-S3). Nic tu nie jest „z pamięci” — poniżej dosłowne fragmenty.

## 1. Jednostki: parametr jest w SEKUNDACH, pole w MILISEKUNDACH

`libraries/WiFiClientSecure/src/WiFiClientSecure.cpp` (SHA-256 `50c05f73…`):

```cpp
void WiFiClientSecure::setHandshakeTimeout(unsigned long handshake_timeout)
{
    sslclient->handshake_timeout = handshake_timeout * 1000;
}
```

`libraries/WiFiClientSecure/src/WiFiClientSecure.h` (SHA-256 `5c1905ee…`):

```cpp
void setHandshakeTimeout(unsigned long handshake_timeout);   // sekundy
int  setTimeout(uint32_t seconds);                           // sekundy
```

Czyli: **`setHandshakeTimeout(4000)` = 4 000 000 ms = 66 min 40 s**, a nie 4 s.
Żeby dostać 4 s, trzeba podać **4**.

## 2. Gdzie ten limit jest używany — pętla handshake

`libraries/WiFiClientSecure/src/ssl_client.cpp` (SHA-256 `10d70932…`), funkcja `start_ssl_client()`:

```cpp
log_v("Performing the SSL/TLS handshake...");
unsigned long handshake_start_time = millis();
while ((ret = mbedtls_ssl_handshake(&ssl_client->ssl_ctx)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        return handle_error(ret);
    }
    if ((millis() - handshake_start_time) > ssl_client->handshake_timeout)
        return -1;
    vTaskDelay(2);   // 2 ticks
}
```

Wartość domyślna (`WiFiClientSecure.cpp`, konstruktory, linie 40 i 61):

```cpp
sslclient->handshake_timeout = 120000;   // 120 s
```

## 3. Dlaczego `applyFbSockTimeouts(…, 8)` tego nie ratuje

`main_centrala.cpp` (3.57.8, linia 42759) ustawia `SO_SNDTIMEO`/`SO_RCVTIMEO` na 8 s.
To limit **pojedynczego `recv()`**, a nie całego handshake'u: gdy `recv()` wróci
z `EAGAIN`, mbedtls zwraca `MBEDTLS_ERR_SSL_WANT_READ`, pętla wyżej **kręci się dalej**
aż do `handshake_timeout`. Czyli przy `4000` (sekund) socket-level timeout tylko
przyspiesza pętlę — nie skraca całości.

## 4. TCP connect: osobny limit, domyślnie 5 s

`libraries/HTTPClient/src/HTTPClient.h` (SHA-256 `cfeb3777…`):

```cpp
#define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)
int32_t _connectTimeout = HTTPCLIENT_DEFAULT_TCP_TIMEOUT;
uint16_t _tcpTimeout     = HTTPCLIENT_DEFAULT_TCP_TIMEOUT;
```

`libraries/HTTPClient/src/HTTPClient.cpp` (SHA-256 `22ae57d4…`):

```cpp
if(!_client->connect(_host.c_str(), _port, _connectTimeout)) { ... }   // 5 s na SYN
_client->setTimeout((_tcpTimeout + 500) / 1000);                      // sekundy
```

```cpp
void HTTPClient::setTimeout(uint16_t timeout) {
    _tcpTimeout = timeout;                       // milisekundy
    if(connected()) _client->setTimeout((timeout + 500) / 1000);  // ale klient dostaje SEKUNDY
}
```

**Uwaga:** `http.setTimeout(4000)` → `_tcpTimeout = 4000` ms, ale klient dostaje
`(4000+500)/1000 = 4` s. Jednostki są tu mieszane, ale wynik (4 s) jest poprawny —
problem jest wyłącznie w `setHandshakeTimeout`.

## 5. Podsumowanie liczbowe

| element | limit domyślny | co ustawia 3.57.8 | faktyczny limit w 3.57.8 |
|---|---|---|---|
| DNS (`hostByName`) | brak wbudowanego | — | bez ograniczenia |
| TCP connect | 5 000 ms | nic (`setConnectTimeout` nieużywane) | **5 s** ✅ |
| TLS handshake | 120 s | `setHandshakeTimeout(4000)` | **4 000 s = 66 min 40 s** ❌ |
| odczyt odpowiedzi | 30 s (klient) | `http.setTimeout(4000)` → 4 s | 4 s bezczynności ✅ |

Realny najgorszy przypadek jednego `GET` w 3.57.8: **5 s + 4 000 s ≈ 66,7 min**,
a nie 125 s. To wyjaśnia restart po 300 s ciszy bez żadnej pętli ponowień.
