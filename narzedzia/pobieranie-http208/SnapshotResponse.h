#pragma once
// Requires AsyncAbstractResponse from the qualified 199 library patch and the
// fixture request/response model. Prototype only: no ranges, compression or templates.
namespace download208 {
struct Reader {
 virtual ~Reader() {}
 virtual bool valid() = 0;
 virtual std::size_t read(unsigned char *buffer, std::size_t capacity) = 0;
};

// Fixed length announced up front; every byte comes from the pinned snapshot.
class Response final : public AsyncAbstractResponse {
 Reader &reader_;
public:
 Response(Reader &reader, std::size_t expected) : AsyncAbstractResponse(nullptr), reader_(reader) {
  _code = 200;
  _contentType = "application/octet-stream";
  _contentLength = expected;
  _sendContentLength = true;
  _chunked = false;
 }
 bool _sourceValid() const override { return reader_.valid(); }
 size_t _fillBuffer(uint8_t *buffer, size_t maxLen) override {
  return reader_.read(reinterpret_cast<unsigned char *>(buffer), maxLen);
 }
};
}
