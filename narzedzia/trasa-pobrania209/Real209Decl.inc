// LGPL-3.0-or-later, ESP32Async/ESPAsyncWebServer pinned c47917dcaba2acefb04277cd679f58a60b4fab30.
// Wyciagniete bez zmian przez build.py; kazdy fragment jest bajtowo rowny plikowi zrodlowemu.
// Czlon 1: klasy WebRequestMethodComposite i AsyncURIMatcher z naglowka.
class WebRequestMethodComposite {
  uint32_t mask;

private:
  constexpr WebRequestMethodComposite(uint32_t m) : mask(m){};

public:
  // Default constructor: by default, matches nothing
  constexpr WebRequestMethodComposite() : mask(0){};

  // Constructor: allows implicit conversion from WebRequestMethod
  constexpr WebRequestMethodComposite(WebRequestMethod m) : mask(static_cast<uint32_t>(m)){};

  // Combine composites
  constexpr inline WebRequestMethodComposite operator|(const WebRequestMethodComposite &r) const {
    return WebRequestMethodComposite(mask | r.mask);
  };

  // == operator for composite
  constexpr inline bool operator==(const WebRequestMethodComposite &r) const {
    return mask == r.mask;
  };

  constexpr inline bool operator!=(const WebRequestMethodComposite &r) const {
    return mask != r.mask;
  };

  // Check for a match
  constexpr inline bool matches(WebRequestMethod m) const {
    return mask & static_cast<uint32_t>(m);
  };

  constexpr inline bool operator&(WebRequestMethod m) const {
    return matches(m);
  }

  // Super cool feature: integration with platform `http_method` enum
#ifdef ASYNCWEBSERVER_HTTP_METHOD_INTEGRATION

// Conversion function for integration with external libraries.
// Horrible ternary implementation for C++11 compatibility.
#define MAP_EXTERNAL_TERNARY(x) (t == http_method::x) ? static_cast<uint32_t>(WebRequestMethod::x)
  constexpr static inline uint32_t map_http_method(http_method t) {
    return MAP_EXTERNAL_TERNARY(HTTP_DELETE)
      : MAP_EXTERNAL_TERNARY(HTTP_GET)
      : MAP_EXTERNAL_TERNARY(HTTP_HEAD)
      : MAP_EXTERNAL_TERNARY(HTTP_POST)
      : MAP_EXTERNAL_TERNARY(HTTP_PUT)
      : MAP_EXTERNAL_TERNARY(HTTP_CONNECT)
      : MAP_EXTERNAL_TERNARY(HTTP_OPTIONS)
      : MAP_EXTERNAL_TERNARY(HTTP_TRACE)
      : MAP_EXTERNAL_TERNARY(HTTP_COPY)
      : MAP_EXTERNAL_TERNARY(HTTP_LOCK)
      : MAP_EXTERNAL_TERNARY(HTTP_MKCOL)
      : MAP_EXTERNAL_TERNARY(HTTP_MOVE)
      : MAP_EXTERNAL_TERNARY(HTTP_PROPFIND)
      : MAP_EXTERNAL_TERNARY(HTTP_PROPPATCH)
      : MAP_EXTERNAL_TERNARY(HTTP_SEARCH)
      : MAP_EXTERNAL_TERNARY(HTTP_UNLOCK)
      : MAP_EXTERNAL_TERNARY(HTTP_BIND)
      : MAP_EXTERNAL_TERNARY(HTTP_REBIND)
      : MAP_EXTERNAL_TERNARY(HTTP_UNBIND)
      : MAP_EXTERNAL_TERNARY(HTTP_ACL)
      : MAP_EXTERNAL_TERNARY(HTTP_PATCH)
      : MAP_EXTERNAL_TERNARY(HTTP_PURGE)
      : MAP_EXTERNAL_TERNARY(HTTP_LINK)
      : MAP_EXTERNAL_TERNARY(HTTP_UNLINK)
#if defined(HTTP_ANY)
      : (t == HTTP_ANY) ? static_cast<uint32_t>(WebRequestMethod::HTTP_INVALID) - 1
#endif
                        : static_cast<uint32_t>(WebRequestMethod::HTTP_INVALID);
  }
#undef MAP_EXTERNAL_TERNARY

  constexpr WebRequestMethodComposite(http_method m) : mask(map_http_method(m)){};
#endif
};  // WebRequestMethodComposite

// Operator| for WebRequestMethod: combine to a WebRequestMethodComposite
constexpr inline WebRequestMethodComposite operator|(WebRequestMethod l, WebRequestMethod r) {
  return static_cast<WebRequestMethodComposite>(l) | r;
};
class AsyncURIMatcher {
private:
  // Matcher types are internal, not part of public API
  enum class Type {
    None,                // default state: matcher does not match anything
    All,                 // matches everything
    Exact,               // matches equivalent to regex: ^{_uri}$
    Prefix,              // matches equivalent to regex: ^{_uri}.*
    Extension,           // non-regular match: /pattern../*.ext
    BackwardCompatible,  // matches equivalent to regex: ^{_uri}(/.*)?$
    Regex,               // matches _url as regex
  };

public:
  /**
   * @brief No special matching behavior (default)
   */
  static constexpr uint16_t None = 0;

