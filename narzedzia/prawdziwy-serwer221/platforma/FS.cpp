// [ETAP 221] Dysk na komputerze - CZESC NASZA.
//
// Prawdziwy FS.h z rdzenia ESP32 tylko deklaruje fs::File i fs::FS; ciala sa
// w rdzeniu i wolaja LittleFS. Tutaj ciala sa na zwyklych plikach w katalogu
// podanym w zmiennej HIST_ROOT (domyslnie "./dysk"), wiec bajty, ktore
// biblioteka wysyla do gniazda, pochodza z PRAWDZIWEGO pliku na dysku.
#include <Arduino.h>
#include <FS.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace fs {

namespace {

std::string korzen() {
  const char *r = std::getenv("HIST_ROOT");
  return (r != nullptr && *r != '\0') ? std::string(r) : std::string("./dysk");
}

std::string pelna(const char *sciezka) {
  std::string p = korzen();
  if (sciezka != nullptr) {
    if (sciezka[0] != '/') p += "/";
    if (sciezka != nullptr) p += sciezka;
  }
  return p;
}

}  // namespace

class FileImpl {
 public:
  explicit FileImpl(std::FILE *f, std::string p) : f_(f), p_(std::move(p)) {}
  ~FileImpl() { zamknij(); }

  void zamknij() {
    if (f_ != nullptr) {
      std::fclose(f_);
      f_ = nullptr;
    }
  }

  size_t write(uint8_t c) { return write(&c, 1); }
  size_t write(const uint8_t *buf, size_t n) {
    if (f_ == nullptr || buf == nullptr) return 0;
    return std::fwrite(buf, 1, n, f_);
  }
  int available() {
    if (f_ == nullptr) return 0;
    const long tu = std::ftell(f_);
    if (tu < 0) return 0;
    std::fseek(f_, 0, SEEK_END);
    const long koniec = std::ftell(f_);
    std::fseek(f_, tu, SEEK_SET);
    return (koniec > tu) ? static_cast<int>(koniec - tu) : 0;
  }
  int read() {
    if (f_ == nullptr) return -1;
    return std::fgetc(f_);
  }
  int peek() {
    if (f_ == nullptr) return -1;
    const int c = std::fgetc(f_);
    if (c != EOF) std::ungetc(c, f_);
    return c;
  }
  void flush() {
    if (f_ != nullptr) std::fflush(f_);
  }
  size_t read(uint8_t *buf, size_t n) {
    if (f_ == nullptr || buf == nullptr) return 0;
    return std::fread(buf, 1, n, f_);
  }
  bool seek(uint32_t pos, SeekMode mode) {
    if (f_ == nullptr) return false;
    const int skad = (mode == SeekCur) ? SEEK_CUR : (mode == SeekEnd) ? SEEK_END : SEEK_SET;
    const long cel = (mode == SeekCur) ? static_cast<long>(pos) : static_cast<long>(pos);
    return std::fseek(f_, cel, skad) == 0;
  }
  size_t position() {
    if (f_ == nullptr) return static_cast<size_t>(-1);
    const long p = std::ftell(f_);
    return (p < 0) ? static_cast<size_t>(-1) : static_cast<size_t>(p);
  }
  size_t size() {
    if (f_ == nullptr) return 0;
    const long tu = std::ftell(f_);
    std::fseek(f_, 0, SEEK_END);
    const long koniec = std::ftell(f_);
    if (tu >= 0) std::fseek(f_, tu, SEEK_SET);
    return (koniec < 0) ? 0 : static_cast<size_t>(koniec);
  }
  bool ok() const { return f_ != nullptr; }
  time_t getLastWrite() { return 0; }
  const char *path() const { return p_.c_str(); }
  const char *name() const {
    const std::size_t slash = p_.find_last_of('/');
    return (slash == std::string::npos) ? p_.c_str() : p_.c_str() + slash + 1;
  }
  bool isDirectory() const { return false; }
  FileImpl *openNextFile(bool *jestKatalog) {
    if (jestKatalog != nullptr) *jestKatalog = false;
    return nullptr;
  }

 private:
  std::FILE *f_ = nullptr;
  std::string p_;
};

