// Modeled surrounding request state; fields needed by extracted body methods.
 ModelHandler*_handler=nullptr;
 std::list<AsyncWebParameter> _params;
 String _temp,_contentType,_boundary,_itemName,_itemFilename,_itemType,_itemValue;
 uint8_t _parseState=PARSE_REQ_BODY,_multiParseState=0,_chunkedParseState=CHUNK_NONE;
 bool _isMultipart=true,_isPlainPost=false,_itemIsFile=false;
 size_t _contentLength=0,_parsedLength=0,_boundaryPosition=0,_itemStartIndex=0,_itemSize=0,_itemBufferIndex=0;
 void _onData(void*,size_t);void _parseMultipartPostByte(uint8_t,bool);void _handleUploadByte(uint8_t,bool);
 void _parseLine(){assert(false);}bool _parseChunkedBytes(uint8_t*,size_t){assert(false);return false;}
 void _parsePlainPostChar(uint8_t){assert(false);}
 // Routing/middleware remain modeled; this is NOT the real header/route pipeline.
 void _runMiddlewareChain(){assert(_handler);_handler->handleRequest(this);}
