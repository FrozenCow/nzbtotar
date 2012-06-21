#include "nzbdownload.h"
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "common.h"
#include "bufferedstream.h"
#include "crc32.h"

#define OUTPUTSIZE 8192
#define BUFFERSIZE (OUTPUTSIZE*2)

inline bool iswhitespace(char c) {
	return c == '\n' || c == '\r' || c == ' ' || c == '\t';
}

inline bool iscrorlf(char c) {
	return c == '\r' || c == '\n';
}

int atoin(char *c,size_t len) {
	// This is bullshit.
	istringstream ss(string(c,len));
	int result;
	ss >> result;
	// char e = c[len];
	// c[len] = '\0';
	// int result = atoi(c);
	// c[len] = e;
	return result;
}

int readstatus(bufferedstream &s) {
	s.ensure(4);
	int result = 0;
	if (s.ptr()[3] == ' ' || s.ptr()[3] == '\r' || s.ptr()[3] == '\n') {
		result = atoin(s.ptr(), 3);
	} else {
		DIE();
	}
	s.takeline();
	return result;
}

struct Header {
	str name;
	str value;
	Header() : name(), value() { }
	Header(str name, str value) : name(name), value(value) { }
};
const section readheader(bufferedstream &s, Header &header) {
	const section r = s.getline();
	if (r.len <= 2) {
		header.name = str();
		header.value = str();
		return r;
	}
	for(int i=0;i<r.len;i++) {
		if (s[i] == ':') {
			str name = str(s.ptr(),i);
			i++; while(iswhitespace(s[i])) { i++; }
			int j = r.len-1;
			while(iswhitespace(s[j])) { j--; }
			str value = str(&(s[i]),j-i+1);
			header = Header(name,value);
			return r;
		}
	}
	DIE();
}

void write(int fd, const char *s,size_t len) {
	send(fd,s,len,0);
}

void writeline(int fd, string s) {
	fprintf(stderr,"> %s\n",s.c_str());
	write(fd,s.c_str(),s.length());
	write(fd,"\r\n",2);
}

template<typename T>
optional<T> parse(string s) {
	T r;
	istringstream ss(s);
	if (ss >> r) {
		return optional<T>(r);
	}
	return optional<T>();
}

template<typename T>
T parseD(string s) {
	T r;
	istringstream ss(s);
	if (ss >> r) {
		return r;
	}
	DIE();
	// Satisfy the compiler:
	return T();
}

template<typename T>
T parseD(const char *s) {
	return parseD<T>(string(s));
}

template<typename T>
T parseD(str &s) {
	return parseD<T>(string(s.ptr,s.len));
}

struct YEncHead {
	int part;
	int total;
	int line;
	size_t size;
	string name;
};

struct YEncPart {
	int begin;
	int end;
};

struct YEncEnd {
	unsigned long crc32;
};

struct KeyValue {
	string key;
	string value;
};

void breakpoint() {
	printf("BREAK\n");
}

string filename = "";
FILE * filehandle;
int lastoffset;

char changedbytes[10];
int changedbytesi = 0;
void handleByte(char c,int offset,YEncHead &head,YEncPart &part) {
	/*if (head.name.compare(filename) != 0) {
		printf("file: %s\n",head.name.c_str());
		if (!filename.empty()) {
			fclose(filehandle);
		}
		filename = head.name;
		filehandle = fopen(filename.c_str(), "w");
		lastoffset = 0;
	}
	fwrite(&c, sizeof(char), 1, filehandle);*/
	if (head.name.compare(filename) != 0) {
		printf("file: %s\n",head.name.c_str());
		if (!filename.empty()) {
			fclose(filehandle);
		}
		filename = head.name;
		filehandle = fopen(("/media/data/projects/nzbgetweb/files/test/"+filename).c_str(), "r");
		lastoffset = 0;
	}
	//fwrite(&c, sizeof(char), 1, filehandle);
	char compare;
	fread(&compare, sizeof(char), 1, filehandle);
	lastoffset++;
	if (compare != c || changedbytesi > 0) {
		changedbytes[changedbytesi] = c;
		changedbytesi++;
		if (changedbytesi == 1) {
			breakpoint();
		}
	}
}

