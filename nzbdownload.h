
#ifndef NZBDOWNLOAD_H
#define NZBDOWNLOAD_H
#include "nzbparser.h"
#include "bufferedstream.h"
struct NntpConnection {
	int fd;
	bufferedstream *s;
	NntpConnection(int fd, bufferedstream *s) : fd(fd), s(s) { }
};

struct NntpServerSettings {
	string hostname;
	int port;
	string username;
	string password;
};

typedef void (*DownloadCallbackF)(void* cookie, char *buf, size_t len);
struct DownloadCallback {
	DownloadCallbackF f;
	void *cookie;
};

NntpConnection *nntp_connect(NntpServerSettings &settings);
void nntp_downloadfile(NntpConnection *connection, NzbFile &file, DownloadCallback callback);
void nntp_disconnect(NntpConnection *connection);

#endif