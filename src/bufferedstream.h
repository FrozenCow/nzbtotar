#ifndef STREAM_H
#define STREAM_H

#include <errno.h>
#include <cstring>
extern "C" {
#include <vrb.h>
}
#include "common.h"

#define CHECK_STREAM 1

class bufferedstream;
struct section {
	size_t len;
#ifdef CHECK_STREAM
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
		fd(fd) {
			b = vrb_new(size, "/tmp/bufferXXXXXX");
			if (b == NULL) { DIE(); }
		}
	inline ~bufferedstream() {
		vrb_destroy(b);
	}
	char& operator[](const size_t i) {
#ifdef CHECK_STREAM
		if (i >= len()) {
			DIE();
		}
#endif
		return vrb_data_ptr(b)[i];
	}
	inline size_t capacity() {
		return vrb_capacity(b);
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
#ifdef CHECK_STREAM
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
#ifdef CHECK_STREAM
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
#ifdef CHECK_STREAM
		if (s.s != this || s.ptr != ptr()) {
			DIE();
		}
#endif
		take(s.len);
	}
};

#endif