char *strnchr(char *str, size_t len, char c) {
	for(int i=0;i<len;i++)
		if (str[i] == c)
			return str+i;
	return NULL;
}

size_t indexOf(char *str, size_t len, char c) {
	size_t i;
	for(i=0;i<len;i++)
		if (str[i] == c)
			return i;
	return i;
}

size_t indexOf(str s, char c) {
	size_t i;
	for(i=0;i<s.len;i++)
		if (s.ptr[i] == c)
			return i;
	return i;
}

size_t indexOf(str s, int clen, const char *c) {
	size_t i;
	for(i=0;i<s.len;i++)
		for(int j=0;j<clen;j++) {
			if (s.ptr[i] == c[j]) {
				return i;
			}
		}
	return i;
}

size_t indexOfWhitespace(str s) {
	return indexOf(s,5," \t\r\n");
}

const section readKeyValue(bufferedstream &s, Header &header) {
	s.takewhile(' ');
	s.ensure(2);
	if (iscrorlf(*s.ptr())) {
		header = Header();
		return s.getline();
	}
	const section r = s.getuntilanyE(" \r\n");
	str name = str(s.ptr(), indexOf(s.ptr(),r.len,'='));
	char *svalue = s.ptr()+name.len+1;
	str value = str(svalue,indexOfWhitespace(str(svalue,r.len-name.len-1)));

	header = Header(name,value);
	return r;
}

void readHeaders(bufferedstream &s) {
	while(true) {
		Header header;
		const section sc = readheader(s, header);
		if (header.name.len == 0) {
			s.release(sc);
			break;
		}
		//printf("%.*s: %.*s\n",header.name.len,header.name.ptr, header.value.len,header.value.ptr);
		s.release(sc);
	}
}

bool readYBegin(bufferedstream &s, YEncHead &yencHead) { // Parsing ybegin
	s.ensure(8);
	if (strncmp(s.ptr(),"=ybegin ",8) != 0) {
		return false;
	}
	s.take(8);
	while (true) {
		Header header;
		const section sc = readKeyValue(s, header);
		if (header.name.len == 0) {
			s.release(sc);
			return true;
		}
		if (header.name.equals("part")) { yencHead.part = parseD<int>(header.value); }
		else if (header.name.equals("line")) { yencHead.line = parseD<int>(header.value); }
		else if (header.name.equals("size")) { yencHead.size = parseD<int>(header.value); }
		else if (header.name.equals("total")) { yencHead.total = parseD<int>(header.value); }
		else if (header.name.equals("name")) { yencHead.name = string(header.value.ptr,header.value.len); /* TODO: Fix? */ }

		s.release(sc);
	}
}

bool readYPart(bufferedstream &s,YEncPart &yencPart) { // Parsing ypart
	s.ensure(7);
	if (strncmp(s.ptr(),"=ypart ",7) != 0) {
		return false;
	}
	s.take(7);
	while (true) {
		Header header;
		const section sc = readKeyValue(s, header);
		if (header.name.len == 0) {
			s.release(sc);
			return true;
		}

		if (header.name.equals("begin")) { yencPart.begin = parseD<int>(header.value); }
		else if (header.name.equals("end")) { yencPart.end = parseD<int>(header.value); }

		s.release(sc);
	}
}

bool readYEnd(bufferedstream &s,YEncEnd &yencEnd) { // Parsing ypart
	s.ensure(6);
	if (strncmp(s.ptr(),"=yend ",6) != 0) {
		return false;
	}
	s.take(6);
	while (true) {
		Header header;
		const section sc = readKeyValue(s, header);
		if (header.name.len == 0) {
			s.release(sc);
			return true;
		}
		if (header.name.equals("crc32")) { yencEnd.crc32 = parseD<int>(header.value); }
		s.release(sc);
	}
}

