// LGPL-3.0-or-later, exact pinned parser bodies.
void AsyncWebServerRequest::_onData(void *buf, size_t len) {
  std::shared_ptr<AsyncWebServerRequest> self = _this;  // Ensure we stay in scope over the function

  // SSL/TLS handshake detection
#ifndef ASYNC_TCP_SSL_ENABLED
  if (_parseState == PARSE_REQ_START && len && ((uint8_t *)buf)[0] == 0x16) {  // 0x16 indicates a Handshake message (SSL/TLS).
    async_ws_log_d("SSL/TLS handshake detected: resetting connection");
    _parseState = PARSE_REQ_FAIL;
    abort();
    return;
  }
#endif

  size_t i = 0;
  while (true) {

    if (_parseState < PARSE_REQ_BODY) {
      // Find new line in buf
      char *str = (char *)buf;
      for (i = 0; i < len; i++) {
        // Check for null characters in header
        if (!str[i]) {
          _parseState = PARSE_REQ_FAIL;
          abort();
          return;
        }
        if (str[i] == '\n') {
          break;
        }
      }
      if (i == len) {  // No new line, just add the buffer in _temp
        char ch = str[len - 1];
        str[len - 1] = 0;
        if (!_temp.reserve(_temp.length() + len)) {
          async_ws_log_e("Failed to allocate");
          _parseState = PARSE_REQ_FAIL;
          abort();
          return;
        }
        _temp.concat(str);
        _temp.concat(ch);
      } else {       // Found new line - extract it and parse
        str[i] = 0;  // Terminate the string at the end of the line.
        _temp.concat(str);
        _temp.trim();
        _parseLine();
        if (++i < len) {
          // Still have more buffer to process
          buf = str + i;
          len -= i;
          continue;
        }
      }
    } else if (_parseState == PARSE_REQ_BODY) {
      if (_chunkedParseState != CHUNK_NONE) {
        if (_parseChunkedBytes((uint8_t *)buf, len)) {
          _parseState = PARSE_REQ_END;
          _runMiddlewareChain();
          _send();
        }
        break;
      }
      // A handler should be already attached at this point in _parseLine function.
      // If handler does nothing (_onRequest is NULL), we don't need to really parse the body.
      const bool needParse = _handler && !_handler->isRequestHandlerTrivial();
      // Discard any bytes after content length; handlers may overrun their buffers
      len = std::min(len, _contentLength - _parsedLength);
      if (_isMultipart) {
        if (needParse) {
          size_t i;
          for (i = 0; i < len; i++) {
            _parseMultipartPostByte(((uint8_t *)buf)[i], i == len - 1);
            _parsedLength++;
          }
        } else {
          _parsedLength += len;
        }
      } else {
        if (_parsedLength == 0) {
          if (_contentType.startsWith(T_app_xform_urlencoded)) {
            _isPlainPost = true;
          } else if (_contentType == T_text_plain && isParamChar(((char *)buf)[0])) {
            size_t i = 0;
            char ch;
            do {
              ch = ((char *)buf)[i];
            } while (i++ < len && isParamChar(ch));
            if (i < len && ((char *)buf)[i - 1] == '=') {
              _isPlainPost = true;
            }
          }
        }
        if (!_isPlainPost) {
          // ESP_LOGD("AsyncWebServer", "_isPlainPost: %d, _handler: %p", _isPlainPost, _handler);
          if (_handler) {
            _handler->handleBody(this, (uint8_t *)buf, len, _parsedLength, _contentLength);
          }
          _parsedLength += len;
        } else if (needParse) {
          size_t i;
          for (i = 0; i < len; i++) {
            _parsedLength++;
            _parsePlainPostChar(((uint8_t *)buf)[i]);
          }
        } else {
          _parsedLength += len;
        }
      }
      if (_parsedLength == _contentLength) {
        _parseState = PARSE_REQ_END;
        _runMiddlewareChain();
        _send();
      }
    }
    break;
  }
}
void AsyncWebServerRequest::_handleUploadByte(uint8_t data, bool last) {
  // CWE-476 defense-in-depth: never write through a NULL buffer pointer.
  // The primary fix resets _itemIsFile when the buffer is freed, but this guard
  // protects against any future code path that might reach here without one.
  if (_itemBuffer == NULL) {
    return;
  }

  _itemBuffer[_itemBufferIndex++] = data;

  if (last || _itemBufferIndex == RESPONSE_STREAM_BUFFER_SIZE) {
    // check if authenticated before calling the upload
    if (_handler) {
      _handler->handleUpload(this, _itemFilename, _itemSize - _itemBufferIndex, _itemBuffer, _itemBufferIndex, false);
    }
    _itemBufferIndex = 0;
  }
}
enum {
  EXPECT_BOUNDARY,
  PARSE_HEADERS,
  WAIT_FOR_RETURN1,
  EXPECT_FEED1,
  EXPECT_DASH1,
  EXPECT_DASH2,
  BOUNDARY_OR_DATA,
  DASH3_OR_RETURN2,
  EXPECT_FEED2,
  PARSING_FINISHED,
  PARSE_ERROR
};

