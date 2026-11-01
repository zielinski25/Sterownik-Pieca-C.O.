// LGPL-3.0-or-later, candidate additions.
#define HISTORY203_DEFERRED_POLL 1

  // Only from the request/network context. Does not replace AsyncClient callbacks.
  bool installDeferred203(void (*poll)(AsyncWebServerRequest*, void*), void (*gone)(void*), void* context) {
    if (!poll || !gone || _sent || _disconnected203 || _pollBusy203 || _deferredDisconnect203) return false;
    _deferredContext203 = context; _deferredDisconnect203 = gone; _deferredPoll203 = poll; return true;
  }
  void stopDeferredPoll203() { _deferredPoll203 = nullptr; }
  bool disconnected203() const { return _disconnected203; }