bool endOfArticle(bufferedstream &s) {
	s.ensure(3);
	if (s[0] == '.') {
		if (s[1] == '\n') {
			s.take(2);
			return true;
		}
		if (s[1] == '\r' && s[2] == '\n') {
			s.take(3);
			return true;
		}
	}
	return false;
}


void readYEnc(bufferedstream &s, YEncHead &head, YEncPart &part, DownloadCallback callback) { // Parsing yEnc lines
	int partSize = part.end - (part.begin - 1);
int offset = part.begin;
	YEncEnd yencEnd;

	unsigned long checksum = crc32_init();
	int readi = 0;
	int writei = 0;
newline:
	// Handle escaped dots at beginning of lines.
	s.ensure(readi+2);
	if (s[0] == '.' && s[1] == '.') {
		readi++;
	} else {
		if (endOfArticle(s)) {
			goto endofenc;
		}

		if (readYEnd(s, yencEnd)) {
			goto newline;
		}
	}

	while(true) {
		s.ensure(readi+1);
		if (s[readi] == '\0') { DIE(); } // Incorrect character
		if (s[readi] == '\r') { readi++; continue; } // Ignore return-character
		if (s[readi] == '\n') { readi++; goto newline; } // Handle new lines
		if (s[readi] == '=') { readi++; s.ensure(readi+1); s[writei] = s[readi] - 64; }
		else { s[writei] = s[readi]; }
		s[writei] -= 42;
		checksum = crc32_add(checksum, s[writei]);

		readi++; writei++;

		if (writei >= OUTPUTSIZE) {
			callback.f(callback.cookie, s.ptr(), writei);
			s.take(writei);
			readi -= writei;
			writei = 0;
		}
	}

endofenc:
	if (writei > 0) {
		callback.f(callback.cookie, s.ptr(), writei);
		readi -= writei;
		writei = 0;
		s.take(writei);
	}

	checksum = crc32_finish(checksum);

	if (yencEnd.crc32 != 0 && yencEnd.crc32 != checksum) {
		DIE();
	}
}

NntpConnection *nntp_connect(NntpServerSettings &settings) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) { DIE(); }
	
	hostent* server = gethostbyname(settings.hostname.c_str());
	if (server == NULL) {
		close(fd);
		DIE();
	}

	struct sockaddr_in serv_addr;
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(settings.port);

	if (connect(fd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		close(fd);
		DIE();
	}

	bufferedstream &s = *(new bufferedstream(fd,BUFFERSIZE));


	if (readstatus(s) != 200) {
		close(fd);
		DIE();
	}
	writeline(fd,"AUTHINFO USER "+settings.username);
	if (readstatus(s) != 381) {
		close(fd);
		DIE();
	}
	writeline(fd,"AUTHINFO PASS "+settings.password);
	int status = readstatus(s);
	if (status != 281) {
		printf("%d\n",status);
		close(fd);
		DIE();
	}

	return new NntpConnection(fd,&s);
}

void nntp_disconnect(NntpConnection *connection) {
	writeline(connection->fd,"QUIT");
	close(connection->fd);
	delete connection->s;
	delete connection;
}

void nntp_downloadfile(NntpConnection *connection, NzbFile &file, DownloadCallback callback) {
	int fd = connection->fd;
	bufferedstream &s = *(connection->s);
	string group = file.groups[0];
	printf("Downloading article...\n");
	writeline(fd,"GROUP "+group);
	if (readstatus(s) != 211) {
		DIE();
	}

	vector<NzbSegment*> & segments = file.segments;
	for(int j=0;j<segments.size();j++) {
		NzbSegment &segment = *segments[j];


		writeline(fd,"ARTICLE <"+segment.article+">");
		if (readstatus(s) != 220) {
			DIE();
		}

		readHeaders(s);

		YEncHead yencHead;
		if (!readYBegin(s,yencHead)) {
			DIE();
		}

		YEncPart yencPart;
		if (!readYPart(s,yencPart)) {
			DIE();
		}

		readYEnc(s,yencHead,yencPart,callback);
	}

	callback.f(callback.cookie, NULL, 0);
}