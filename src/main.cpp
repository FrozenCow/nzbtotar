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

#include <string>
#include <sstream>
#include <list>
#include <vector>

#include "common.h"
#include "crc32.h"
#include "stream.h"

using namespace std;

#define foreach(it, iterable) auto & cat(it,__LINE__) = iterable; for(auto it = cat(it,__LINE__).begin(); it != cat(it,__LINE__).end(); ++it)

#include "nzbfile.cpp"

// optional - either has a value of type T, or has no value at all
template<typename T>
class optional {
  bool _hasValue;
  T _value;
public:
  optional() :
    _hasValue(false) { }
  optional(T value) :
    _hasValue(true),
    _value(value) {}
  inline bool hasValue() {
    return _hasValue;
  }
  inline T value() {
    if (!_hasValue) {
      throw "No value!";
    }
    return _value;
  }
};


#define BUFFERSIZE 1024
struct Buffer {
	int index;
	int size;
	char buffer[1024];
	Buffer() :
		index(0),
		size(0)
	{}
	inline char *current() {
		return buffer+index;
	}
	inline char *begin() {
		return buffer;
	}
	inline char *end() {
		return buffer+size;
	}
	inline int remaining() {
		return size-index;
	}
	inline char *at(int index) {
		return buffer+index;
	}
	inline void advance() {
		index++;
	}
	inline bool eob() {
		return index >= size;
	}
};

Buffer socketbuffer;

int readbuffer(int fd, Buffer &buffer) {
	buffer.index = 0;
	buffer.size = read(fd, buffer.buffer, BUFFERSIZE);
	return buffer.size;
}

void printbytes(char *line) {
	return;
	while(line[0] != '\0') {
		fprintf(stderr,"%02x ",line[0] & 0xff);
		line++;
	}
	fprintf(stderr,"\n");
}

optional<char> readchar(int fd, Buffer &buffer) {
	if (buffer.eob()) {
		readbuffer(fd, buffer);
		if (buffer.eob()) { return optional<char>(); }
	}
	char result = *buffer.current();
	buffer.advance();
	return optional<char>(result);
}

int creaduntil(int fd,Buffer &buffer,char *line,int n,char delimeter) {
	n--; // To add '\0'

	optional<char> oc;

	int lineSize = 0;
	while ((oc = readchar(fd,buffer)).hasValue()) {
		char c = oc.value();
		if (c == delimeter) {
			line[lineSize] = '\0';
			printbytes(line);
			return lineSize;
		} else if (lineSize < n) {
			line[lineSize] = c;
			lineSize++;
		} else {
			// Line is too long.
			DIE();
		}
	}
	return lineSize;
}

int creadline(int fd,Buffer &buffer,char *line,int n) {
	int len = creaduntil(fd,buffer,line,n,'\n');
	if (line[len-1] == '\r') {
		line[len-1] = '\0';
		len--;
	} else {
		DIE();
	}
	return len;
}

optional<int> indexOf(string s, char c,int start) {
	for(int i=start;i<s.length();i++) {
		if (s[i] == c) { return optional<int>(i); }
	}
	return optional<int>();
}

string readuntil(int fd, char delimeter) {
	static char linebuf[1024];
	int len = creaduntil(fd,socketbuffer,linebuf,1024,delimeter);
	return string(linebuf,len);
}

string readline(int fd) {
	static char linebuf[1024];
	int linelength = creadline(fd,socketbuffer,linebuf,1024);
	return string(linebuf,linelength);
}

inline bool iswhitespace(char c) {
	return c == '\n' || c == '\r' || c == ' ' || c == '\t';
}

inline bool iscrorlf(char c) {
	return c == '\r' || c == '\n';
}

int atoin(char *c,size_t len) {
	// This is bullshit.
	char e = c[len];
	c[len] = '\0';
	int result = atoi(c);
	c[len] = e;
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
	if (r.len <= 2)
		return r;
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

void writeline(int fd, string s) {
	fprintf(stderr,"> %s\n",s.c_str());
	write(fd,s.c_str(),s.length());
	write(fd,"\r\n",2);
}

bool startsWith(string s, string cmp) {
	if (s.length() < cmp.length()) { return false; }
	for(int i=0;i<cmp.length();i++) {
		if (s[i] != cmp[i]) { return false; }
	}
	return true;
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
		printf("%.*s: %.*s\n",header.name.len,header.name.ptr, header.value.len,header.value.ptr);
		s.release(sc);
	}
}

bool readYBegin(bufferedstream &s, YEncHead &yencHead) { // Parsing ybegin
	printf("readYBegin\n");
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
	printf("readYPart\n");
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


void readYEnc(bufferedstream &s, YEncHead &head, YEncPart &part) { // Parsing yEnc lines
	int partSize = part.end - (part.begin - 1);
	int offset = part.begin;
	YEncEnd yencEnd;

	unsigned long checksum = crc32_init();
	bool eoa = false;

newline:
	if (readYEnd(s, yencEnd)) {
		// Do something.
	}
	if (endOfArticle(s)) {
		return;
	}
	s.ensure(2);
	if (s[0] == '.' && s[1] == '.') {
		// A double dot at the beginning of a line is a dot escaped by another dot... (?!)
		s.take(1);
	}

	while(!eoa) {
		s.ensure(1);
		switch(s[0]) {
			case '\0': { DIE(); } break;
			case '=': {
				s.take(1);
				s.ensure(1);
				handleByte(s[0] - 64 - 42, offset, head, part);
			} break;
			case '\r': {
				// Ignore \r
			} break;
			case '\n': {
				s.take(1);
				goto newline;
			} break;
			default: {
				handleByte(s[0] - 42, offset, head, part);
			} break;
		}
		s.take(1);
	}
	checksum = crc32_finish(checksum);

	// TODO: Fix crc32
	if (yencEnd.crc32 != checksum) {
		DIE();
	}
}

int main(int argc, const char **args) {
	Nzb *nzb = ParseNZB();

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) { DIE(); }
	
	hostent* server = gethostbyname("eu.news.bintube.com");
	if (server == NULL) {
		DIE();
	}

	struct sockaddr_in serv_addr;
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(119);

	if (connect(fd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		DIE();
	}

	bufferedstream s(fd,4096);

	if (readstatus(s) != 200) {
		DIE();
	}
	writeline(fd,"AUTHINFO USER Afosick");
	if (readstatus(s) != 381) {
		DIE();
	}
	writeline(fd,"AUTHINFO PASS m96xn7");
	if (readstatus(s) != 281) {
		DIE();
	}

{ // Download file-segments
	vector<File*> &files = nzb->files;
	for(int i=0;i<files.size();i++) {
		File &file = *files[i];
		string group = file.groups[0];
		writeline(fd,"GROUP "+group);
		if (readstatus(s) != 211) {
			DIE();
		}

		vector<Segment*> & segments = file.segments;
		for(int j=0;j<segments.size();j++) {
			Segment &segment = *segments[j];

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

			readYEnc(s,yencHead,yencPart);
		}
	}
}
	close(fd);
	return 0;
}
