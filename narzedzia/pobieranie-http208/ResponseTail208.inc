// Derived from pinned 199 Transport.inc: _finished, stock _onAck and _onTimeout, unchanged.
bool AsyncWebServerResponse::_finished() const {
  return _state > RESPONSE_WAIT_ACK;
}

void AsyncWebServerRequest::_onAck(size_t len, uint32_t time) {
  // os_printf("a:%u:%u\n", len, time);
  if (!_response) {
    return;
  }

  std::shared_ptr<AsyncWebServerRequest> self = _this;  // Ensure we stay in scope over the function

  if (!_response->_finished()) {
    _response->_ack(this, len, time);
    // recheck if response has just completed, close connection
    if (_response->_finished()) {
      _client->close();  // this will trigger _onDisconnect() and object destruction
    }
  } else {
    // this will close responses that were complete via a single _send() call
    _client->close();  // this will trigger _onDisconnect() and object destruction
  }
}

// _onPoll is supplied by the 203 candidate (deferred hook), not by this file.
void AsyncWebServerRequest::_onTimeout(uint32_t time) {
  (void)time;
  // os_printf("TIMEOUT: %u, state: %s\n", time, _client->stateToString());
  // We do not need to lock the shared pointer here as we do no work after closing the client
  _client->close();
}
