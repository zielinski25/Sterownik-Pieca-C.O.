#!/usr/bin/env python3
"""Test exact ESP HTTPClient 3.3.8 body-reader + patched receiver, with fake network.
Usage: python3 run_host_test.py /path/main_centrala.cpp /path/HTTPClient.cpp /path/ArduinoJson/src
HTTPClient.cpp can be obtained from the Arduino-ESP32 3.3.8 repository.
"""
import pathlib, subprocess, sys, tempfile
if len(sys.argv) != 4:
    raise SystemExit(__doc__)
firmware, client = (pathlib.Path(p).read_text() for p in sys.argv[1:3])
def func(src, signature):
    i=src.index(signature); start=src.index('{',i); depth=0
    for pos in range(start,len(src)):
        if src[pos]=='{': depth+=1
        elif src[pos]=='}':
            depth-=1
            if not depth: return src[i:pos+1]
    raise ValueError(signature)
receiver = firmware[firmware.index('class PrognozaOdbiornik :'):firmware.index('static constexpr size_t PROGNOZA_MAX_BAJTOW')]
body = func(client,'int HTTPClient::writeToStream(Stream *stream)')
block = func(client,'int HTTPClient::writeToStreamDataBlock(Stream *stream, int size)')
for fragment in ['http.writeToStream(&payload)', 'deserializeJson(wejscie, payload.data(), payload.size())', 'payload.przepelniony()', 'http.setAcceptEncoding("identity")', 'dlugosc > (int)PROGNOZA_MAX_BAJTOW']:
    assert fragment in firmware, fragment
prefix = r'''
#include <ArduinoJson.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <iostream>
using std::size_t;
#define MALLOC_CAP_SPIRAM 1
void* heap_caps_malloc(size_t n, int) { return malloc(n); }
void heap_caps_free(void* p) { free(p); }
class Stream {
 public:
 virtual ~Stream() {};
 virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t*,size_t)=0;
 virtual int available()=0; virtual int read()=0; virtual int peek()=0; virtual void flush()=0;
 int getWriteError(){return 0;} void clearWriteError(){}
};
class String {
 std::string s;
 public:
 String(std::string v=""):s(v){}
 size_t length() const {return s.size();}
 const char* c_str() const {return s.c_str();}
 void trim() {while(!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
   while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(0,1);}
};
class FakeClient {
 public:
 std::string s; size_t pos=0;
 explicit FakeClient(std::string input):s(input){}
 bool connected() const { return pos<s.size(); }
 size_t available() const { return s.size()-pos; }
 String readStringUntil(char c) {
   size_t end=s.find(c,pos); if (end==std::string::npos) end=s.size();
   std::string r=s.substr(pos,end-pos); pos=(end<s.size()?end+1:end); return String(r);
 }
 int readBytes(uint8_t* dst,int n) {size_t k=std::min((size_t)n,s.size()-pos); memcpy(dst,s.data()+pos,k); pos+=k; return (int)k;}
};
#define HTTPC_ERROR_NO_STREAM -1
#define HTTPC_ERROR_NOT_CONNECTED -2
#define HTTPC_ERROR_CONNECTION_LOST -3
#define HTTPC_ERROR_READ_TIMEOUT -4
#define HTTPC_ERROR_STREAM_WRITE -5
#define HTTPC_ERROR_ENCODING -6
#define HTTPC_ERROR_TOO_LESS_RAM -7
#define HTTPC_TE_IDENTITY 0
#define HTTPC_TE_CHUNKED 1
#define HTTP_TCP_RX_BUFFER_SIZE 1460
#define log_v(...) ((void)0)
#define log_d(...) ((void)0)
#define log_w(...) ((void)0)
void delay(int) {}
class HTTPClient {
 public:
 FakeClient* _client; int _size; int _transferEncoding;
 HTTPClient(FakeClient* c,int n,int t):_client(c),_size(n),_transferEncoding(t){}
 bool connected(){return _client->connected();}
 int returnError(int n){return n;}
 void disconnect(bool){}
 int writeToStream(Stream*); int writeToStreamDataBlock(Stream*,int);
};
'''
suffix = r'''
std::string chunk(const std::string& s, size_t part) {
 std::string out; char hdr[40];
 for(size_t i=0;i<s.size();i+=part) {size_t n=std::min(part,s.size()-i); snprintf(hdr,sizeof(hdr),"%zx\r\n",n);out+=hdr;out+=s.substr(i,n);out+="\r\n";}
 return out+"0\r\n\r\n";
}
int main() {
 const std::string json="{\"current\":{\"weather_code\":61,\"is_day\":0},\"hourly\":{\"temperature_2m\":[1,2,3,4]}}";
 for (size_t step : {1u, 10u, 32u, 200u}) {
  auto wire=chunk(json,step);
  FakeClient socket(wire); HTTPClient h(&socket,-1,HTTPC_TE_CHUNKED); PrognozaOdbiornik p(65536);
  int n=h.writeToStream(&p); assert(n==(int)json.size()); assert(!p.przepelniony());
  JsonDocument doc; assert(!deserializeJson(doc,p.data(),p.size())); assert(doc["current"]["weather_code"].as<int>()==61); assert(doc["current"]["is_day"].as<int>()==0);
 }
 {FakeClient socket(json); HTTPClient h(&socket,json.size(),HTTPC_TE_IDENTITY); PrognozaOdbiornik p(65536);
  assert(h.writeToStream(&p)==(int)json.size()); JsonDocument doc; assert(!deserializeJson(doc,p.data(),p.size()));}
 {auto wire=chunk(json,10); JsonDocument old; assert(deserializeJson(old,wire.c_str()));}
 {FakeClient socket(chunk(json,16)); HTTPClient h(&socket,-1,HTTPC_TE_CHUNKED); PrognozaOdbiornik p(20);
  assert(h.writeToStream(&p)<0); assert(p.przepelniony());}
 {std::string truncated="20\r\n{\"a\":1}"; FakeClient socket(truncated); HTTPClient h(&socket,-1,HTTPC_TE_CHUNKED); PrognozaOdbiornik p(65536); assert(h.writeToStream(&p)<0);}
 {FakeClient socket("{\"bad\":"); HTTPClient h(&socket,7,HTTPC_TE_IDENTITY); PrognozaOdbiornik p(64);
  assert(h.writeToStream(&p)==7); JsonDocument d; assert(deserializeJson(d,p.data(),p.size()));}
 {PrognozaOdbiornik p(3); assert(p.write((const uint8_t*)"ABC",3)==3); assert(p.write((uint8_t)'X')==0 && p.przepelniony());}
 std::cout<<"OK: original HTTPClient 3.3.8 decoding + JSON, chunked, plain, overflow, truncated, invalid\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=pathlib.Path(tmp)/'test.cpp'; cpp.write_text(prefix+'\n'+receiver+'\n'+body+'\n'+block+'\n'+suffix)
    exe=pathlib.Path(tmp)/'test'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-I',sys.argv[3],str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
