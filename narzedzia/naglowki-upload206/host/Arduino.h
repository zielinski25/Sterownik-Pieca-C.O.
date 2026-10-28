#pragma once
// Host-only String/Stream model. Not Arduino allocation, OOM or Unicode qualification.
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <string>
#include <algorithm>
#include <cctype>
typedef bool boolean;
class String {
 std::string s_;
public:
 String(){}String(const char*p):s_(p?p:""){}String(const char*p,unsigned n):s_(p?std::string(p,n):std::string()){}
 String(const std::string&s):s_(s){}
 const char*c_str()const{return s_.c_str();}size_t length()const{return s_.size();}
 bool reserve(size_t n){s_.reserve(n);return true;}
 void concat(const char*p){s_+=p;}void concat(char c){s_+=c;}
 void concat(const char*p,size_t n){if(p)s_.append(p,n);}
 void clear(){s_.clear();}
 void trim(){auto a=s_.find_first_not_of(" \t\r\n"),b=s_.find_last_not_of(" \t\r\n");s_=a==std::string::npos?"":s_.substr(a,b-a+1);}
 void toLowerCase(){for(auto&c:s_)c=(char)std::tolower((unsigned char)c);}
 bool startsWith(const char*p)const{return s_.find(p)==0;}
 bool operator==(const char*p)const{return s_==p;}bool operator==(const String&o)const{return s_==o.s_;}
 bool operator!=(const char*p)const{return !(*this==p);}bool operator!=(const String&o)const{return !(*this==o);}
 String&operator+=(char c){s_+=c;return *this;}
 char charAt(size_t i)const{return i<s_.size()?s_[i]:0;}
 int indexOf(char c)const{auto i=s_.find(c);return i==std::string::npos?-1:int(i);}
 int indexOf(char c,unsigned start)const{auto i=s_.find(c,start);return i==std::string::npos?-1:int(i);}
 String substring(unsigned a,unsigned b)const{if(a>b)std::swap(a,b);if(a>=s_.size())return String();return s_.substr(a,std::min<size_t>(b,s_.size())-a);}
 String substring(unsigned a)const{return substring(a,s_.size());}
 bool equalsIgnoreCase(const char*p)const{return equalsIgnoreCase(String(p));}
 bool equalsIgnoreCase(const String&t)const{if(t.length()!=s_.size())return false;for(size_t i=0;i<t.length();++i)if(std::tolower((unsigned char)t.s_[i])!=std::tolower((unsigned char)s_[i]))return false;return true;}
};
class Stream {protected:unsigned long _timeout=0;public:virtual ~Stream(){}virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;};