class FSImpl {
 public:
  File open(const char *path, const char *mode, const bool create) {
    (void)create;
    const std::string p = pelna(path);
    std::FILE *f = std::fopen(p.c_str(), mode);
    if (f == nullptr) return File(FileImplPtr());
    return File(FileImplPtr(new FileImpl(f, p)));
  }
  bool exists(const char *path) {
    std::FILE *f = std::fopen(pelna(path).c_str(), "rb");
    if (f == nullptr) return false;
    std::fclose(f);
    return true;
  }
  bool remove(const char *path) { return std::remove(pelna(path).c_str()) == 0; }
  bool rename(const char *od, const char *do_) { return std::rename(pelna(od).c_str(), pelna(do_).c_str()) == 0; }
  bool mkdir(const char *path) {
    (void)path;
    return true;
  }
  bool rmdir(const char *path) {
    (void)path;
    return true;
  }
  const char *mountpoint() { return "/littlefs"; }
};

// ---------------------------------------------------------------------------
// Ciala fs::File - patrz FS.h. Kazda metoda to jeden krok do FileImpl.
// ---------------------------------------------------------------------------
size_t File::write(uint8_t c) { return _p ? _p->write(c) : 0; }
size_t File::write(const uint8_t *buf, size_t size) { return _p ? _p->write(buf, size) : 0; }
int File::available() { return _p ? _p->available() : 0; }
int File::read() { return _p ? _p->read() : -1; }
int File::peek() { return _p ? _p->peek() : -1; }
void File::flush() {
  if (_p) _p->flush();
}
size_t File::read(uint8_t *buf, size_t size) { return _p ? _p->read(buf, size) : 0; }
bool File::seek(uint32_t pos, SeekMode mode) { return _p ? _p->seek(pos, mode) : false; }
size_t File::position() const { return _p ? const_cast<FileImpl *>(_p.get())->position() : static_cast<size_t>(-1); }
size_t File::size() const { return _p ? const_cast<FileImpl *>(_p.get())->size() : 0; }
bool File::setBufferSize(size_t size) {
  (void)size;
  return false;
}
void File::close() {
  if (_p) _p->zamknij();
}
File::operator bool() const { return _p && const_cast<FileImpl *>(_p.get())->ok(); }
time_t File::getLastWrite() { return _p ? _p->getLastWrite() : 0; }
const char *File::path() const { return _p ? const_cast<FileImpl *>(_p.get())->path() : ""; }
const char *File::name() const { return _p ? const_cast<FileImpl *>(_p.get())->name() : ""; }
boolean File::isDirectory(void) { return _p ? _p->isDirectory() : false; }
boolean File::seekDir(long position) {
  (void)position;
  return false;
}
File File::openNextFile(const char *mode) {
  (void)mode;
  return File(FileImplPtr());
}
String File::getNextFileName(void) { return String(); }
String File::getNextFileName(boolean *isDir) {
  if (isDir != nullptr) *isDir = false;
  return String();
}
void File::rewindDirectory(void) {}

// ---------------------------------------------------------------------------
// Ciala fs::FS
// ---------------------------------------------------------------------------
File FS::open(const char *path, const char *mode, const bool create) { return _impl->open(path, mode, create); }
File FS::open(const String &path, const char *mode, const bool create) { return _impl->open(path.c_str(), mode, create); }
bool FS::exists(const char *path) { return _impl->exists(path); }
bool FS::exists(const String &path) { return _impl->exists(path.c_str()); }
bool FS::remove(const char *path) { return _impl->remove(path); }
bool FS::remove(const String &path) { return _impl->remove(path.c_str()); }
bool FS::rename(const char *od, const char *do_) { return _impl->rename(od, do_); }
bool FS::rename(const String &od, const String &do_) { return _impl->rename(od.c_str(), do_.c_str()); }
bool FS::mkdir(const char *path) { return _impl->mkdir(path); }
bool FS::mkdir(const String &path) { return _impl->mkdir(path.c_str()); }
bool FS::rmdir(const char *path) { return _impl->rmdir(path); }
bool FS::rmdir(const String &path) { return _impl->rmdir(path.c_str()); }
const char *FS::mountpoint() { return _impl->mountpoint(); }

// Instancja dla testu - biblioteka wymaga obiektu fs::FS, nie LittleFS.
static fs::FSImpl hostImpl;
fs::FS Dysk(FSImplPtr(&hostImpl));

}  // namespace fs
