// LGPL-3.0-or-later: exact pinned request/header bodies plus candidate 205/206 additions.
static inline const char *skip_ows(const char *p, const char *end) {
  while (p < end && (*p == ' ' || *p == '\t')) {
    p++;
  }
  return p;
}

static inline const char *rtrim_ows(const char *start, const char *end) {
  while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) {
    end--;
  }
  return end;
}

void AsyncWebServerRequest::_addGetParams(const String &params) {
  size_t start = 0;
  while (start < params.length()) {
    int end = params.indexOf('&', start);
    if (end < 0) {
      end = params.length();
    }
    int equal = params.indexOf('=', start);
    if (equal < 0 || equal > end) {
      equal = end;
    }
    String name = urlDecode(params.substring(start, equal));
    String value = urlDecode(equal + 1 < end ? params.substring(equal + 1, end) : asyncsrv::emptyString);
    if (name.length()) {
      _params.emplace_back(name, value);
    }
    start = end + 1;
  }
}
bool AsyncWebServerRequest::_parseReqHead() {
  // Split the head into method, url and version
  int index = _temp.indexOf(' ');
  String m = _temp.substring(0, index);
  index = _temp.indexOf(' ', index + 1);
  String u = _temp.substring(m.length() + 1, index);
  _temp = _temp.substring(index + 1);

  _method = asyncsrv::stringToMethod(m);
  if (_method == AsyncWebRequestMethod::HTTP_INVALID) {
    return false;
  }

  String g;
  index = u.indexOf('?');
  if (index > 0) {
    g = u.substring(index + 1);
    u = u.substring(0, index);
  }
  _url = urlDecode(u);
  _addGetParams(g);

  if (!_url.length()) {
    return false;
  }

  if (!_temp.startsWith(T_HTTP_1_0)) {
    _version = 1;
  }

  _temp = asyncsrv::emptyString;
  return true;
}
String AsyncWebServerRequest::urlDecode(const String &text) const {
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  String decoded;
  // Allocate the string internal buffer - never longer from source text
  if (!decoded.reserve(len)) {
    async_ws_log_e("Failed to allocate");
    return asyncsrv::emptyString;
  }
  while (i < len) {
    char decodedChar;
    char encodedChar = text.charAt(i++);
    if ((encodedChar == '%') && (i + 1 < len)) {
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);
      decodedChar = strtol(temp, NULL, 16);
    } else if (encodedChar == '+') {
      decodedChar = ' ';
    } else {
      decodedChar = encodedChar;  // normal ascii char
    }
    decoded.concat(decodedChar);
  }
  return decoded;
}
bool AsyncWebServerRequest::_parseReqHeader() {
  AsyncWebHeader header = AsyncWebHeader::parse(_temp);
  if (header) {
    const String &name = header.name();
    const String &value = header.value();
    if (name.equalsIgnoreCase(T_Host)) {
      _host = value;
    } else if (name.equalsIgnoreCase(T_Content_Type)) {
      _contentType = value.substring(0, value.indexOf(';'));
      // Trim _contentType defensively; AsyncWebHeader::parse now strips all
      // leading OWS per RFC 7230, but trim() guards against any future change.
      _contentType.trim();
      if (strncasecmp(value.c_str(), T_MULTIPART_, sizeof(T_MULTIPART_) - 1) == 0) {
        // Delimiter-structured, quote-aware search for the boundary parameter.
        // We scan parameter by parameter (';'-delimited) so that 'boundary='
        // inside a quoted value (e.g. foo="x; boundary=y") is never mistaken
        // for the real parameter.  A ';' inside a quoted-string is not a
        // parameter separator per RFC 2045 §5.1.
        const char *p = value.c_str();
        const char *end = p + value.length();

        // Advance past the media-type to the first ';'
        while (p < end && *p != ';') {
          p++;
        }

        const char *bvdata = nullptr;
        while (p < end) {
          p++;  // consume ';'
          p = skip_ows(p, end);
          if ((size_t)(end - p) > T_BOUNDARY_LEN && strncasecmp(p, T_BOUNDARY, T_BOUNDARY_LEN) == 0) {
            bvdata = p + T_BOUNDARY_LEN;
            break;
          }
          // Advance to the next ';', skipping over any quoted-string value
          while (p < end && *p != ';') {
            if (*p++ == '"') {
              while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) {
                  p++;  // skip quoted-pair
                }
                p++;
              }
              if (p < end) {
                p++;  // skip closing '"'
              }
            }
          }
        }

        if (bvdata == nullptr) {
          // No valid boundary parameter found — cannot parse multipart body.
          async_ws_log_d("Missing multipart boundary parameter, aborting");
          _parseState = PARSE_REQ_FAIL;
          abort();
          return true;
        }

        // Strip leading OWS after 'boundary=' — non-RFC but seen in the wild.
        bvdata = skip_ows(bvdata, end);

        // Clear any previous value — duplicate Content-Type headers must not
        // concatenate into the boundary string.
        _boundary = String();

        if (bvdata < end && *bvdata == '"') {
          // Quoted-string: unescape quoted-pairs into _boundary directly.
          bvdata++;  // skip opening '"'
          // Reserve at most 70 chars — the RFC 2046 §5.1 maximum — rather than
          // the full (attacker-controlled) remaining suffix length.
          if (!_boundary.reserve(70)) {
            async_ws_log_e("Failed to allocate");
            _parseState = PARSE_REQ_FAIL;
            abort();
            return true;
          }
          bool closed = false;
          while (bvdata < end) {
            char c = *bvdata++;
            if (c == '\\' && bvdata < end) {
              _boundary += *bvdata++;  // quoted-pair: keep only the escaped char
            } else if (c == '"') {
              closed = true;
              break;
            } else {
              _boundary += c;
            }
            // CWE-190 / DoS fix: RFC 2046 §5.1 limits boundary strings to 70 characters.
            if (_boundary.length() > 70) {
              async_ws_log_d("Invalid multipart boundary length (%u), aborting", _boundary.length());
              _parseState = PARSE_REQ_FAIL;
              abort();
              return true;
            }
          }
          if (!closed) {
            // Opening quote was never closed — malformed header.
            async_ws_log_d("Invalid multipart boundary (unterminated quote), aborting");
            _parseState = PARSE_REQ_FAIL;
            abort();
            return true;
          }
        } else {
          // Token form: value ends at the next ';' or end of string.
          const char *tok_end = bvdata;
          while (tok_end < end && *tok_end != ';') {
            tok_end++;
          }
          tok_end = rtrim_ows(bvdata, tok_end);
          size_t bvlen = (size_t)(tok_end - bvdata);
          // CWE-190 / DoS fix: RFC 2046 §5.1 limits boundary strings to 70 characters.
          if (bvlen == 0 || bvlen > 70) {
            async_ws_log_d("Invalid multipart boundary length (%u), aborting", (unsigned)bvlen);
            _parseState = PARSE_REQ_FAIL;
            abort();
            return true;
          }
#ifdef ESP8266
          {
            // ESP8266 Arduino String lacks String(const char*, size_t).
            // bvlen <= 70 is guaranteed above, so a stack buffer is safe.
            char buf[71];
            memcpy(buf, bvdata, bvlen);
            buf[bvlen] = '\0';
            _boundary = String(buf);
          }
#else
          _boundary = String(bvdata, (unsigned int)bvlen);
#endif
        }

        // CWE-190 / DoS fix: final safety net.  The per-branch checks above
        // cover the token path (bvlen == 0 || bvlen > 70) and the quoted-string
        // path (> 70 inside the loop), but an empty quoted-string (boundary="")
        // closes immediately with _boundary still empty and is not rejected
        // by either branch.  Reject it here before _isMultipart is set, so an
        // empty boundary never reaches the multipart body parser (where it
        // would cause --\r\n to be accepted as a delimiter).
        if (_boundary.length() == 0 || _boundary.length() > 70) {
          async_ws_log_d("Invalid multipart boundary length (%u), aborting", _boundary.length());
          _parseState = PARSE_REQ_FAIL;
          abort();
          return true;
        }

        _isMultipart = true;
      }
    } else if (name.equalsIgnoreCase(T_Content_Length) || name.equalsIgnoreCase(T_X_Expected_Entity_Length)) {
      // MacOS WebDAVFS uses X-Expected-Entity-Length to indicate the
      // total length of a chunked request body.  It is useful to
      // determine if a PUT can possibly fit in the available space.
      _contentLength = atoi(value.c_str());
    } else if (name.equalsIgnoreCase(T_EXPECT) && value.equalsIgnoreCase(T_100_CONTINUE)) {
      _expectingContinue = true;
    } else if (name.equalsIgnoreCase(T_AUTH)) {
      int space = value.indexOf(' ');
      if (space == -1) {
        _authorization = value;
        _authMethod = AsyncAuthType::AUTH_OTHER;
      } else {
        String method = value.substring(0, space);
        if (method.equalsIgnoreCase(T_BASIC)) {
          _authMethod = AsyncAuthType::AUTH_BASIC;
        } else if (method.equalsIgnoreCase(T_DIGEST)) {
          _authMethod = AsyncAuthType::AUTH_DIGEST;
        } else if (method.equalsIgnoreCase(T_BEARER)) {
          _authMethod = AsyncAuthType::AUTH_BEARER;
        } else {
          _authMethod = AsyncAuthType::AUTH_OTHER;
        }
        _authorization = value.substring(space + 1);
      }
    } else if (name.equalsIgnoreCase(T_UPGRADE) && value.equalsIgnoreCase(T_WS)) {
      // WebSocket request can be uniquely identified by header: [Upgrade: websocket]
      // Per RFC 6455 §4.1 the handshake is a GET.  Only classify when the
      // connection is still a plain HTTP connection so a previously detected
      // SSE request (or any other classified type) cannot be clobbered by
      // header ordering.
      if (_method == AsyncWebRequestMethod::HTTP_GET && _reqconntype == RCT_HTTP) {
        _reqconntype = RCT_WS;
      }
    } else if (name.equalsIgnoreCase(T_ACCEPT)) {
      String lowcase(value);
      lowcase.toLowerCase();
#ifndef ESP8266
      const char *substr = std::strstr(lowcase.c_str(), T_text_event_stream);
#else
      const char *substr = std::strstr(lowcase.c_str(), String(T_text_event_stream).c_str());
#endif
      // Server-Sent Events (HTML §9.2) are GET-only connections negotiated via
      // Accept: text/event-stream.  Only classify when the connection is still
      // a plain HTTP connection so a previously detected WebSocket upgrade
      // cannot be clobbered by header ordering.
      if (substr != NULL && _method == AsyncWebRequestMethod::HTTP_GET && _reqconntype == RCT_HTTP) {
        // WebEvent request can be uniquely identified by header:  [Accept: text/event-stream]
        _reqconntype = RCT_EVENT;
      }
    } else if (name.equalsIgnoreCase(T_Transfer_Encoding)) {
      String lowcase(value);
      lowcase.toLowerCase();
      String key;

      while (lowcase.length()) {
        auto pos = lowcase.indexOf(',');
        if (pos >= 0) {
          key = lowcase.substring(0, pos);
          lowcase = lowcase.substring(pos + 1);
        } else {
          key = lowcase;
          lowcase = "";
        }
        key.trim();
        if (key == "chunked") {
          _chunkSize = 0;
          _chunkStartIndex = 0;
          _chunkedParseState = CHUNK_LENGTH;
          break;
        }
      }
    }
    _headers.emplace_back(std::move(header));
  }
