// Exact pinned request factory. LGPL-3.0-or-later.
  static std::shared_ptr<AsyncWebServerRequest> create(AsyncWebServer *server, AsyncClient *client) {
    AsyncWebServerRequest *req = new (std::nothrow) AsyncWebServerRequest(server, client);
    if (req) {
      req->_this = std::shared_ptr<AsyncWebServerRequest>(req);  // store shared pointer to this request
      return req->_this;
    }
    return {};  // empty shared_ptr
  }
