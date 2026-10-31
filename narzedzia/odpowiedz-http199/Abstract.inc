// LGPL-3.0-or-later: pinned declaration with candidate finished guard.
#define HISTORY199_RESPONSE_GUARD 1
class AsyncAbstractResponse : public AsyncWebServerResponse {
private:
#if ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
  // amount of response data in-flight, i.e. sent, but not acked yet
  size_t _in_flight{0};
  // in-flight queue credits
  size_t _in_flight_credit{2};
#endif
  // buffer to accumulate all response headers
  String _assembled_headers;
  // amount of headers buffer writtent to sockbuff
  size_t _writtenHeadersLength{0};
  // Data is inserted into cache at begin().
  // This is inefficient with vector, but if we use some other container,
  // we won't be able to access it as contiguous array of bytes when reading from it,
  // so by gaining performance in one place, we'll lose it in another.
  std::vector<uint8_t> _cache;
  // intermediate buffer to copy outbound data to, also it will keep pending data between _send calls
  std::unique_ptr<std::array<uint8_t, ASYNC_RESPONCE_BUFF_SIZE> > _send_buffer;
  // buffer data size specifiers
  size_t _send_buffer_offset{0}, _send_buffer_len{0};
  size_t _readDataFromCacheOrContent(uint8_t *data, const size_t len);
  size_t _fillBufferAndProcessTemplates(uint8_t *buf, size_t maxLen);

protected:
  AwsTemplateProcessor _callback;
  /**
   * @brief write next portion of response data to send buffs
   * this method (re)fills tcp send buffers, it could be called either at will
   * or from a tcp_recv/tcp_poll callbacks from AsyncTCP
   *
   * @param request - used to access client object
   * @param len - size of acknowledged data from the remote side (TCP window update, not TCP ack!)
   * @param time - time passed between last sent and received packet
   * @return size_t amount of response data placed to TCP send buffs for delivery (defined by sdkconfig value CONFIG_LWIP_TCP_SND_BUF_DEFAULT)
   */
  size_t write_send_buffs(AsyncWebServerRequest *request, size_t len, uint32_t time);

public:
  AsyncAbstractResponse(AwsTemplateProcessor callback = nullptr);
  virtual ~AsyncAbstractResponse() {}
  bool _finished() const override {
    return _state == RESPONSE_FAILED || (_state > RESPONSE_WAIT_ACK && _send_buffer_len == 0);
  }
  void _respond(AsyncWebServerRequest *request) final;
  size_t _ack(AsyncWebServerRequest *request, size_t len, uint32_t time) final {
    return write_send_buffs(request, len, time);
  };
  virtual bool _sourceValid() const {
    return false;
  }
  virtual size_t _fillBuffer(uint8_t *buf __attribute__((unused)), size_t maxLen __attribute__((unused))) {
    return 0;
  }
};