#if defined(TARGET_RP2040) || defined(TARGET_RP2350) || defined(PICO_RP2040) || defined(PICO_RP2350) || defined(LIBRETINY) || defined(HOST)
  // ArduinoCore-API does not have String::clear() method 8-()
  _temp = asyncsrv::emptyString;
#else
  _temp.clear();
#endif
  return true;
}
size_t AsyncWebServerRequest::headers() const {
  return _headers.size();
}
const AsyncWebHeader *AsyncWebServerRequest::getHeader(const char *name) const {
  auto iter = std::find_if(std::begin(_headers), std::end(_headers), [&name](const AsyncWebHeader &header) {
    return header.name().equalsIgnoreCase(name);
  });
  return (iter == std::end(_headers)) ? nullptr : &(*iter);
}
const AsyncWebHeader *AsyncWebServerRequest::getHeader(size_t num) const {
  if (num >= _headers.size()) {
    return nullptr;
  }
  return &(*std::next(_headers.cbegin(), num));
}
const String &AsyncWebServerRequest::header(const char *name) const {
  const AsyncWebHeader *h = getHeader(name);
  return h ? h->value() : asyncsrv::emptyString;
}
const String &AsyncWebServerRequest::header(size_t i) const {
  const AsyncWebHeader *h = getHeader(i);
  return h ? h->value() : asyncsrv::emptyString;
}
const String &AsyncWebServerRequest::headerName(size_t i) const {
  const AsyncWebHeader *h = getHeader(i);
  return h ? h->name() : asyncsrv::emptyString;
}
namespace asyncsrv {
WebRequestMethod stringToMethod(const String &m) {
  if (m == T_GET) {
    return AsyncWebRequestMethod::HTTP_GET;
  } else if (m == T_POST) {
    return AsyncWebRequestMethod::HTTP_POST;
  } else if (m == T_DELETE) {
    return AsyncWebRequestMethod::HTTP_DELETE;
  } else if (m == T_PUT) {
    return AsyncWebRequestMethod::HTTP_PUT;
  } else if (m == T_PATCH) {
    return AsyncWebRequestMethod::HTTP_PATCH;
  } else if (m == T_HEAD) {
    return AsyncWebRequestMethod::HTTP_HEAD;
  } else if (m == T_OPTIONS) {
    return AsyncWebRequestMethod::HTTP_OPTIONS;
  } else if (m == T_TRACE) {
    return AsyncWebRequestMethod::HTTP_TRACE;
  } else if (m == T_CONNECT) {
    return AsyncWebRequestMethod::HTTP_CONNECT;
  } else if (m == T_PURGE) {
    return AsyncWebRequestMethod::HTTP_PURGE;
  } else if (m == T_LINK) {
    return AsyncWebRequestMethod::HTTP_LINK;
  } else if (m == T_UNLINK) {
    return AsyncWebRequestMethod::HTTP_UNLINK;
  } else if (m == T_PROPFIND) {
    return AsyncWebRequestMethod::HTTP_PROPFIND;
  } else if (m == T_LOCK) {
    return AsyncWebRequestMethod::HTTP_LOCK;
  } else if (m == T_UNLOCK) {
    return AsyncWebRequestMethod::HTTP_UNLOCK;
  } else if (m == T_PROPPATCH) {
    return AsyncWebRequestMethod::HTTP_PROPPATCH;
  } else if (m == T_MKCOL) {
    return AsyncWebRequestMethod::HTTP_MKCOL;
  } else if (m == T_MOVE) {
    return AsyncWebRequestMethod::HTTP_MOVE;
  } else if (m == T_COPY) {
    return AsyncWebRequestMethod::HTTP_COPY;
  } else if (m == T_SEARCH) {
    return AsyncWebRequestMethod::HTTP_SEARCH;
  } else if (m == T_BIND) {
    return AsyncWebRequestMethod::HTTP_BIND;
  } else if (m == T_REBIND) {
    return AsyncWebRequestMethod::HTTP_REBIND;
  } else if (m == T_UNBIND) {
    return AsyncWebRequestMethod::HTTP_UNBIND;
  } else if (m == T_ACL) {
    return AsyncWebRequestMethod::HTTP_ACL;
  } else if (m == T_QUERY) {
    return AsyncWebRequestMethod::HTTP_QUERY;
  } else {
    return AsyncWebRequestMethod::HTTP_INVALID;
  }
}
}

