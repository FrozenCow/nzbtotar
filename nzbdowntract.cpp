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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

using namespace std;

void writeTarHeader(const char *filename, uint64_t size) {
	struct {
		char name[100];               /*   0-99 */
		char mode[8];                 /* 100-107 */
		char uid[8];                  /* 108-115 */
		char gid[8];                  /* 116-123 */
		char size[12];                /* 124-135 */
		char mtime[12];               /* 136-147 */
		char chksum[8];               /* 148-155 */
		char typeflag;                /* 156-156 */
		char linkname[100];           /* 157-256 */
		char magic[6];                /* 257-262 */
		char version[2];              /* 263-264 */
		char uname[32];               /* 265-296 */
		char gname[32];               /* 297-328 */
		char devmajor[8];             /* 329-336 */
		char devminor[8];             /* 337-344 */
		char prefix[155];             /* 345-499 */
		char padding[8];             /* 500-512 (pad to exactly the TAR_BLOCK_SIZE) */
		char magic2[4];
	} header;
	command::output("writeTarHeader",filename,size);
	memset(header.name, 0, 100);
	strncpy(header.name, filename, 100);
	sprintf(header.mode, "%07o", 0777);
	sprintf(header.uid, "%07o", 1000);
	sprintf(header.gid, "%07o", 1000);
	sprintf(header.size, "%011lo", size);
	sprintf(header.mtime, "%011o", 011774322254);

	memset(header.chksum, 0, 8);
	memset(header.chksum, ' ', 8);

	header.typeflag = '0';
	memset(header.linkname, 0, 100);
	memcpy(header.magic, "ustar ", 6);
	memcpy(header.version, " \0", 2);
	
	memset(header.uname, 0, 32);
	strcpy(header.uname, "");

	memset(header.gname, 0, 32);
	strcpy(header.gname, "");

	memset(header.devmajor, 0, 8);
	memset(header.devminor, 0, 8);
	memset(header.prefix, 0, 155);
	memset(header.padding, 0, 8);
	strcpy(header.magic2, "tar");

	unsigned int chksum = 0;
	for(int i=0;i<sizeof(header);i++) {
		chksum += ((char*)&header)[i];
	}
	sprintf(header.chksum, "%06o", chksum);

	fwrite(&header, sizeof(header), 1, stdout);
	fflush(stdout);
}

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

command::keyword ERarKeyword(int result) {
	switch(result) {
		case 0: return command::keyword("SUCCESS");
		case ERAR_END_ARCHIVE: return command::keyword("ERAR_END_ARCHIVE");
		case ERAR_NO_MEMORY: return command::keyword("ERAR_NO_MEMORY");
		case ERAR_BAD_DATA: return command::keyword("ERAR_BAD_DATA");
		case ERAR_BAD_ARCHIVE: return command::keyword("ERAR_BAD_ARCHIVE");
		case ERAR_UNKNOWN_FORMAT: return command::keyword("ERAR_UNKNOWN_FORMAT");
		case ERAR_EOPEN: return command::keyword("ERAR_EOPEN");
		case ERAR_ECREATE: return command::keyword("ERAR_ECREATE");
		case ERAR_ECLOSE: return command::keyword("ERAR_ECLOSE");
		case ERAR_EREAD: return command::keyword("ERAR_EREAD");
		case ERAR_EWRITE: return command::keyword("ERAR_EWRITE");
		case ERAR_SMALL_BUF: return command::keyword("ERAR_SMALL_BUF");
		case ERAR_UNKNOWN: return command::keyword("ERAR_UNKNOWN");
		case ERAR_MISSING_PASSWORD: return command::keyword("ERAR_MISSING_PASSWORD");
		default: return command::keyword("UNKNOWN");
	}
}

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
		uint64_t size = header.UnpSize;//(uint64_t)header.UnpSizeHigh << 4 || (uint64_t)header.UnpSize;
		command::output("extracting_file",header.FileName,size);
		writeTarHeader(header.FileName, size);
		status = RARProcessFile(h, RAR_EXTRACT, destination, NULL);
		command::output("extracted_file",ERarKeyword(status));
		if (status != 0) { break; }
		status = RARReadHeader(h, &header);
	}
}

FILE *rar_fopen(const char *filename, const char *mode) {
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

__ssize_t destination_read(void *cookie, char *buf, size_t nbytes) {
	DIE();
	return 0;
}
ssize_t destination_write(void *cookie, const char *buf, size_t n) {
	return fwrite((const void*)buf,sizeof(char),n,stdout);
}
int destination_seek(void *cookie, _IO_off64_t *__pos, int __w) {
	DIE();
	return 0;
}
int destination_close(void *cookie) {
	return 0;
}

FILE *destination_fopen(const char *filename, const char *mode) {
	_IO_cookie_io_functions_t fns;
	fns.read = destination_read;
	fns.write = destination_write;
	fns.seek = destination_seek;
	fns.close = destination_close;
	return fopencookie(NULL, mode, fns);
}

FILE *custom_fopen(const char *filename, const char *mode) {
	if (strcmp(mode,"r") == 0) // Rar-volumes are opened as read-only.
		return rar_fopen(filename, mode);
	else if (strcmp(mode,"w") == 0) // Destination files are opened as write-only.
		return destination_fopen(filename, mode);
	else // For other files (are there any others?) don't hook io-operations.
		return fopen(filename,mode);
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

void *rarthread_run(void *arg) {
	RarVolumeFile *rarfile = (RarVolumeFile*)arg;
	string filename = rarfile->volume.toString();

	char arcname[512];
	std::copy(filename.begin(), filename.end(), arcname);
	arcname[filename.length()] = '\0';

	RARSetFopenCallback(custom_fopen, NULL);
	char empty[1] = "";
	extractrar(arcname,empty);
	
	return NULL;
}

int main(int argc, char * const *args) {
	// Parse command-line options.
	static struct option long_options[] = {
		{"hostname",	required_argument,	0,	'h'},
		{"port",		required_argument,	0,	'p'},
		{"username",	required_argument,	0,	'u'},
		{"password",	required_argument,	0,	's'},
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
		, args[0]);
		exit(1);
	}

	command::output("parsing",nzbfilename);
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

	command::output("settings",hostname,port,username,password);

	command::output("connecting");
	NntpConnection *connection = nntp_connect(settings);
	DownloadCallback cb;
	cb.f = file_download;

	command::output("starting");

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