void AsyncWebServerRequest::_parseMultipartPostByte(uint8_t data, bool last) {
#define itemWriteByte(b)          \
  do {                            \
    _itemSize++;                  \
    if (_itemIsFile)              \
      _handleUploadByte(b, last); \
    else                          \
      _itemValue += (char)(b);    \
  } while (0)

  if (!_parsedLength) {
    _multiParseState = EXPECT_BOUNDARY;
    _temp = asyncsrv::emptyString;
    _itemName = asyncsrv::emptyString;
    _itemFilename = asyncsrv::emptyString;
    _itemType = asyncsrv::emptyString;
  }

  if (_multiParseState == WAIT_FOR_RETURN1) {
    if (data != '\r') {
      itemWriteByte(data);
    } else {
      _multiParseState = EXPECT_FEED1;
    }
  } else if (_multiParseState == EXPECT_BOUNDARY) {
    // Note: when _parsedLength < 2, the subtractions below (_parsedLength - 2
    // and _parsedLength - 3) wrap around to very large size_t values.  This is
    // intentional: the wrapped values are always greater than _boundary.length()
    // (which is at most 70), so those comparisons evaluate to false and the
    // first two bytes (the '--' prefix) are consumed without error.
    if (_parsedLength < 2 && data != '-') {
      _multiParseState = PARSE_ERROR;
      return;
    } else if (_parsedLength - 2 < _boundary.length() && _boundary.c_str()[_parsedLength - 2] != data) {
      _multiParseState = PARSE_ERROR;
      return;
    } else if (_parsedLength - 2 == _boundary.length() && data != '\r') {
      _multiParseState = PARSE_ERROR;
      return;
    } else if (_parsedLength - 3 == _boundary.length()) {
      if (data != '\n') {
        _multiParseState = PARSE_ERROR;
        return;
      }
      _multiParseState = PARSE_HEADERS;
      _itemIsFile = false;
    }
  } else if (_multiParseState == PARSE_HEADERS) {
    if ((char)data != '\r' && (char)data != '\n') {
      _temp += (char)data;
    }
    if ((char)data == '\n') {
      if (_temp.length()) {
        if (_temp.length() > 12 && _temp.substring(0, 12).equalsIgnoreCase(T_Content_Type)) {
          _itemType = _temp.substring(14);
          _itemIsFile = true;
        } else if (_temp.length() > 19 && _temp.substring(0, 19).equalsIgnoreCase(T_Content_Disposition)) {
          _temp = _temp.substring(_temp.indexOf(';') + 2);
          while (_temp.indexOf(';') > 0) {
            String name = _temp.substring(0, _temp.indexOf('='));
            String nameVal = _temp.substring(_temp.indexOf('=') + 2, _temp.indexOf(';') - 1);
            if (name == T_name) {
              _itemName = nameVal;
            } else if (name == T_filename) {
              _itemFilename = nameVal;
              _itemIsFile = true;
            }
            _temp = _temp.substring(_temp.indexOf(';') + 2);
          }
          String name = _temp.substring(0, _temp.indexOf('='));
          String nameVal = _temp.substring(_temp.indexOf('=') + 2, _temp.length() - 1);
          if (name == T_name) {
            _itemName = nameVal;
          } else if (name == T_filename) {
            _itemFilename = nameVal;
            _itemIsFile = true;
          }
          // Add the parameters from the content-disposition header to the param list, flagged as POST and File,
          // so that they can be retrieved using getParam(name, isPost=true, isFile=true)
          // in the upload handler to correctly handle multiple file uploads within the same request.
          // Example: Content-Disposition: form-data; name="fw"; filename="firmware.bin"
          // See: https://github.com/ESP32Async/ESPAsyncWebServer/discussions/328
          if (_itemIsFile && _itemName.length() && _itemFilename.length()) {
            // add new parameters for this content-disposition
            _params.emplace_back(T_name, _itemName, true, true);
            _params.emplace_back(T_filename, _itemFilename, true, true);
          }
        }
        _temp = asyncsrv::emptyString;
      } else {
        _multiParseState = WAIT_FOR_RETURN1;
        // value starts from here
        _itemSize = 0;
        _itemStartIndex = _parsedLength;
        _itemValue = asyncsrv::emptyString;
        if (_itemIsFile) {
          if (_itemBuffer) {
            free(_itemBuffer);
          }
          _itemBuffer = (uint8_t *)malloc(RESPONSE_STREAM_BUFFER_SIZE);
          if (_itemBuffer == NULL) {
            async_ws_log_e("Failed to allocate");
            _multiParseState = PARSE_ERROR;
            abort();
            return;
          }
          _itemBufferIndex = 0;
        }
      }
    }
  } else if (_multiParseState == EXPECT_FEED1) {
    if (data != '\n') {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      _parseMultipartPostByte(data, last);
    } else {
      _multiParseState = EXPECT_DASH1;
    }
  } else if (_multiParseState == EXPECT_DASH1) {
    if (data != '-') {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      _parseMultipartPostByte(data, last);
    } else {
      _multiParseState = EXPECT_DASH2;
    }
  } else if (_multiParseState == EXPECT_DASH2) {
    if (data != '-') {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      _parseMultipartPostByte(data, last);
    } else {
      _multiParseState = BOUNDARY_OR_DATA;
      _boundaryPosition = 0;
    }
  } else if (_multiParseState == BOUNDARY_OR_DATA) {
    if (_boundaryPosition < _boundary.length() && _boundary.c_str()[_boundaryPosition] != data) {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      itemWriteByte('-');
      // CWE-190 fix: loop variable was uint8_t, matching the old type of
      // _boundaryPosition.  Changed to size_t for consistency.
      for (size_t i = 0; i < _boundaryPosition; i++) {
        itemWriteByte(_boundary.c_str()[i]);
      }
      _parseMultipartPostByte(data, last);
    } else if (_boundaryPosition == _boundary.length() - 1) {
      _multiParseState = DASH3_OR_RETURN2;
      if (!_itemIsFile) {
        _params.emplace_back(_itemName, _itemValue, true);
      } else {
        if (_handler) {
          _handler->handleUpload(this, _itemFilename, _itemSize - _itemBufferIndex, _itemBuffer, _itemBufferIndex, true);
        }
        _itemBufferIndex = 0;
        _params.emplace_back(_itemName, _itemFilename, true, true, _itemSize);
        // remove previous occurrence(s) of content-disposition parameters for this upload
        _params.remove_if([this](const AsyncWebParameter &p) {
          return p.isPost() && p.isFile() && (p.name() == T_name || p.name() == T_filename);
        });
        free(_itemBuffer);
        _itemBuffer = NULL;
        // CWE-476 fix: _itemIsFile must be cleared when the buffer is freed.
        // Otherwise, if the next byte does not confirm a clean boundary close
        // (e.g. '--<boundary>' appeared in the file data by coincidence), the
        // rewind logic in DASH3_OR_RETURN2 / EXPECT_FEED2 calls itemWriteByte()
        // which dereferences the now-NULL _itemBuffer and crashes (DoS).
        _itemIsFile = false;
      }

    } else {
      _boundaryPosition++;
    }
  } else if (_multiParseState == DASH3_OR_RETURN2) {
    if (data == '-' && (_contentLength - _parsedLength - 4) != 0) {
      // os_printf("ERROR: The parser got to the end of the POST but is expecting %u bytes more!\nDrop an issue so we can have more info on the matter!\n", _contentLength - _parsedLength - 4);
      _contentLength = _parsedLength + 4;  // lets close the request gracefully
    }
    if (data == '\r') {
      _multiParseState = EXPECT_FEED2;
    } else if (data == '-' && _contentLength == (_parsedLength + 4)) {
      _multiParseState = PARSING_FINISHED;
    } else {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      itemWriteByte('-');
      for (size_t i = 0; i < _boundary.length(); i++) {
        itemWriteByte(_boundary.c_str()[i]);
      }
      _parseMultipartPostByte(data, last);
    }
  } else if (_multiParseState == EXPECT_FEED2) {
    if (data == '\n') {
      _multiParseState = PARSE_HEADERS;
      _itemIsFile = false;
    } else {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      itemWriteByte('-');
      for (size_t i = 0; i < _boundary.length(); i++) {
        itemWriteByte(_boundary.c_str()[i]);
      }
      itemWriteByte('\r');
      _parseMultipartPostByte(data, last);
    }
  }
}
