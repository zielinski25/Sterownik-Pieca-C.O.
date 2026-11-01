enum {
  PARSE_REQ_START = 0,
  PARSE_REQ_HEADERS = 1,
  PARSE_REQ_BODY = 2,
  PARSE_REQ_END = 3,
  PARSE_REQ_FAIL = 4
};

enum {
  CHUNK_NONE = 0,   // Body transfer encoding is not chunked
  CHUNK_LENGTH,     // Getting chunk length - HHHH[;...] CR LF
  CHUNK_EXTENSION,  // Getting chunk extension - ;... CR LF
  CHUNK_DATA,       // Handling chunk data
  CHUNK_ERROR,      // Invalid chunk header
  CHUNK_END,        // Getting chunk end marker  - CR LF
};

namespace AsyncWebRequestMethod {
// The long name here is because we sometimes include this in the global namespace
enum AsyncWebRequestMethodType : uint32_t {
  HTTP_UNKNOWN = 0u,

  HTTP_DELETE = 1u << 0,
  HTTP_GET = 1u << 1,
  HTTP_HEAD = 1u << 2,
  HTTP_POST = 1u << 3,
  HTTP_PUT = 1u << 4,

  /* pathological */
  HTTP_CONNECT = 1u << 5,
  HTTP_OPTIONS = 1u << 6,
  HTTP_TRACE = 1u << 7,

  /* WebDAV */
  HTTP_COPY = 1u << 8,
  HTTP_LOCK = 1u << 9,
  HTTP_MKCOL = 1u << 10,
  HTTP_MOVE = 1u << 11,
  HTTP_PROPFIND = 1u << 12,
  HTTP_PROPPATCH = 1u << 13,
  HTTP_SEARCH = 1u << 14,
  HTTP_UNLOCK = 1u << 15,
  HTTP_BIND = 1u << 16,
  HTTP_REBIND = 1u << 17,
  HTTP_UNBIND = 1u << 18,
  HTTP_ACL = 1u << 19,

  /* subversion */
  // HTTP_REPORT
  // HTTP_MKACTIVITY
  // HTTP_CHECKOUT
  // HTTP_MERGE

  /* upnp */
  // HTTP_MSEARCH
  // HTTP_NOTIFY
  // HTTP_SUBSCRIBE
  // HTTP_UNSUBSCRIBE

  /* RFC-5789 */
  HTTP_PATCH = 1u << 20,
  HTTP_PURGE = 1u << 21,

  /* CalDAV */
  // HTTP_MKCALENDAR

  /* RFC-2068, section 19.6.1.2 */
  HTTP_LINK = 1u << 22,
  HTTP_UNLINK = 1u << 23,

  /* RFC 10008 */
  HTTP_QUERY = 1u << 24,

  /* icecast */
  // HTTP_SOURCE

  HTTP_INVALID = 1u << 31  // Sentinel
};

};  // namespace AsyncWebRequestMethod

typedef AsyncWebRequestMethod::AsyncWebRequestMethodType WebRequestMethod;

typedef enum {
  RCT_HTTP = 1,
  RCT_WS = 2,
  RCT_EVENT = 3
} RequestedConnectionType;

typedef enum {
  AUTH_NONE = 0,  // always allow
  AUTH_BASIC = 1,
  AUTH_DIGEST = 2,
  AUTH_BEARER = 3,
  AUTH_OTHER = 4,
  AUTH_DENIED = 255,  // always returns 401
} AsyncAuthType;
