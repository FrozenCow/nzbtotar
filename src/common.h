#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#define DIE() { fprintf(stderr, "Failed! %s:%d",__FILE__ ,__LINE__); exit(1); }

// optional - either has a value of type T, or has no value at all
template<typename T>
class optional {
  bool _hasValue;
  T _value;
public:
  optional() :
    _hasValue(false) { }
  optional(T value) :
    _hasValue(true),
    _value(value) {}
  inline bool hasValue() {
    return _hasValue;
  }
  inline T value() {
    if (!_hasValue) {
      throw "No value!";
    }
    return _value;
  }
};

struct str {
  const char *ptr;
  size_t len;
  inline str() : ptr(NULL), len(0) {}
  inline str(const char *ptr, size_t len) : ptr(ptr), len(len) {}
  inline bool equals(str &s) {
    if (len != s.len) { return false; }
    for(int i=0;i<s.len;i++) {
      if (ptr[i] != s.ptr[i]) { return false; }
    }
    return true;
  }
  inline bool equals(const char *s) {
    int i;
    for(i=0;s[i] != '\0';i++)
      if (ptr[i] != s[i]) { return false; }
    return len == i;
  }
  inline bool startsWith(str &s) {
    if (len < s.len) { return false; }
    for(int i=0;i<s.len;i++)
      if (ptr[i] != s.ptr[i]) { return false; }
    return true;
  }
  inline bool startsWith(const char *s) {
    for(int i=0;s[i] != '\0';i++)
      if (ptr[i] != s[i]) { return false; }
    return true;
  }
};

void printrarread(const char *buf,size_t size, size_t offset);
void printrarseek(size_t nr, int type, size_t offset);

#endif