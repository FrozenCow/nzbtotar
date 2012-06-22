#define _GNU_SOURCE
#define __USE_GNU
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//#include "nzbfetch/nzb_fetch.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <expat.h>

#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <map>
#include <algorithm>

#include "common.h"
#include "crc32.h"
#include "bufferedstream.h"

#include "unrar.h"
#include "nzbparser.h"
#include "nzbdownload.h"
#include "memoryfile.h"
#include "proconstream.h"
#include "parserarvolume.h"

using namespace std;

struct RarVolumeFile {
	NzbFile *file;
	RarVolume volume;
	RarVolumeFile(NzbFile *file,RarVolume volume) : file(file), volume(volume) { }
};

// Combines data needed for downloading as well as extracting for a single rar-volume.
struct DownloadRarState {
	RarVolumeFile *f;
	ProconStream stream;
	size_t readpos;
	size_t offset;
	DownloadRarState(RarVolumeFile *f) :
		f(f),
		stream(16384),
		readpos(0),
		offset(0)
	{}
};

map<string,DownloadRarState*> rarfiles;

// A cache to place the header of a RAR-volume.
char headerbuf[8192*2];
size_t headerbufcap = 8192*2;
size_t headerbuflen;

__ssize_t rar_read(void *cookie, char *buf, size_t nbytes) {
	DownloadRarState *st = (DownloadRarState*)cookie;

	if (st->readpos != st->offset) {
		// Uhoh, our vrb isn't at the same location as unrar wants to read...
		// This should only happen for the header (which is read twice), so
		// we should have cached the header.
		if (st->offset <= headerbuflen) {
			size_t len = min(nbytes,headerbuflen-st->offset);
			memcpy(buf, headerbuf+st->offset, len);
			//printf("READ %ld @ %ld\n",len,st->offset);
			st->offset += len;
			return len;
		}
		// If unrar wants to read something else, we have a problem since we have not cached that.
		// This should never happen.
		printf("read %lld %lld %d\n",st->readpos, st->offset, nbytes);
		DIE();
	}

	// Gradually read from the vrb to buf.
	size_t bo = st->stream.read(buf,nbytes);
	if (bo == 0) {
		// End of Stream
		printf("Unrar End Of Stream\n");
	}

	// If we are at the header, cache it for later use.
	if (st->offset < headerbufcap) {
		memcpy(headerbuf+st->offset, buf, min(headerbufcap-st->offset,bo));
		headerbuflen = bo;
	}

	st->readpos += bo;
	st->offset += bo;
	return bo;
}

ssize_t rar_write(void *cookie, const char *buf, size_t n) {
	// libunrar called fwrite on a RAR-volume.
	// This should never happen, so just die.
	DIE();
	return 0;
}
int rar_seek(void *cookie, _IO_off64_t *__pos, int __w) {
	DownloadRarState *st = (DownloadRarState*)cookie;
	// libunrar called fseek on a RAR-volume.
	_IO_off64_t newpos;
	switch(__w) {
		case SEEK_SET: newpos = *__pos; break;
		case SEEK_CUR: newpos = st->offset + *__pos; break;
		case SEEK_END: newpos = 999999; break;
		default: DIE(); break;
	}
	printf("SEEK %lld > %lld (%d)\n",st->offset,newpos,__w);
	// if (__w == SEEK_SET && newpos < st->offset && st->offset > 20000000) {
	// 	DIE();
	// }
	st->offset = newpos;
	*__pos = newpos;
	return 0;
}
int rar_close(void *cookie) {
	DownloadRarState *st = (DownloadRarState*)cookie;
	// libunrar called fclose on a RAR-volume.
	return 0;
}

// Extracts a file to a certain destination using libunrar.
void extractrar(char *filename, char *destination) {
	RAROpenArchiveData oad;
	memset(&oad, sizeof(RAROpenArchiveData), 1);
	oad.ArcName = filename;
	oad.OpenMode = RAR_OM_EXTRACT;

	HANDLE h = RAROpenArchive(&oad);
	if (h == NULL) {
		DIE();
	}
	if (oad.OpenResult != 0) {
		DIE();
	}
	RARHeaderData header;
	int status;
	status = RARReadHeader(h, &header);
	while (status == 0) {
		printf("extracting %s\n", header.FileName);
		status = RARProcessFile(h, RAR_EXTRACT, destination, NULL);
		if (status != 0) { break; }
		status = RARReadHeader(h, &header);
	}
}

FILE *rar_fopen(const char *filename, const char *mode) {
	printf("fopen %s %s\n",filename,mode);
	if (strcmp(mode,"r") != 0) {
		return fopen(filename,mode);
	}
	_IO_cookie_io_functions_t fns;
	fns.read = rar_read;
	fns.write = rar_write;
	fns.seek = rar_seek;
	fns.close = rar_close;

	printf("RAR: Waiting for %s...\n",filename);
	map<string,DownloadRarState*>::iterator it;
	while ((it = rarfiles.find(string(filename))) == rarfiles.end()) {
	}
	printf("RAR: Got %s.\n",filename);
	DownloadRarState *state = (*it).second;
	return fopencookie(state, mode, fns);
}

void file_download(void *cookie, char *buf, size_t len) {
	DownloadRarState *s = (DownloadRarState*)cookie;
	if (len == 0) {
		// Download is complete.
		printf("Downloading complete!\n");
		s->stream.write_close();
		return;
	}
	s->stream.write(buf,len);
}

void *rarthread_run(void *arg) {
	RarVolumeFile *rarfile = (RarVolumeFile*)arg;
	string filename = rarfile->volume.toString();

	char arcname[512];
	std::copy(filename.begin(), filename.end(), arcname);
	arcname[filename.length()] = '\0';

	RARSetFopenCallback(rar_fopen, NULL);
	extractrar(arcname,"storage");
	
	return NULL;
}

int main(int argc, const char **args) {
	Nzb nzb = ParseNZB("test.nzb");

	list<RarVolumeFile*> rarlist;

	// Put RAR-volumes from the nzb into rarlist in the correct order.
	list<RarVolumeFile*>::iterator it;
	for(int i=0;i<nzb.files.size();i++) {
		string filename = guessFilename(nzb.files[i]->subject);
		if (filename.empty()) { delete nzb.files[i]; continue; }
		optional<RarVolume> ovolume = parseRarVolume(filename);
		if (!ovolume.hasValue()) { continue; }
		for(it = rarlist.begin();it != rarlist.end();it++) {
			if ((*it)->volume.nr > ovolume.value().nr) {
				break;
			}
		}
		rarlist.insert(it, new RarVolumeFile(nzb.files[i],ovolume.value()));
	}

	pthread_t rarthread;
	pthread_attr_t rarattr;
	pthread_attr_init(&rarattr);
	pthread_create(&rarthread, &rarattr, rarthread_run, rarlist.front());


	NntpServerSettings settings;
	//settings.hostname = "eu.news.bintube.com";
	settings.hostname = "us.news.bintube.com";
	settings.port = 119;
	settings.username = "Afosick";
	settings.password = "m96xn7";

	NntpConnection *connection = nntp_connect(settings);
	DownloadCallback cb;
	cb.f = file_download;

	for(it = rarlist.begin();it != rarlist.end();it++) {
		RarVolumeFile *rarfile = *it;
		printf("NZB: Downloading %s...\n",rarfile->volume.toString().c_str());

		cb.cookie = rarfiles[rarfile->volume.toString()] = new DownloadRarState(rarfile);
		nntp_downloadfile(connection, *rarfile->file, cb);
	}
	nntp_disconnect(connection);
	return 0;

}
