#ifndef PROCONSTREAM_H
#define PROCONSTREAM_H

#include <pthread.h>
#include "vrb.h"

// Producer-Consumer Stream:
// a producer-thread can write bytes and a consumer-thread can read bytes.

class ProconStream {
	vrb_p b;
	bool eos;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
public:
	ProconStream(size_t size);
	~ProconStream();
	void write(char *buf, size_t len);
	void write_close();
	size_t read(char *buf, size_t len);
};

#endif