  /**
   * @brief Enable case-insensitive URI matching
   *
   * When CaseInsensitive is specified:
   * - The URI pattern is converted to lowercase during construction
   * - Incoming request URLs are converted to lowercase before matching
   * - For regex matchers, the std::regex::icase flag is used
   *
   * Example usage:
   * ```cpp
   * // Matches /login, /LOGIN, /Login, /LoGiN, etc.
   * server.on(AsyncURIMatcher::exact("/login", AsyncURIMatcher::CaseInsensitive), handler);
   *
   * // Matches /api/\*, /API/\*, /Api/\*, etc.
   * server.on(AsyncURIMatcher::prefix("/api", AsyncURIMatcher::CaseInsensitive), handler);
   *
   * // Regex with case insensitive matching
   * server.on(AsyncURIMatcher::regex("^/user/([a-z]+)$", AsyncURIMatcher::CaseInsensitive), handler);
   * ```
   *
   * Performance note: Case conversion adds minimal overhead during construction and matching.
   */
  static constexpr uint16_t CaseInsensitive = (1 << 0);

  // public constructors
  AsyncURIMatcher() : AsyncURIMatcher({}, Type::None, None) {}
  AsyncURIMatcher(const char *uri, uint16_t modifiers = None) : AsyncURIMatcher(String(uri), modifiers) {}
  AsyncURIMatcher(String uri, uint16_t modifiers = None);

#ifdef ASYNCWEBSERVER_REGEX
  AsyncURIMatcher(const AsyncURIMatcher &c);
  AsyncURIMatcher(AsyncURIMatcher &&c);
  ~AsyncURIMatcher();

  AsyncURIMatcher &operator=(const AsyncURIMatcher &r);
  AsyncURIMatcher &operator=(AsyncURIMatcher &&r);

#else
  AsyncURIMatcher(const AsyncURIMatcher &) = default;
  AsyncURIMatcher(AsyncURIMatcher &&) = default;
  ~AsyncURIMatcher() = default;

  AsyncURIMatcher &operator=(const AsyncURIMatcher &) = default;
  AsyncURIMatcher &operator=(AsyncURIMatcher &&) = default;
#endif

  bool matches(AsyncWebServerRequest *request) const;

  // static factory methods for common match types:
  // - AsyncURIMatcher::all() - matches everything
  // - AsyncURIMatcher::none() - matches nothing
  // - AsyncURIMatcher::exact(uri, modifiers) - exact match
  // - AsyncURIMatcher::prefix(uri, modifiers) - prefix match
  // - AsyncURIMatcher::dir(uri, modifiers) - directory/folder match (trailing slash added automatically)
  // - AsyncURIMatcher::ext(uri, modifiers) - extension match (pattern with wildcard)
  // - AsyncURIMatcher::regex(uri, modifiers) - regex match (requires ASYNCWEBSERVER_REGEX)

  /**
   * @brief Create a matcher that matches all URIs unconditionally
   * @return AsyncURIMatcher that accepts any request URL
   *
   * Usage: server.on(AsyncURIMatcher::all(), handler);
   */
  static inline AsyncURIMatcher all() {
    return AsyncURIMatcher{{}, Type::All, None};
  }

  /**
   * @brief Create a matcher that matches no URIs (never matches)
   * @return AsyncURIMatcher that rejects all request URLs
   *
   * Usage: server.on(AsyncURIMatcher::none(), handler);
   */
  static inline AsyncURIMatcher none() {
    return AsyncURIMatcher{{}, Type::None, None};
  }

  /**
   * @brief Create an exact URI matcher
   * @param c The exact URI string to match (e.g., "/login", "/api/status")
   * @param modifiers Optional modifiers (CaseInsensitive, etc.)
   * @return AsyncURIMatcher that matches only the exact URI
   *
   * Usage: server.on(AsyncURIMatcher::exact("/login"), handler);
   * Matches: "/login"
   * Doesn't match: "/login/", "/login-page"
   * Doesn't match: "/LOGIN" (unless CaseInsensitive flag used)
   */
  static inline AsyncURIMatcher exact(String c, uint16_t modifiers = None) {
    return AsyncURIMatcher{std::move(c), Type::Exact, modifiers};
  }

  /**
   * @brief Create a prefix URI matcher
   * @param c The URI prefix to match (e.g., "/api", "/static")
   * @param modifiers Optional modifiers (CaseInsensitive, etc.)
   * @return AsyncURIMatcher that matches URIs starting with the prefix
   *
   * Usage: server.on(AsyncURIMatcher::prefix("/api"), handler);
   * Matches: "/api", "/api/users", "/api-v2", "/apitest"
   * Note: This is pure prefix matching - does NOT require folder separator
   */
  static inline AsyncURIMatcher prefix(String c, uint16_t modifiers = None) {
    return AsyncURIMatcher{std::move(c), Type::Prefix, modifiers};
  }

