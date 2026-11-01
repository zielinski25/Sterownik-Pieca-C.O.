enum {
  PARSE_REQ_START = 0,
  PARSE_REQ_HEADERS = 1,
  PARSE_REQ_BODY = 2,
  PARSE_REQ_END = 3,
  PARSE_REQ_FAIL = 4
};

enum {
  CHUNK_NONE = 0,   // Body transfer encoding is not chunked
  CHUNK_LENGTH,     // Getting chunk length - HHHH[;...] CR LF
  CHUNK_EXTENSION,  // Getting chunk extension - ;... CR LF
  CHUNK_DATA,       // Handling chunk data
  CHUNK_ERROR,      // Invalid chunk header
  CHUNK_END,        // Getting chunk end marker  - CR LF
};

