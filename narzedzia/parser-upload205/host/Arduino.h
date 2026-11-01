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
 String(){}String(const char*p):s_(p?p:""){}String(const std::string&s):s_(s){}
 const char*c_str()const{return s_.c_str();}size_t length()const{return s_.size();}
 bool reserve(size_t n){s_.reserve(n);return true;}void concat(const char*p){s_+=p;}void concat(char c){s_+=c;}
 void clear(){s_.clear();}void trim(){auto a=s_.find_first_not_of(" \t\r\n"),b=s_.find_last_not_of(" \t\r\n");s_=a==std::string::npos?"":s_.substr(a,b-a+1);}
 bool startsWith(const char*p)const{return s_.find(p)==0;}bool operator==(const char*p)const{return s_==p;}bool operator==(const String&o)const{return s_==o.s_;}
 String&operator+=(char c){s_+=c;return *this;}char charAt(size_t i)const{return i<s_.size()?s_[i]:0;}
 int indexOf(char c)const{auto i=s_.find(c);return i==std::string::npos?-1:int(i);}
 String substring(unsigned a,unsigned b)const{if(a>b)std::swap(a,b);if(a>=s_.size())return String();return s_.substr(a,std::min<size_t>(b,s_.size())-a);}
 String substring(unsigned a)const{return substring(a,s_.size());}
 bool equalsIgnoreCase(const char*p)const{std::string t=p;if(t.size()!=s_.size())return false;for(size_t i=0;i<t.size();++i)if(std::tolower((unsigned char)t[i])!=std::tolower((unsigned char)s_[i]))return false;return true;}
};
class Stream {protected:unsigned long _timeout=0;public:virtual ~Stream(){}virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;};
