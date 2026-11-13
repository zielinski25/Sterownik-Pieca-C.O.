// Licznik sterty dla 32-bitowego modelu firmware'u panelu (pomiar na PC).
// Zastepuje malloc/calloc/realloc/free calego programu wlasnym, prostym
// alokatorem i dla kazdego bloku przydzielonego w watku firmware'u pamieta
// rozmiar oraz stos wywolan. Dzieki temu widac, ILE pamieci trzyma firmware
// i KTO ja wzial.
//
// "Bajty ESP" = ile ten sam blok zajmuje na sterycie plytki z arduino-esp32
// 3.x (ESP-IDF 5): do rozmiaru dochodzi 12 bajtow "zatruwania" sterty
// (CONFIG_HEAP_POISONING_LIGHT=1 w gotowych bibliotekach arduino-esp32:
// 8 bajtow przed blokiem i 4 za nim - multi_heap_poisoning.c), calosc jest
// zaokraglana do 4 bajtow, a alokator TLSF dodaje 4 bajty naglowka. Razem:
// rozmiar zaokraglony w gore do 4 bajtow + 16. Przy ok. 2500 blokach, ktore
// trzyma ten firmware, sam narzut to ok. 40 kB.
#define _GNU_SOURCE
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#define MAKS_RAMEK 24
#define MAGIA 0x51EDFACEu

typedef struct Naglowek {
  uint32_t rozmiar;      // o ile prosil program
  uint32_t klasa;        // indeks listy wolnych blokow; 0xFFFFFFFF = osobny mmap
  uint32_t stos;         // indeks w tablicy stosow + 1; 0 = blok nieliczony
  uint32_t magia;
} Naglowek;

typedef struct Stos {
  uint32_t skrot;
  uint32_t n;
  void* ramki[MAKS_RAMEK];
  uint32_t zywe_bloki;
  uint32_t zywe_bajty;
  uint32_t zywe_esp;
  uint32_t szczyt_esp;   // najwiecej "bajtow ESP" naraz od ostatniego pomiar_zeruj_szczyty()
  uint64_t lacznie_blokow;
} Stos;

#define ILE_STOSOW (1u << 16)
static Stos g_stosy[ILE_STOSOW];

static atomic_flag g_zamek = ATOMIC_FLAG_INIT;
static void zamknij(void) { while (atomic_flag_test_and_set_explicit(&g_zamek, memory_order_acquire)) { } }
static void otworz(void) { atomic_flag_clear_explicit(&g_zamek, memory_order_release); }

// --- alokator: listy wolnych blokow wg klas rozmiaru ------------------------
#define KLAS_MALYCH 257                 // 16, 32, ... 4096 bajtow (z naglowkiem)
#define KLAS_DUZYCH 12                  // 8 kB, 16 kB, ... 16 MB
static void* g_wolne[KLAS_MALYCH + KLAS_DUZYCH];
static char* g_kawalek;
static size_t g_kawalek_zostalo;

static uint32_t klasa_dla(size_t calosc, size_t* rozmiar_klasy) {
  if (calosc <= 4096) {
    uint32_t k = (uint32_t)((calosc + 15) / 16);
    *rozmiar_klasy = (size_t)k * 16;
    return k;
  }
  size_t r = 8192;
  uint32_t k = KLAS_MALYCH;
  while (r < calosc && k < KLAS_MALYCH + KLAS_DUZYCH - 1) { r *= 2; k++; }
  if (r < calosc) { *rozmiar_klasy = 0; return 0xFFFFFFFFu; }
  *rozmiar_klasy = r;
  return k;
}

