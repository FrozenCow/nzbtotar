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

using namespace std;

void extractrar(char *filename, char *destination) {
	RAROpenArchiveData oad;
	memset(&oad, sizeof(RAROpenArchiveData), 1);
	oad.ArcName = filename;
	oad.OpenMode = RAR_OM_EXTRACT;
	printf("OpenArchive\n");
	HANDLE h = RAROpenArchive(&oad);
	if (h == NULL) {
		printf("Error opening.\n");
		exit(1);
	}
	if (oad.OpenResult != 0) {
		printf("Error opening2.\n");
		exit(1);
	}
	RARHeaderData header;
	int status;
	printf("ReadHeader...\n");
	status = RARReadHeader(h, &header);
	while (status == 0) {
		status = RARProcessFile(h, RAR_EXTRACT, destination, NULL);
		if (status != 0) { break; }
		printf("ReadHeader...\n");
		status = RARReadHeader(h, &header);
	}
	printf("DONE!\n");
}

void appendToFile(const char *filename, const char *buf, size_t size) {
	FILE * f = fopen(filename, "a");
	for(int i=0;i<size;i++) {
		fprintf(f, "%02x ", buf[i]);
	}
	fprintf(f, "\n");
	fclose(f);
}

#define RARTYPE_PART 1
#define RARTYPE_EXT 2
struct RarVolume {
	int nr;
	int type;
	string prefix;
	RarVolume() : nr(0), type(0), prefix("") { }
	RarVolume(int nr, int type) : nr(nr), type(type) {}
	RarVolume(int nr, int type, string prefix) : nr(nr), type(type), prefix(prefix) {}
	string toString() {
		stringstream ss;
		ss << prefix;
		switch(type) {
		case RARTYPE_PART: {
			char s[3];
			sprintf(s, "%02d", (nr+1) % 100);
			ss << ".part" << s << ".rar";
		} break;
		case RARTYPE_EXT: {
			if (nr == 0) {
				ss << ".rar";
			} else {
				ss << ".r";
				char s[3];
				sprintf(s, "%02d", nr % 100);
				ss << s;
			}
		} break;
		}
		return ss.str();
	}
};

struct RarVolumeFile {
	NzbFile *file;
	RarVolume volume;
	RarVolumeFile(NzbFile *file,RarVolume volume) : file(file), volume(volume) { }
};


struct DownloadRarState {
	RarVolumeFile *f;
	vrb_p vrb;
	size_t readpos;
	size_t offset;
	bool downloaddone;
	char tmp[1024];
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	DownloadRarState(RarVolumeFile *f) :
		f(f),
		vrb(vrb_new(4096, "/tmp/bufferXXXXXX")),
		readpos(0),
		offset(0),
		downloaddone(false)
	{
		pthread_mutexattr_t mutexattr;
		pthread_mutexattr_init(&mutexattr);
		pthread_mutex_init(&mutex, &mutexattr);
		pthread_condattr_t condattr;
		pthread_condattr_init(&condattr);
		pthread_cond_init(&cond, &condattr);
	}
	~DownloadRarState() {
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
		vrb_destroy(vrb);
	}
};

map<string,DownloadRarState*> rarfiles;


char file_content[50*1024*1024];
FILE*testfile=NULL;

FILE*rarfile = NULL;
DownloadRarState*rarstate = NULL;

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
		printf("read %lld %lld %d",st->readpos, st->offset, nbytes);
		DIE();
	}

	// Gradually read from the vrb to buf.
	size_t bo = 0;
	pthread_mutex_lock(&st->mutex);
	while(bo<nbytes) {
		if (st->downloaddone) {
			int len = vrb_data_len(st->vrb);
			printf("Download done, but %d left\n",len);
			if (len == 0) {
				printf("End of Stream found!\n");
				break;
			}
		}
		while (vrb_data_len(st->vrb) == 0 && !st->downloaddone) {
			pthread_cond_wait(&st->cond, &st->mutex);
		}
		size_t len = min(max((size_t)0,nbytes-bo), (size_t)vrb_data_len(st->vrb));
		memcpy(buf+bo,vrb_data_ptr(st->vrb),len);
		vrb_take(st->vrb, len);
		bo += len;

		pthread_cond_signal(&st->cond);
	}
	pthread_mutex_unlock(&st->mutex);

	// If we are at the header, cache it for later use.
	if (st->offset < headerbufcap) {
		memcpy(headerbuf+st->offset, buf, min(headerbufcap-st->offset,bo));
		headerbuflen = bo;
	}

	//printf("READ %ld/%ld @ %ld\n",bo,nbytes,st->offset);

	st->readpos += bo;
	st->offset += bo;
	return bo;
}

ssize_t rar_write(void *cookie, const char *buf, size_t n) {
	DownloadRarState *st = (DownloadRarState*)cookie;
	printf("RAR_WRITE\n");
	return 0;
}
int rar_seek(void *cookie, _IO_off64_t *__pos, int __w) {
	DownloadRarState *st = (DownloadRarState*)cookie;
	_IO_off64_t newpos;
	switch(__w) {
		case SEEK_SET: newpos = *__pos; break;
		case SEEK_CUR: newpos = st->offset + *__pos; break;
		case SEEK_END: newpos = 999999; break;
		default: DIE(); break;
	}
	printf("SEEK %lld > %lld (%d)\n",st->offset,newpos,__w);
	if (__w != SEEK_END && abs((long long)(newpos - st->offset)) > 8192*2 && newpos > 8192) {
		DIE();
	}
	st->offset = newpos;
	*__pos = newpos;
	return 0;
}
int rar_close(void *cookie) {
	DownloadRarState *st = (DownloadRarState*)cookie;
	printf("RAR_CLOSE\n");
	return 0;
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
	rarstate = state;
	rarfile = fopencookie(state, mode, fns);
	return rarfile;
}