void AsyncWebServerRequest::_parseLine() {
  if (_parseState == PARSE_REQ_START) {
    if (!_temp.length()) {
      _parseState = PARSE_REQ_FAIL;
      abort();
    } else {
      if (_parseReqHead()) {
        _parseState = PARSE_REQ_HEADERS;
      } else {
        _parseState = PARSE_REQ_FAIL;
        abort();
      }
    }
    return;
  }

  if (_parseState == PARSE_REQ_HEADERS) {
    if (!_temp.length()) {
      // end of headers
      _server->_rewriteRequest(this);
      _server->_attachHandler(this);
      if (!_server->_runAttachPolicy206(this)) {
        _parseState = PARSE_REQ_FAIL;
        abort();
        return;
      }
      if (_expectingContinue) {
        String response(T_HTTP_100_CONT);
        _client->write(response.c_str(), response.length());
      }
      if (_contentLength || _chunkedParseState != CHUNK_NONE) {
        _parseState = PARSE_REQ_BODY;
      } else {
        _parseState = PARSE_REQ_END;
        _runMiddlewareChain();
        _send();
      }
    } else {
      _parseReqHeader();
    }
  }
}
bool AsyncWebServer::setAttachPolicy206(AttachPolicy206 policy, void *context) {
  if (!policy || _attachPolicy206) {
    return false;
  }
  _attachPolicy206 = policy;
  _attachPolicyContext206 = context;
  return true;
}
void AsyncWebServer::clearAttachPolicy206() {
  _attachPolicy206 = nullptr;
  _attachPolicyContext206 = nullptr;
}
bool AsyncWebServer::_runAttachPolicy206(AsyncWebServerRequest *request) {
  AttachPolicy206 policy = _attachPolicy206;
  return !policy || policy(request, _attachPolicyContext206);
}