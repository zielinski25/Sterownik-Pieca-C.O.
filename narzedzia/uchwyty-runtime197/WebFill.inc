// Exact pinned method body, model class, real fs::File. LGPL-3.0-or-later.
size_t AsyncFileResponse::_fillBuffer(uint8_t *data, size_t len) {
  return _content.read(data, len);
}
static const uint32_t tryAgain197=0xFFFFFFFF;
