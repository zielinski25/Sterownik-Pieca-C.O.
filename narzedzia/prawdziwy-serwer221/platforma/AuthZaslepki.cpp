// [ETAP 221] Zaslepki sciezki authenticate() biblioteki - CZESC NASZA.
//
// PO CO TO JEST: WebRequest.cpp definiuje AsyncWebServerRequest::authenticate(),
// ktore wola funkcje z WebAuthentication.cpp. Tamten plik potrzebuje MD5Builder
// i mbedtls z rdzenia ESP32 (MD5, SHA1) - na komputerze ich nie ma i nie sa
// przedmiotem tego etapu.
//
// CZEGO TO NIE UKRYWA: nasze trasy (/history*) NIE wola authenticate().
// Sprawdzaja naglowek Authorization same (HistoriaPobranie.h: wyciagnijToken +
// tokenZgodny) - to jest kontrola Z05/Z06 w audyt.py. Zaslepki sa tylko po to,
// zeby program sie linkowal; kazda z nich zwraca "odmowa".
//
// Innymi slowy: jesli kiedys nasza trasa zawolalaby authenticate(), dostalaby
// ZAWSZE false - czyli odmowe - i kontrola audytu by to wykryla (Z06).
#include <Arduino.h>

bool checkBasicAuthentication(const char *header, const char *username, const char *password) {
  (void)header;
  (void)username;
  (void)password;
  return false;
}

bool checkDigestAuthentication(
  const char *header, const char *method, const char *username, const char *password, const char *realm, bool passwordIsHash, const char *nonce,
  const char *opaque, const char *uri
) {
  (void)header;
  (void)method;
  (void)username;
  (void)password;
  (void)realm;
  (void)passwordIsHash;
  (void)nonce;
  (void)opaque;
  (void)uri;
  return false;
}

String generateDigestHash(const char *username, const char *password, const char *realm) {
  (void)username;
  (void)password;
  (void)realm;
  return String();
}

String generateBasicHash(const char *username, const char *password) {
  (void)username;
  (void)password;
  return String();
}

String genRandomMD5() { return String(); }
