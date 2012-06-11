#ifndef STREAM_H
#define STREAM_H

#include <cstring>
extern "C" {
#include <vrb.h>
}
#include "common.h"

#define DEBUG 1

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

class bufferedstream;
struct section {
	size_t len;
#ifdef DEBUG
	char *ptr;
	bufferedstream *s;
	inline section() : len(0), ptr(NULL), s(NULL) { }
	inline section(bufferedstream *s, char *ptr, size_t len) : len(len), ptr(ptr), s(s) {}
#else
	inline section() : len(0) { }
	inline section(size_t len) : len(len) {}
#endif
};



class bufferedstream {
	int fd;
	vrb_p b;
public:
	inline bufferedstream(int fd, size_t size) :
		fd(fd),
		b(vrb_new(size,NULL)) { }
	inline ~bufferedstream() {
		vrb_destroy(b);
	}
	char& operator[](const size_t i) {
#ifdef DEBUG
		if (i >= len()) {
			DIE();
		}
#endif
		return vrb_data_ptr(b)[i];
	}
	inline size_t read() {
		return vrb_read(b, fd, ~0);
	}
	inline size_t read(size_t max) {
		return vrb_read(b, fd, max);
	}
	inline size_t readMin(size_t min, size_t max) {
		size_t c = 0;
		while (c < min) {
			c += vrb_read(b, fd, max);
		}
		return c;
	}
	inline size_t readMin(size_t min) {
		return readMin(min, ~0);
	}
	inline bool take(size_t n) {
#ifdef DEBUG
		if (n > len()) {
			DIE();
		} 
#endif
		return vrb_take(b, n) == 0;
	}
	inline void takeuntilI(char c) {
		section s = getuntilI(c);
		release(s);
	}
	inline void takeuntilE(char c) {
		section s = getuntilE(c);
		release(s);
	}
	inline void takewhile(char c) {
		while(true) {
			ensure(1);
			if (*ptr() == c) {
				take(1);
			} else {
				break;
			}
		}
	}
	inline void takeline() {
		size_t n = ensureuntil('\n');
		take(n+1);
	}
	inline size_t space() {
		return vrb_space_len(b);
	}
	inline char *ptr() {
		return vrb_data_ptr(b);
	}
	inline size_t len() {
		return vrb_data_len(b);
	}
	inline void ensure(size_t l) {
		if (len() < l) {
			if (readMin(l - len()) == 0) {
				DIE();
			}
		}
	}
	inline size_t ensureuntil(char c) {
		char *p = ptr();
		int i=0;
		for(;i<len();i++)
			if (p[i] == c)
				return i;
		if (read() > 0) {
			for(;i<len();i++)
				if (p[i] == c)
					return i;
		}
		DIE();
	}
	inline const section get(size_t len) {
		ensure(len);
#ifdef DEBUG
		return section(this,ptr(),len);
#else
		return section(len);
#endif
	}
	inline const section getuntilI(char c) {
		char *p = ptr();
		int i=0;
		for(;i<len();i++)
			if (p[i] == c)
				return get(i+1);
		if (read() > 0) {
			for(;i<len();i++)
				if (p[i] == c)
					return get(i+1);
		}
		DIE();
	}
	inline const section getuntilanyI(const char *c) {
		char *p = ptr();
		int i=0;
		for(;i<len();i++)
			if (strchr(c,p[i]) != NULL)
				return get(i+1);
		if (read() > 0) {
			for(;i<len();i++)
				if (strchr(c,p[i]) != NULL)
					return get(i+1);
		}
		DIE();
	}
	inline const section getuntilE(char c) {
		char *p = ptr();
		int i=0;
		for(;i<len();i++)
			if (p[i] == c)
				return get(i);
		if (read() > 0) {
			for(;i<len();i++)
				if (p[i] == c)
					return get(i);
		}
		DIE();
	}
	inline const section getuntilanyE(const char *c) {
		char *p = ptr();
		int i=0;
		for(;i<len();i++)
			if (strchr(c,p[i]) != NULL)
				return get(i);
		if (read() > 0) {
			for(;i<len();i++)
				if (strchr(c,p[i]) != NULL)
					return get(i);
		}
		DIE();
	}
	inline const section getline() {
		return getuntilI('\n');
	}
	inline void release(const section &s) {
#ifdef DEBUG
		if (s.s != this || s.ptr != ptr()) {
			DIE();
		}
#endif
		take(s.len);
	}
};

#endif