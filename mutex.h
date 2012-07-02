#ifndef MUTEX_H
#define MUTEX_H
#include <pthread.h>

class Mutex;
class Cond;

class Mutex {
	pthread_mutex_t mutex;
public:
	inline Mutex() {
		pthread_mutexattr_t mutexattr;
		pthread_mutexattr_init(&mutexattr);
		pthread_mutex_init(&mutex, &mutexattr);
	}
	inline ~Mutex() {
		pthread_mutex_destroy(&mutex);
	}
	inline void lock() {
		pthread_mutex_lock(&mutex);
	}
	inline void unlock() {
		pthread_mutex_unlock(&mutex);
	}
	friend class Cond;
};

class Cond {
	pthread_cond_t cond;
public:
	inline Cond() {
		pthread_condattr_t condattr;
		pthread_condattr_init(&condattr);
		pthread_cond_init(&cond, &condattr);
	}
	inline void signal() {
		pthread_cond_signal(&cond);
	}
	inline void wait(Mutex &mutex) {
		pthread_cond_wait(&cond,&mutex.mutex);
	}
	inline ~Cond() {
		pthread_cond_destroy(&cond);
	}
};

class MutexCond {
	Mutex mutex;
	Cond cond;
public:
	inline MutexCond() : mutex(), cond() { }
	inline void lock() { mutex.lock(); }
	inline void signal() { cond.signal(); }
	inline void wait() { cond.wait(mutex); }
	inline void unlock() { mutex.unlock(); }
};

#endif