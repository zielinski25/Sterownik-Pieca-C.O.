// LGPL-3.0-or-later: exact pinned methods, candidate poll/disconnect.
void AsyncWebServerRequest::_onPoll() {
  std::shared_ptr<AsyncWebServerRequest> self = _this;
  if (!self || _disconnected203 || _pollBusy203) return;
  if (_deferredPoll203) {
    auto poll = _deferredPoll203; auto context = _deferredContext203;
    _pollBusy203 = true; poll(this, context); _pollBusy203 = false;
    if (_disconnected203 || !_client || !_this) return;
  }
  if (_response && _client && _client->canSend()) {
    _response->_ack(this, 0, 0);
  }
}

void AsyncWebServerRequest::_onDisconnect() {
  if (_disconnected203) return;
  std::shared_ptr<AsyncWebServerRequest> self = _this;
  _disconnected203 = true;
  auto gone = _deferredDisconnect203; auto context = _deferredContext203;
  _deferredPoll203 = nullptr; _deferredDisconnect203 = nullptr; _deferredContext203 = nullptr;
  if (gone) gone(context);
  if (_onDisconnectfn) _onDisconnectfn();
  _this.reset();
}

AsyncWebServerRequest::~AsyncWebServerRequest() {
  if (_client) {
    // usually it is _client's disconnect triggers object destruct, but for completeness we define behavior
    // if for some reason *this will be destructed while client is still connected
    _client->onDisconnect(nullptr);
    delete _client;
    _client = nullptr;
  }

  if (_response) {
    delete _response;
    _response = nullptr;
  }

  if (_tempObject != NULL) {
    free(_tempObject);
  }

  if (_tempFile) {
    _tempFile.close();
  }

  if (_itemBuffer) {
    free(_itemBuffer);
  }
}

AsyncWebServerRequestPtr AsyncWebServerRequest::pause() {
  if (_paused) {
    return _this;
  }
  client()->setRxTimeout(0);
  _paused = true;
  return _this;
}

void AsyncWebServerRequest::abort() {
  if (!_sent) {
    _sent = true;
    _paused = false;
    async_ws_log_v("Abort request: %s", _url.c_str());
    _client->abort();
  }
}

void AsyncWebServerRequest::send(AsyncWebServerResponse *response) {
  // request is already sent on the wire ?
  if (_sent) {
    return;
  }

  // if we already had a response, delete it and replace it with the new one
  if (_response) {
    delete _response;
  }
  _response = response;

  // if request was paused, we need to send the response now
  if (_paused) {
    _paused = false;
    _send();
  }
}

void AsyncWebServerRequest::_send() {
  if (!_sent && !_paused) {
    // async_ws_log_d("AsyncWebServerRequest::_send()");

    // user did not create a response ?
    if (!_response) {
      send(501, T_text_plain, "Handler did not handle the request");
    }

    // response is not valid ?
    if (!_response->_sourceValid()) {
      send(500, T_text_plain, "Invalid data in handler");
    }

    // here, we either have a response given from user or one of the two above
    _client->setRxTimeout(0);
    _response->_respond(this);
    _sent = true;
  }
}

void AsyncWebServerRequest::onDisconnect(ArDisconnectHandler fn) {
  _onDisconnectfn = fn;
}
