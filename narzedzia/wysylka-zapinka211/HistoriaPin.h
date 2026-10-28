#pragma once
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/stat.h>
#include "CheckedHistoryIO.h"

// [v3.55.9 HISTORIA-PIN] Zapinka na POTWIERDZONY prefiks pliku historii.
//
// Po co: wysylacze dokumentow Telegram (sendTelegramDocumentHistKrotka/
// HistDluga) czytaly plik po nazwie, wlasna petla open()/read() i dlugoscia
// z cache (g_histShortSize/g_histLongSize). Skutki uboczne:
//   1. miedzy sprawdzeniem istnienia a otwarciem plik mogl zniknac,
//   2. dlugosc z cache mogla byc wieksza niz plik (np. po nieudanym zapisie),
//      a wtedy ogoloszony Content-Length nie zgadzal sie z cialem,
//   3. krotki odczyt i krotki zapis do gniazda byly traktowane jak koniec
//      danych - uciety dokument jechal jako "udana" wysylka,
//   4. nikt nie uzgadnial odczytu z logMutexem, ktory bierze pisarz.
//
// Ta klasa robi dokladnie jedno: pod blokada wywolujacego otwiera plik RAZ
// i ustala RAZ dlugosc prefiksu potwierdzonego przez oba zrodla - cache
// (znacznik "zapis potwierdzony") i rozmiar fizyczny z fstat. Potem pliku
// nie otwiera juz nikt, a czytane jest wylacznie tyle bajtow, ile zapowiedziano.
//
// Umowa wywolujacego (wazna):
//   * open() wola sie Z LOGMUTEXEM, ta sama blokada, ktora bierze
//     historiaFlushDoFS()/historiaDlugaFlushDoFS() i historiaPrzytnijPlik().
//     Dzieki temu fstat widzi granice potwierdzona przez pisarza, a nie
//     plik w trakcie dopisywania (Engine::append rosnie porcjami po 256 B,
//     wiec bez blokady dalo by sie zobaczyc polowke linii),
//   * blokade oddaje sie NATYCHMIAST po open() - nigdy na czas wysylki
//     (wysylka trwa minuty; trzymanie logMutexa zatrzymaloby zapis historii),
//   * po ReadCode::Short albo ReadCode::Fault zapinki nie wolno uzywac dalej:
//     plik jest krotszy niz zapowiedziano albo VFS zawiodl. Wywolujacy MUSI
//     przerwac polaczenie, a nie wysylac resztke jako sukces.
//
// Czego klasa NIE robi: nie blokuje rotacji (w v3.55.8 rotacja w czasie pracy
// jest wylaczona - historiaWyczysc() zwraca false), nie zmienia nazw, nie
// usuwa, nie dopisuje, nie trzyma blokady, nie alokuje pamietu.

namespace historia211 {

// Ścieżki VFS muszą być te same, których używa history193::Engine::name().
// Zgodność jest sprawdzana maszynowo przez build.py tego etapu (odczyt
// literałów z prawdziwego CheckedHistoryIO.h).
static constexpr const char *kShortVfsPath = "/littlefs/hist_short.csv";
static constexpr const char *kLongVfsPath = "/littlefs/hist_long.csv";

// Limit Telegrama na dokument wysylany przez bota to 50 MB. Plik wiekszy
// zostalby odrzucony po kilkudziesieciu minutach wysylania, wiec lepiej
// odmowic PRZED ogłoszeniem Content-Length. W praktyce limit nieosiagalny:
// historia krotka jest przycinana do HISTORIA_MAX_PROBEK linii, dluga do
// HISTORIA_DLUGA_MAX_PROBEK, czyli razem kilkaset KB.
static constexpr std::uint64_t kMaxDocumentBytes = 50ull * 1024ull * 1024ull;

enum class OpenCode : std::uint8_t {
  Ok,
  Fault,       // zle wywolanie, fstat/close zawiodl albo to nie zwykly plik
  Missing,     // nie ma czego otworzyc (open zwrocil blad)
  NothingCommitted,  // plik istnieje, ale potwierdzony prefiks ma 0 bajtow
  TooLarge,    // prefiks wiekszy niz kMaxDocumentBytes
};

enum class ReadCode : std::uint8_t {
  Data,   // dokladnie tyle bajtow, ile zapowiedziano w tej porcji
  End,    // caly zapowiedziany prefiks zostal wydany
  Short,  // plik skonczyl sie przed zapowiedziana dlugoscia - NIE sukces
  Fault,  // blad VFS albo zle uzycie - NIE sukces
};

struct ReadResult {
  ReadCode code;
  std::size_t bytes;  // sensowne wylacznie dla Data
  ReadResult(ReadCode c, std::size_t n = 0) : code(c), bytes(n) {}
};

class Pin {
 public:
  Pin() = default;
  ~Pin() { close(); }
  Pin(const Pin &) = delete;
  Pin &operator=(const Pin &) = delete;