static void* surowy_przydzial(size_t rozmiar, uint32_t* klasa_wy) {
  size_t rk;
  const size_t calosc = rozmiar + sizeof(Naglowek);
  const uint32_t k = klasa_dla(calosc, &rk);
  *klasa_wy = k;
  if (k == 0xFFFFFFFFu) {
    const size_t strony = (calosc + 4095) & ~(size_t)4095;
    void* p = mmap(NULL, strony, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
  }
  if (g_wolne[k] != NULL) {
    void* p = g_wolne[k];
    g_wolne[k] = *(void**)p;
    return p;
  }
  if (g_kawalek_zostalo < rk) {
    const size_t ile = rk > (4u << 20) ? rk : (4u << 20);
    void* p = mmap(NULL, ile, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;
    g_kawalek = (char*)p;
    g_kawalek_zostalo = ile;
  }
  void* p = g_kawalek;
  g_kawalek += rk;
  g_kawalek_zostalo -= rk;
  return p;
}

// --- liczenie ---------------------------------------------------------------
static __thread int t_wstrzymany;       // >0: przydzialy atrap albo innego watku, nie firmware'u
static uintptr_t g_stos_dol, g_stos_gora;
static int g_granice_znane;

// Granice stosu watku glownego z /proc/self/maps (bez malloc).
#include <fcntl.h>
static void ustal_granice_stosu(void) {
  static char buf[1 << 16];
  g_granice_znane = 1;
  const int fd = open("/proc/self/maps", O_RDONLY);
  if (fd < 0) return;
  size_t n = 0;
  for (;;) {
    const ssize_t r = read(fd, buf + n, sizeof(buf) - 1 - n);
    if (r <= 0) break;
    n += (size_t)r;
    if (n >= sizeof(buf) - 1) break;
  }
  close(fd);
  buf[n] = 0;
  char* p = strstr(buf, "[stack]");
  if (p == NULL) return;
  while (p > buf && p[-1] != '\n') p--;
  uintptr_t a = 0, b = 0;
  while (*p && *p != '-') { a = a * 16 + (uintptr_t)(*p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10); p++; }
  if (*p == '-') p++;
  while (*p && *p != ' ') { b = b * 16 + (uintptr_t)(*p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10); p++; }
  // Stos rosnie w dol i jadro powieksza mapowanie w miare potrzeb - dolna
  // granica z zapasem 16 MB.
  g_stos_gora = b;
  g_stos_dol = b > (16u << 20) ? b - (16u << 20) : 4096;
  (void)a;
}

static uint32_t g_zywe_bloki, g_zywe_bajty, g_zywe_esp, g_szczyt_esp;
// Sztuczny limit: gdy > 0, przydzial, po ktorym "bajty ESP" przekroczylyby
// limit, zwraca NULL - tak jak malloc na plytce, ktorej skonczyla sie pamiec.
static uint32_t g_limit_esp;
static uint32_t g_odmow;
// >0: przydzial jest liczony, ale limit go nie dotyczy. Tak przydziela atrapa
// String (std::string przez operator new): odmowa konczylaby program wyjatkiem
// std::bad_alloc, a prawdziwy String z Arduino przy braku pamieci zostaje pusty.
static __thread int t_bez_limitu;

static uint32_t esp_bajty(size_t r) {
  return (uint32_t)((r + 12 + 3) & ~(size_t)3) + 4;
}

static uint32_t zlap_stos(void** wy) {
  void** r = (void**)__builtin_frame_address(0);
  uint32_t n = 0;
  while (n < MAKS_RAMEK) {
    if ((uintptr_t)r < g_stos_dol || (uintptr_t)r + 8 > g_stos_gora || ((uintptr_t)r & 3)) break;
    void* powrot = r[1];
    if (powrot == NULL) break;
    wy[n++] = powrot;
    void** nast = (void**)r[0];
    if (nast <= r) break;
    r = nast;
  }
  return n;
}

static uint32_t znajdz_stos(void** ramki, uint32_t n) {
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < n; i++) { h ^= (uint32_t)(uintptr_t)ramki[i]; h *= 16777619u; }
  if (h == 0) h = 1;
  uint32_t i = h & (ILE_STOSOW - 1);
  for (uint32_t proba = 0; proba < ILE_STOSOW; proba++) {
    Stos* s = &g_stosy[i];
    if (s->skrot == 0) {
      s->skrot = h;
      s->n = n;
      memcpy(s->ramki, ramki, n * sizeof(void*));
      return i + 1;
    }
    if (s->skrot == h && s->n == n && memcmp(s->ramki, ramki, n * sizeof(void*)) == 0) return i + 1;
    i = (i + 1) & (ILE_STOSOW - 1);
  }
  return 0;
}

static void* przydziel(size_t rozmiar) {
  void* ramki[MAKS_RAMEK];
  uint32_t n = 0;
  const int licz = !t_wstrzymany;
  if (licz) {
    if (!g_granice_znane) ustal_granice_stosu();
    n = zlap_stos(ramki);
  }
  zamknij();
  if (licz && g_limit_esp > 0 && !t_bez_limitu && g_zywe_esp + esp_bajty(rozmiar) > g_limit_esp) {
    g_odmow++;
    otworz();
    errno = ENOMEM;
    return NULL;
  }
  uint32_t klasa;
  Naglowek* h = (Naglowek*)surowy_przydzial(rozmiar, &klasa);
  if (h == NULL) { otworz(); return NULL; }
  h->rozmiar = (uint32_t)rozmiar;
  h->klasa = klasa;
  h->magia = MAGIA;
  h->stos = 0;
  if (licz) {
    const uint32_t s = znajdz_stos(ramki, n);
    h->stos = s;
    const uint32_t e = esp_bajty(rozmiar);
    g_zywe_bloki++;
    g_zywe_bajty += (uint32_t)rozmiar;
    g_zywe_esp += e;
    if (g_zywe_esp > g_szczyt_esp) g_szczyt_esp = g_zywe_esp;
    if (s) {
      Stos* st = &g_stosy[s - 1];
      st->zywe_bloki++;
      st->zywe_bajty += (uint32_t)rozmiar;
      st->zywe_esp += e;
      if (st->zywe_esp > st->szczyt_esp) st->szczyt_esp = st->zywe_esp;
      st->lacznie_blokow++;
    }
  }
  otworz();
  return h + 1;
}

static void zwolnij(void* p) {
  if (p == NULL) return;
  Naglowek* h = (Naglowek*)p - 1;
  if (h->magia != MAGIA) {
    static const char kom[] = "pomiar_sterty: free() obcego albo zwolnionego juz bloku\n";
    (void)!write(2, kom, sizeof(kom) - 1);
    _exit(70);
  }
  zamknij();
  h->magia = 0;
  if (h->stos) {
    const uint32_t e = esp_bajty(h->rozmiar);
    g_zywe_bloki--;
    g_zywe_bajty -= h->rozmiar;
    g_zywe_esp -= e;
    Stos* st = &g_stosy[h->stos - 1];
    st->zywe_bloki--;
    st->zywe_bajty -= h->rozmiar;
    st->zywe_esp -= e;
  }
  if (h->klasa == 0xFFFFFFFFu) {
    const size_t strony = (h->rozmiar + sizeof(Naglowek) + 4095) & ~(size_t)4095;
    otworz();
    munmap(h, strony);
    return;
  }
  *(void**)h = g_wolne[h->klasa];
  g_wolne[h->klasa] = h;
  otworz();
}

void* malloc(size_t n) { return przydziel(n ? n : 1); }
void free(void* p) { zwolnij(p); }
void* calloc(size_t a, size_t b) {
  if (b != 0 && a > (size_t)-1 / b) { errno = ENOMEM; return NULL; }
  void* p = przydziel(a * b ? a * b : 1);
  if (p != NULL) memset(p, 0, a * b);
  return p;
}
void* realloc(void* p, size_t n) {
  if (p == NULL) return przydziel(n ? n : 1);
  if (n == 0) { zwolnij(p); return NULL; }
  Naglowek* h = (Naglowek*)p - 1;
  // Zmniejszenie bloku: w miejscu i zawsze z powodzeniem - tak robi sterta
  // ESP-IDF (TLSF). Ma to znaczenie przy limicie pamieci (pomiar_limit()):
  // ArduinoJson 7 konczy kazdy tekst realloc() w dol i nie sprawdza wyniku,
  // wiec "odmowa" zmniejszenia - na plytce niemozliwa - wywracala model.
  if (n <= h->rozmiar && h->klasa != 0xFFFFFFFFu && h->magia == MAGIA) {
    zamknij();
    if (h->stos) {
      const uint32_t e_stare = esp_bajty(h->rozmiar), e_nowe = esp_bajty(n);
      const uint32_t roznica = h->rozmiar - (uint32_t)n;
      g_zywe_bajty -= roznica;
      g_zywe_esp -= e_stare - e_nowe;
      Stos* st = &g_stosy[h->stos - 1];
      st->zywe_bajty -= roznica;
      st->zywe_esp -= e_stare - e_nowe;
    }
    h->rozmiar = (uint32_t)n;
    otworz();
    return p;
  }
  void* q = przydziel(n);
  if (q == NULL) return NULL;          // stary blok zostaje, jak w prawdziwym realloc
  memcpy(q, p, h->rozmiar < n ? h->rozmiar : n);
  zwolnij(p);
  return q;
}
// Wyrownane przydzialy: program ich nie uzywa (sprawdzane przy linkowaniu -
// gdyby byly potrzebne, linker zglosi brak symbolu w tej wersji libc).
int posix_memalign(void** wy, size_t wyrownanie, size_t n) {
  if (wyrownanie <= 16) { *wy = przydziel(n ? n : 1); return *wy ? 0 : ENOMEM; }
  return ENOMEM;
}
void* aligned_alloc(size_t wyrownanie, size_t n) { void* p = NULL; return posix_memalign(&p, wyrownanie, n) == 0 ? p : NULL; }
size_t malloc_usable_size(void* p) { return p ? ((Naglowek*)p - 1)->rozmiar : 0; }

// --- interfejs dla symulatora -----------------------------------------------
void pomiar_start(void) { }
void pomiar_wstrzymaj(int o_ile) { t_wstrzymany += o_ile; }
void pomiar_limit(uint32_t esp_bajtow) { zamknij(); g_limit_esp = esp_bajtow; otworz(); }
void pomiar_bez_limitu(int o_ile) { t_bez_limitu += o_ile; }
uint32_t pomiar_odmow(void) { return g_odmow; }
uint32_t pomiar_zywe_esp(void) { return g_zywe_esp; }
uint32_t pomiar_zywe_bajty(void) { return g_zywe_bajty; }
uint32_t pomiar_zywe_bloki(void) { return g_zywe_bloki; }
uint32_t pomiar_szczyt_esp(void) { return g_szczyt_esp; }
void pomiar_zeruj_szczyty(void) {
  zamknij();
  g_szczyt_esp = g_zywe_esp;
  for (uint32_t i = 0; i < ILE_STOSOW; i++)
    if (g_stosy[i].skrot) g_stosy[i].szczyt_esp = g_stosy[i].zywe_esp;
  otworz();
}

// Zapisuje stosy, ktore trzymaja pamiec (tryb 0) albo trzymaly ja w szczycie
// (tryb 1): "esp bajty bloki lacznie ramka ramka ..." - po jednym na linie.
void pomiar_zrzut(const char* plik, int tryb) {
  FILE* f = fopen(plik, "w");
  if (f == NULL) return;
  t_wstrzymany++;
  for (uint32_t i = 0; i < ILE_STOSOW; i++) {
    const Stos* s = &g_stosy[i];
    if (!s->skrot) continue;
    const uint32_t esp = tryb ? s->szczyt_esp : s->zywe_esp;
    if (esp == 0) continue;
    fprintf(f, "%u %u %u %llu", esp, s->zywe_bajty, s->zywe_bloki, (unsigned long long)s->lacznie_blokow);
    for (uint32_t k = 0; k < s->n; k++) fprintf(f, " %p", s->ramki[k]);
    fprintf(f, "\n");
  }
  fclose(f);
  t_wstrzymany--;
}

// --- awaria modelu (SIGSEGV itp.) ---------------------------------------------
// Gdy program modelu siega pod zly adres (na plytce: wyjatek procesora i
// restart), wypisuje adresy stosu wywolan - pomiar_pamieci.sh zamienia je na
// nazwy funkcji (addr2line). Bez malloc i bez stdio.
#include <signal.h>
#include <ucontext.h>
static void pisz_hex(uintptr_t v) {
  char b[2 + 2 * sizeof(v) + 1];
  int n = 0;
  b[n++] = ' '; b[n++] = '0'; b[n++] = 'x';
  for (int i = (int)(2 * sizeof(v)) - 1; i >= 0; i--) b[n++] = "0123456789abcdef"[(v >> (4 * i)) & 0xf];
  (void)!write(1, b, (size_t)n);
}
static void awaria(int sygnal, siginfo_t* info, void* kontekst) {
  static const char pocz[] = "\nMODEL-AWARIA sygnal=";
  (void)!write(1, pocz, sizeof(pocz) - 1);
  char c = (char)('0' + (sygnal / 10) % 10); (void)!write(1, &c, 1);
  c = (char)('0' + sygnal % 10); (void)!write(1, &c, 1);
  static const char adr[] = " adres="; (void)!write(1, adr, sizeof(adr) - 1);
  pisz_hex((uintptr_t)info->si_addr);
  static const char st[] = " stos="; (void)!write(1, st, sizeof(st) - 1);
#if defined(__i386__)
  const ucontext_t* u = (const ucontext_t*)kontekst;
  pisz_hex((uintptr_t)u->uc_mcontext.gregs[REG_EIP]);
  void** r = (void**)(uintptr_t)u->uc_mcontext.gregs[REG_EBP];
  for (int i = 0; i < 40; i++) {
    if ((uintptr_t)r < g_stos_dol || (uintptr_t)r + 8 > g_stos_gora || ((uintptr_t)r & 3)) break;
    if (r[1] == NULL) break;
    pisz_hex((uintptr_t)r[1]);
    void** nast = (void**)r[0];
    if (nast <= r) break;
    r = nast;
  }
#else
  (void)kontekst;
#endif
  (void)!write(1, "\n", 1);
  _exit(139);
}
void pomiar_lap_awarie(void) {
  if (!g_granice_znane) ustal_granice_stosu();
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = awaria;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
}
