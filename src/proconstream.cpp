#include "proconstream.h"
#include <string.h>
#include <algorithm>

using namespace std;

ProconStream::ProconStream(size_t size) {
	eos = false;
	b = vrb_new(size, "/tmp/bufferXXXXXX");
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutex_init(&mutex, &mutexattr);
	pthread_condattr_t condattr;
	pthread_condattr_init(&condattr);
	pthread_cond_init(&cond, &condattr);
}

ProconStream::~ProconStream()  {
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
	vrb_destroy(b);
}

void ProconStream::write(char *buf, size_t len) {
	while(len > 0) {
		pthread_mutex_lock(&mutex);
		while (vrb_space_len(b) == 0) {
			pthread_cond_wait(&cond, &mutex);
		}
		int written = vrb_put(b, buf, len);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
		buf += written;
		len -= written;
	}
}

void ProconStream::write_close() {
	pthread_mutex_lock(&mutex);
	// Mark end of stream
	eos = true;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
}

size_t ProconStream::read(char *buf, size_t len) {
	size_t bo = 0;
	pthread_mutex_lock(&mutex);
	while(bo<len) {
		if (eos) {
			int data_available = vrb_data_len(b);
			if (data_available == 0) {
				break;
			}
		}
		while (vrb_data_len(b) == 0 && !eos) {
			pthread_cond_wait(&cond, &mutex);
		}
		size_t copylen = min(max((size_t)0,len-bo), (size_t)vrb_data_len(b));
		memcpy(buf+bo,vrb_data_ptr(b),copylen);
		vrb_take(b, copylen);
		bo += copylen;

		pthread_cond_signal(&cond);
	}
	pthread_mutex_unlock(&mutex);

	return bo;
}
