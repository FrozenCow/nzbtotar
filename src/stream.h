#ifndef STREAM_H
#define STREAM_H

extern "C" {
#include <vrb.h>
}
#include "common.h"

#define DEBUG 1

class bufferedstream;
struct section {
	size_t len;
#ifdef DEBUG
	char *ptr;
	bufferedstream *s;
	inline section(bufferedstream *s, char *ptr, size_t len) : len(len), ptr(ptr), s(s) {}
#else
	inline section(size_t len) : len(len) {}
#endif
};

class bufferedstream {
	int fd;
	vrb_p b;
public:
	inline bufferedstream(int fd, size_t size) {
		this->fd = fd;
		this->b = vrb_new(size, NULL);
	}
	inline ~bufferedstream() {
		vrb_destroy(b);
	}
	inline size_t read() {
		return vrb_read(b, fd, vrb_space_len(b));
	}
	inline size_t read(size_t max) {
		return vrb_read(b, fd, max);
	}
	inline size_t read(size_t min, size_t max) {
		return vrb_read_min(b, fd, max, min);
	}
	inline bool take(size_t n) {
#ifdef DEBUG
		if (n > len()) {
			DIE();
		} 
#endif
		return vrb_take(b, n) == 0;
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
	inline const section get(size_t size) {
#ifdef DEBUG
		return section(this,ptr(),size);
#else
		return section(size);
#endif
	}
	inline const section getuntil(char c) {
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
	inline const section getline() {
		return getuntil('\n');
	}
	inline void release(section &s) {
#ifdef DEBUG
		if (s.s != this || s.ptr != ptr()) {
			DIE();
		}
#endif
		take(s.len);
	}
};

#endif