  /**
   * @brief Create a directory/folder URI matcher
   * @param c The directory path (trailing slash automatically added if missing)
   * @param modifiers Optional modifiers (CaseInsensitive, etc.)
   * @return AsyncURIMatcher that matches URIs under the directory
   *
   * Usage: server.on(AsyncURIMatcher::dir("/admin"), handler);
   * Matches: "/admin/users", "/admin/settings", "/admin/sub/path"
   * Doesn't match: "/admin" (exact), "/admin-panel" (no folder separator)
   *
   * The trailing slash is automatically added for convenience and efficiency.
   */
  static inline AsyncURIMatcher dir(String c, uint16_t modifiers = None) {
    // Pre-calculate folder for efficiency
    if (!c.length()) {
      return AsyncURIMatcher{"/", Type::Prefix, modifiers};
    }
    if (c[c.length() - 1] != '/') {
      c.concat('/');
    }
    return AsyncURIMatcher{std::move(c), Type::Prefix, modifiers};
  }

  /**
   * @brief Create a file extension URI matcher
   * @param c The pattern with wildcard extension (e.g., "/images/\*.jpg", "/docs/\*.pdf")
   * @param modifiers Optional modifiers (CaseInsensitive, etc.)
   * @return AsyncURIMatcher that matches files with specific extensions under a path
   *
   * Usage: server.on(AsyncURIMatcher::ext("/images/\*.jpg"), handler);
   * Matches: "/images/photo.jpg", "/images/gallery/pic.jpg"
   * Doesn't match: "/images/photo.png", "/img/photo.jpg"
   *
   * Pattern format: "/path/\*.extension" where "*" is a literal wildcard placeholder.
   * The path before "/\*." must match exactly, and the URI must end with the extension.
   */
  static inline AsyncURIMatcher ext(String c, uint16_t modifiers = None) {
    return AsyncURIMatcher{std::move(c), Type::Extension, modifiers};
  }

#ifdef ASYNCWEBSERVER_REGEX
  /**
   * @brief Create a regular expression URI matcher
   * @param c The regex pattern string (e.g., "^/user/([0-9]+)$", "^/blog/([0-9]{4})/([0-9]{2})$")
   * @param modifiers Optional modifiers (CaseInsensitive applies to regex compilation)
   * @return AsyncURIMatcher that matches URIs using regex with capture groups
   *
   * Usage: server.on(AsyncURIMatcher::regex("^/user/([0-9]+)$"), handler);
   * Matches: "/user/123", "/user/456"
   * Doesn't match: "/user/abc", "/user/123/profile"
   *
   * Captured groups can be accessed via request->pathArg(index) in the handler.
   * Requires ASYNCWEBSERVER_REGEX to be defined during compilation.
   * Performance note: Regex matching is slower than other match types.
   */
  static inline AsyncURIMatcher regex(String c, uint16_t modifiers = None) {
    return AsyncURIMatcher{std::move(c), Type::Regex, modifiers};
  }
#endif

private:
  // fields
  String _value;
  union {
    intptr_t _flags;  // type and flags packed together
#ifdef ASYNCWEBSERVER_REGEX
    // Overlay the pattern pointer storage with the type.  It is treated as a tagged pointer:
    // if any of the LSBs are set, it stores type, as a valid object must be aligned and so
    // none of the LSBs can be set in a valid pointer.
    std::regex *pattern;
#endif
  };

  // private constructor called from static factory methods
  AsyncURIMatcher(String uri, Type type, uint16_t modifiers);

#ifdef ASYNCWEBSERVER_REGEX
  inline bool _isRegex() const {
    static_assert(
      (std::alignment_of<std::regex>::value % 2) == 0, "Unexpected regex type alignment - please let the ESPAsyncWebServer team know about your platform!"
    );
    // pattern is non-null pointer with correct alignment.
    // We use the _flags view as it's already a integer type.
    return _flags && !(_flags & (std::alignment_of<std::regex>::value - 1));
  }
#endif

  static constexpr intptr_t _toFlags(Type type, uint16_t modifiers) {
    // Use lsb to disambiguate from regex pointer in the case where someone has regex activated but uses a non-regex type.
    // We always do this shift, even if regex is not enabled, to keep the layout identical and also catch programmatic errors earlier.
    // For example a mistake is to set a modifier flag to (1 << 15), which is the msb of the uint16_t.
    // This msb is discarded during this shift operation.
    // So pay attention to not have more than 15 modifier flags.
    return ((uint32_t(modifiers) << 16 | uint16_t(type)) << 1) + 1;
  }

  static constexpr std::tuple<Type, uint16_t> _fromFlags(intptr_t in_flags) {
    // shift off disambiguation bit
    // - Type is lower 16 bits
    // - Modifiers are upper 16 bits
    return std::make_tuple(static_cast<Type>((in_flags >> 1) & 0xFFFF), (in_flags >> 1) >> 16);
  }
};