  // Wywolac z logMutexem. committed = cache pisarza (g_histShortSize /
  // g_histLongSize, "ostatni potwierdzony zapis"). Dlugosc zapinki to
  // min(committed, rozmiar fizyczny), czyli prefiks potwierdzony PRZEZ OBA
  // zrodla: mniejsza z wartosci jest zawsze bezpieczna.
  OpenCode open(history193::Calls &io, const char *path, std::uint64_t committed,
                std::uint64_t maxBytes) {
    if (fd_ >= 0 || io_ != nullptr) return OpenCode::Fault;  // juz otwarta
    if (path == nullptr || *path == '\0') return OpenCode::Fault;
    if (maxBytes == 0) return OpenCode::Fault;
    io_ = &io;
    const int fd = io.open(path, O_RDONLY);
    if (fd < 0 || io.error() != 0) {
      io_ = nullptr;
      return OpenCode::Missing;
    }
    fd_ = fd;
    struct stat st {};
    if (io.stat(fd, &st) != 0 || io.error() != 0 || !S_ISREG(st.st_mode) ||
        st.st_size <= 0) {
      abandon();
      return OpenCode::Fault;
    }
    const std::uint64_t physical = static_cast<std::uint64_t>(st.st_size);
    length_ = (physical < committed) ? physical : committed;
    if (length_ == 0) {
      abandon();
      return OpenCode::NothingCommitted;
    }
    if (length_ > maxBytes) {
      abandon();
      return OpenCode::TooLarge;
    }
    return OpenCode::Ok;
  }

  std::uint64_t length() const { return length_; }
  std::uint64_t sent() const { return sent_; }
  bool active() const { return fd_ >= 0; }

  // Porcja: najwyzej capacity bajtow, nigdy wiecej niz zostalo do konca
  // zapowiedzianego prefiksu. Data tylko wtedy, gdy wydano DOKLADNIE tyle,
  // ile bylo miejsca w tej porcji - VFS moze zwracac mniej niz zadano,
  // wiec dokladamy az do pelnej porcji.
  ReadResult read(void *buffer, std::size_t capacity) {
    if (broken_) return ReadResult(ReadCode::Fault, 0);
    if (fd_ < 0 || io_ == nullptr) return ReadResult(ReadCode::Fault, 0);
    if (sent_ == length_) return ReadResult(ReadCode::End, 0);
    if (buffer == nullptr || capacity == 0) return ReadResult(ReadCode::Fault, 0);
    const std::uint64_t left = length_ - sent_;
    const std::size_t want =
        (left < static_cast<std::uint64_t>(capacity)) ? static_cast<std::size_t>(left) : capacity;
    std::size_t got = 0;
    while (got < want) {
      const ssize_t n = io_->read(fd_, static_cast<std::uint8_t *>(buffer) + got, want - got);
      if (n < 0 || io_->error() != 0) {
        broken_ = true;
        return ReadResult(ReadCode::Fault, got);
      }
      if (n == 0) {  // plik skonczyl sie przed zapowiedzia - nie koniec
        broken_ = true;
        return ReadResult(ReadCode::Short, got);
      }
      got += static_cast<std::size_t>(n);
    }
    sent_ += got;
    return ReadResult(ReadCode::Data, got);
  }

  // Zwraca false, gdy VFS zglosil blad zamkniecia. Zamknieta raz zapinka
  // zostaje zamknieta - powtorne close() jest bezpieczne i zwraca true.
  bool close() {
    if (fd_ < 0 || io_ == nullptr) {
      fd_ = -1;
      io_ = nullptr;
      return true;
    }
    const int fd = fd_;
    history193::Calls *io = io_;
    fd_ = -1;
    io_ = nullptr;
    const int rc = io->close(fd);
    return rc == 0 && io->error() == 0;
  }

 private:
  void abandon() { close(); }

  history193::Calls *io_ = nullptr;
  int fd_ = -1;
  std::uint64_t length_ = 0;
  std::uint64_t sent_ = 0;
  bool broken_ = false;
};

}  // namespace historia211
