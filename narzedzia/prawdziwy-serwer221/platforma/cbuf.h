// [ETAP 221] cbuf na komputerze - CZESC NASZA.
//
// Prawdziwy cbuf.h z rdzenia ESP32 stoi na buforach FreeRTOS (RingbufHandle_t
// + semafor), ktorych na komputerze nie ma. Tutaj jest ta sama KLASA z tym
// samym interfejsem (metody uzywane przez biblioteke: available, room, size,
// read, write, resizeAdd), na zwyklym buforze kolowym.
//
// UWAGA: nasze trasy (/history*) NIE korzystaja z cbuf - to tylko builder
// AsyncResponseStream, uzywany przez inne trasy centrali. Uczciwie: ta klasa
// nie jest przedmiotem etapu i nie jest prawdziwa; jest zamiennikiem.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

class cbuf {
 public:
  explicit cbuf(size_t size) : dane_(size) {}
  ~cbuf() = default;

  size_t resizeAdd(size_t addSize) { return resize(dane_.size() + addSize); }
  size_t resize(size_t newSize) {
    dane_.resize(newSize);
    if (zajete_ > newSize) zajete_ = newSize;
    return dane_.size();
  }

  size_t available() const { return zajete_; }
  size_t size() { return dane_.size(); }
  size_t room() const { return dane_.size() - zajete_; }
  bool empty() const { return zajete_ == 0; }
  bool full() const { return zajete_ == dane_.size(); }

  int peek() { return empty() ? -1 : static_cast<int>(dane_[ogon_]); }

  int read() {
    if (empty()) return -1;
    const int c = static_cast<int>(dane_[ogon_]);
    ogon_ = (ogon_ + 1) % dane_.size();
    --zajete_;
    return c;
  }

  size_t read(char *dst, size_t len) {
    size_t n = 0;
    while (n < len && !empty()) dst[n++] = static_cast<char>(read());
    return n;
  }

  size_t write(char c) { return write(&c, 1); }

  size_t write(const char *src, size_t len) {
    if (dane_.empty()) return 0;
    size_t n = 0;
    while (n < len && !full()) {
      dane_[(ogon_ + zajete_) % dane_.size()] = static_cast<uint8_t>(src[n++]);
      ++zajete_;
    }
    return n;
  }

  void flush() { zajete_ = 0; ogon_ = 0; }
  size_t remove(size_t len) {
    size_t n = 0;
    while (n < len && !empty()) {
      read();
      ++n;
    }
    return n;
  }

  cbuf *next = nullptr;
  bool has_peek = false;
  uint8_t peek_byte = 0;

 protected:
  std::vector<uint8_t> dane_;
  size_t ogon_ = 0;
  size_t zajete_ = 0;
};
