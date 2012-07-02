#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <getopt.h>

#include "common.h"
#include "unrar.h"
#include "nzbparser.h"
#include "nzbdownload.h"
#include "proconstream.h"
#include "parserarvolume.h"
#include "command.h"
#include "mutex.h"

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
MutexCond rarfilesMC;

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
		fprintf(stderr,"Read at %lld while we're at %lld\n",st->offset,st->readpos);
		DIE();
	}

	// Gradually read from the vrb to buf.
	size_t bo = st->stream.read(buf,nbytes);
	if (bo == 0) {
		// End of Stream
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
		command::output("extracting", header.FileName, header.UnpSize);
		status = RARProcessFile(h, RAR_EXTRACT, destination, NULL);
		if (status != 0) { break; }
		command::output("extracted", header.FileName);
		status = RARReadHeader(h, &header);
	}
}

FILE *rar_fopen(const char *filename, const char *mode) {
	// Rar-volumes are opened as read-only, extracting files are opened as write.
	if (strcmp(mode,"r") != 0) {
		return fopen(filename,mode);
	}
	_IO_cookie_io_functions_t fns;
	fns.read = rar_read;
	fns.write = rar_write;
	fns.seek = rar_seek;
	fns.close = rar_close;
	map<string,DownloadRarState*>::iterator it;
	rarfilesMC.lock();
	while ((it = rarfiles.find(string(filename))) == rarfiles.end()) {
		rarfilesMC.wait();
	}
	rarfilesMC.unlock();
	DownloadRarState *state = (*it).second;
	return fopencookie(state, mode, fns);
}

void file_download(void *cookie, char *buf, size_t len) {
	DownloadRarState *s = (DownloadRarState*)cookie;
	if (len == 0) {
		// Download is complete.
		s->stream.write_close();
		return;
	}
	s->stream.write(buf,len);
}

string destination;

void *rarthread_run(void *arg) {
	RarVolumeFile *rarfile = (RarVolumeFile*)arg;
	string filename = rarfile->volume.toString();

	char arcname[512];
	std::copy(filename.begin(), filename.end(), arcname);
	arcname[filename.length()] = '\0';

	RARSetFopenCallback(rar_fopen, NULL);
	extractrar(arcname,(char*)destination.c_str());
	
	return NULL;
}

int main(int argc, char * const *args) {
	// Parse command-line options.
	static struct option long_options[] = {
		{"hostname",	required_argument,	0,	'h'},
		{"port",		required_argument,	0,	'p'},
		{"username",	required_argument,	0,	'u'},
		{"password",	required_argument,	0,	's'},
		{"destination",	required_argument,	0,	'd'},
		{0, 0, 0, 0}
	};

	string hostname;
	int port = 119;
	string username;
	string password;
	string nzbfilename;

	int c;
	int option_index = 0;
	while((c = getopt_long(argc,args,"hpus",long_options,&option_index)) != -1) {
		switch(c) {
			case 0: break;
			case 'h': hostname = string(optarg); break;
			case 'p': port = atoi(optarg); break;
			case 'u': username = string(optarg); break;
			case 's': password = string(optarg); break;
			case 'd': destination = string(optarg); break;
			default: DIE(); break;
		}
	}
	if (optind < argc)
	{
		nzbfilename = string(args[optind]);
	}

	if (hostname == "" ||
		username == "" ||
		nzbfilename == "") {
		printf(
			"Usage: %s [OPTION]... NZBFILE\n"
			"Download and extract NZBFILE\n"
			"\n"
			"  -h, --hostname    \n"
			"  -p, --port        \n"
			"  -u, --username    \n"
			"  -s, --password    \n"
			"  -d, --destination \n"
		, args[0]);
		exit(1);
	}

	// 
	Nzb nzb = ParseNZB(nzbfilename.c_str());

	list<RarVolumeFile*> rarlist;

	// Put RAR-volumes from the nzb into rarlist in their correct order. (.part01.rar, .part02.rar, etc)
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


	// Start thread to extract rar-files.
	pthread_t rarthread;
	pthread_attr_t rarattr;
	pthread_attr_init(&rarattr);
	pthread_create(&rarthread, &rarattr, rarthread_run, rarlist.front());


	// Setup downloader.
	NntpServerSettings settings;
	settings.hostname = hostname;
	settings.port = port;
	settings.username = username;
	settings.password = password;

	NntpConnection *connection = nntp_connect(settings);
	DownloadCallback cb;
	cb.f = file_download;

	// Download rar-files from rarlist.
	for(it = rarlist.begin();it != rarlist.end();it++) {
		RarVolumeFile *rarfile = *it;

		// Put rar to be downloaded into rarfiles, so it can be picked up by the rar-extraction-thread.
		rarfilesMC.lock();
		cb.cookie = rarfiles[rarfile->volume.toString()] = new DownloadRarState(rarfile);
		rarfilesMC.signal();
		rarfilesMC.unlock();
		command::output("downloading",rarfile->volume.toString());
		nntp_downloadfile(connection, *rarfile->file, cb);
		command::output("downloaded",rarfile->volume.toString());
	}
	nntp_disconnect(connection);
	return 0;

}
