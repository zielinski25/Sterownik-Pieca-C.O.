// LGPL-3.0-or-later, candidate additions.

  void (*_deferredPoll203)(AsyncWebServerRequest*, void*) = nullptr;
  void (*_deferredDisconnect203)(void*) = nullptr;
  void* _deferredContext203 = nullptr;
  bool _disconnected203 = false;
  bool _pollBusy203 = false;
