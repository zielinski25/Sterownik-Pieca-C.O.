// LGPL-3.0-or-later, ESP32Async/ESPAsyncWebServer pinned c47917dcaba2acefb04277cd679f58a60b4fab30.
// Wyciagniete bez zmian przez build.py; kazdy fragment jest bajtowo rowny plikowi zrodlowemu.
// Czlon 2: konstruktory i matches z WebServer.cpp (kandydat 206), AsyncWebServer::_attachHandler
// z WebServer.cpp, AsyncCallbackWebHandler::canHandle i handleRequest z WebHandlers.cpp.
AsyncURIMatcher::AsyncURIMatcher(String uri, uint16_t modifiers) : _value(std::move(uri)) {
#ifdef ASYNCWEBSERVER_REGEX
  if (_value.startsWith("^") && _value.endsWith("$")) {
    pattern = new std::regex(_value.c_str(), (modifiers & CaseInsensitive) ? (std::regex::icase | std::regex::optimize) : (std::regex::optimize));
    return;  // no additional processing - flags are overwritten by pattern pointer
  }
#endif
  if (modifiers & CaseInsensitive) {
    _value.toLowerCase();
  }
  // Inspect _value to set flags
  // empty URI matches everything
  if (!_value.length()) {
    _flags = _toFlags(Type::All, modifiers);
  } else if (_value.endsWith("*")) {
    // wildcard match with * at the end
    _flags = _toFlags(Type::Prefix, modifiers);
    _value = _value.substring(0, _value.length() - 1);
  } else if (_value.lastIndexOf("/*.") >= 0) {
    // prefix match with /*.ext
    // matches any path ending with .ext
    // e.g. /images/*.png will match /images/pic.png and /images/2023/pic.png but not /img/pic.png
    _flags = _toFlags(Type::Extension, modifiers);
  } else {
    // backward compatible use case: exact match or prefix with trailing /
    _flags = _toFlags(Type::BackwardCompatible, modifiers);
  }
}

AsyncURIMatcher::AsyncURIMatcher(String uri, Type type, uint16_t modifiers) : _value(std::move(uri)), _flags(_toFlags(type, modifiers)) {
#ifdef ASYNCWEBSERVER_REGEX
  if (type == Type::Regex) {
    pattern = new std::regex(_value.c_str(), (modifiers & CaseInsensitive) ? (std::regex::icase | std::regex::optimize) : (std::regex::optimize));
    return;  // no additional processing - flags are overwritten by pattern pointer
  }
#endif
  if (modifiers & CaseInsensitive) {
    _value.toLowerCase();
  }
}

bool AsyncURIMatcher::matches(AsyncWebServerRequest *request) const {
#ifdef ASYNCWEBSERVER_REGEX
  if (_isRegex()) {
    // when type == Type::Regex, or when _value was auto-detected as regex
    std::smatch matches;
    std::string s(request->url().c_str());
    if (std::regex_search(s, matches, *pattern)) {
      for (size_t i = 1; i < matches.size(); ++i) {
        request->_pathParams.emplace_back(matches[i].str().c_str());
      }
      return true;
    }
    return false;
  }
#endif

  // extract matcher type from _flags
  Type type;
  uint16_t modifiers;
  std::tie(type, modifiers) = _fromFlags(_flags);

  // apply modifiers
  String path = request->url();
  if (modifiers & CaseInsensitive) {
    path.toLowerCase();
  }

  switch (type) {
    case Type::All:    return true;
    case Type::None:   return false;
    case Type::Exact:  return (_value == path);
    case Type::Prefix: return path.startsWith(_value);
    case Type::Extension:
    {
      int split = _value.lastIndexOf("/*.");
      return (split >= 0 && path.startsWith(_value.substring(0, split)) && path.endsWith(_value.substring(split + 2)));
    }
    case Type::BackwardCompatible: return (_value == path) || path.startsWith(_value + "/");
    default:
      // Should never happen - programming error
      assert("Invalid type");
      return false;
  }
}

void AsyncWebServer::_attachHandler(AsyncWebServerRequest *request) {
  for (auto &h : _handlers) {
    if (h->filter(request) && h->canHandle(request)) {
      request->setHandler(h.get());
      return;
    }
  }
  // ESP_LOGD("AsyncWebServer", "No handler found for %s, using _catchAllHandler pointer: %p", request->url().c_str(), _catchAllHandler);
  request->setHandler(_catchAllHandler);
}

bool AsyncCallbackWebHandler::canHandle(AsyncWebServerRequest *request) const {
  if (!_onRequest || !request->isHTTP() || !_method.matches(request->method())) {
    return false;
  }
  return _uri.matches(request);
}

void AsyncCallbackWebHandler::handleRequest(AsyncWebServerRequest *request) {
  if (_onRequest) {
    _onRequest(request);
  } else {
    request->send(404, T_text_plain, "Not found");
  }
}
