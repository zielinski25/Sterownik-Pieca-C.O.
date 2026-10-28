#pragma once
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <string>
typedef bool boolean;
class String {std::string s_;public:String(){}String(const char*p):s_(p?p:""){}String(const std::string&s):s_(s){}size_t length()const{return s_.size();}const char*c_str()const{return s_.c_str();}};
class Stream {protected:unsigned long _timeout=0;public:virtual ~Stream(){}virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;};