bool isLetter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool isNumeric(char c) {
	return c >= '0' && c <= '9';
}
bool isAlphaNumeric(char c) {
	return isLetter(c) || isNumeric(c);
}

bool startsWith(string s, string b) {
	if (s.length() < b.length()) { return false; }
	return s.compare(0,b.length(),b) == 0;
}

bool endsWith(string s, string b) {
	if (s.length() < b.length()) { return false; }
	return s.compare(s.length()-b.length(),b.length(),b) == 0;
}

optional<RarVolume> parseRarVolume(string filename) {
	if (endsWith(filename, ".rar")) {
		int doti = filename.length()-4;
		if (!isNumeric(filename[doti])) {
			return optional<RarVolume>(RarVolume(0,RARTYPE_EXT,filename.substr(0,doti)));
		}
		while(doti >= 0 && isNumeric(filename[--doti])) { }
		if (doti < 0) { return optional<RarVolume>(); }
		int nri = doti+1;
		int nr = atoi(filename.substr(nri,filename.length()-3-nri).c_str())-1;
		if (filename.compare(doti-5, 5, ".part") == 0) {
			return optional<RarVolume>(RarVolume(nr,RARTYPE_PART,filename.substr(0,nri)));
		}
		return optional<RarVolume>();
	}
	if (isNumeric(filename[filename.length()-1]) &&
		isNumeric(filename[filename.length()-2]) &&
		filename[filename.length()-3] == 'r' &&
		filename[filename.length()-4] == '.') {
		int nr = atoi(filename.substr(filename.length()-2).c_str())+1;
		return optional<RarVolume>(RarVolume(nr,RARTYPE_EXT,filename.substr(0,filename.length()-4)));
	}
	return optional<RarVolume>();
}

bool isAny(char c, const char *s) {
	int i=0;
	while(s[i] != '\0') {
		if (c == s[i]) {
			return true;
		}
		i++;
	}
	return false;
}

bool isFilenameChar(char c) {
	return isAlphaNumeric(c) || isAny(c,".-+");
}

string guessFilename(string subject) {
	for(int i=0;i<subject.length();i++) {
		int j;
		for(j=i;j<subject.length() && isFilenameChar(subject[j]);j++) { }
		if (j > 4 && isAlphaNumeric(subject[j-1]) && isAlphaNumeric(subject[j-2]) && isAlphaNumeric(subject[j-3])) {
			if (subject[j-4] == '.' || (subject[j-5] == '.' && isAlphaNumeric(subject[j-4]))) {
				return subject.substr(i,j-i);
			}
		}
	}
	return subject;
}

void file_download(void *cookie, char *buf, size_t len) {
	DownloadRarState *s = (DownloadRarState*)cookie;
	pthread_mutex_lock(&s->mutex);
	if (len == 0) {
		printf("Downloading complete!\n");
		// End of stream
		s->downloaddone = true;
		pthread_cond_signal(&s->cond);
		pthread_mutex_unlock(&s->mutex);
		return;
	}
	int written;
	while(len > 0) {
		while (vrb_space_len(s->vrb) == 0) {
			pthread_cond_wait(&s->cond, &s->mutex);
		}
		int written = vrb_put(s->vrb, buf, len);
		buf += written;
		len -= written;
		pthread_cond_signal(&s->cond);
	}
	pthread_mutex_unlock(&s->mutex);
}

void *run_rar(void *arg) {
	RarVolumeFile *rarfile = (RarVolumeFile*)arg;
	string filename = rarfile->volume.toString();

	char arcname[512];
	std::copy(filename.begin(), filename.end(), arcname);
	arcname[filename.length()] = '\0';

	RARSetFopenCallback(rar_fopen, NULL);
	extractrar(arcname,"storage");
	

	return NULL;
}

FILE *myfopen(const char *filename, const char *modes) {
	if (!endsWith(string(filename),".rar")) {
		printf("Opening %s\n",filename);
		return fopen(filename,modes);
	}
	int size = 50*1024*1024;
	printf("fopen...\n");
	char * c = (char*)malloc(size);
	FILE * f = fopenmemory();
	FILE * b = fopen("/home/bob/projects/nzbgetweb/files/South.Park.S16E07.720p.WEB-DL.AAC2.0.H.264-CtrlHD/South.Park.S16E07.720p.WEB-DL.AAC2.0.H.264-CtrlHD.part01.rar", "r");

	char buff[4096];
	int read;
	while ((read = fread(buff, sizeof(char), 4096, b)) > 0) {
		fwrite(buff, sizeof(char), read, f);
	}
	fclose(b);
	fseek(f, 0, SEEK_SET);
	printf("fopened.\n");
	return f;
}

int main(int argc, const char **args) {
	Nzb nzb = ParseNZB("test.nzb");

	// RARSetFopenCallback(myfopen, NULL);
	// extractrar("South.Park.S16E07.720p.WEB-DL.AAC2.0.H.264-CtrlHD.part01.rar","storage");
	// exit(0);
	list<RarVolumeFile*> rarlist;

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
	pthread_create(&rarthread, &rarattr, run_rar, rarlist.front());


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
