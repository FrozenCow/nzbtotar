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

int readstatus(int fd) {
	string s = readline(fd);
	stringstream ss(s);
	int result = 0;
	ss >> result;
	return result;
}

struct Header {
	string name;
	string value;
	Header() { }
	Header(string name, string value) : name(name), value(value) { }
};
optional<Header> readheader(int fd) {
	static char linebuf[1024];
	int linelen = creadline(fd,socketbuffer,linebuf,1024);
	if (linelen == 0) {
		return optional<Header>();
	}
	for(int i=0;i<linelen;i++) {
		if (linebuf[i] == ':') {
			linebuf[i] = '\0';
			string name = string(linebuf,i);
			i++;
			while(linebuf[i] == ' ') { i++; }
			string value = string(&(linebuf[i]),linelen-i);
			return optional<Header>(Header(name,value));
		}
	}
	return optional<Header>();
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

string filename = "";
FILE * filehandle;
int lastoffset;
void handleByte(char c,int offset,YEncHead &head,YEncPart &part) {
	if (head.name.compare(filename) != 0) {
		if (!filename.empty()) {
			fclose(filehandle);
		}
		filename = head.name;
		filehandle = fopen(filename.c_str(), "w");
		lastoffset = 0;
	}
	fwrite(&c, sizeof(char), 1, filehandle);
}

optional<KeyValue> readKeyValue(string s,int& i) {
	KeyValue result;
	optional<int> r = indexOf(s,'=',i);
	if (!r.hasValue()) { return optional<KeyValue>(); }
	result.key = s.substr(i,r.value()-i);
	i=r.value()+1;
	if ((r = indexOf(s,' ',r.value())).hasValue()) {
		result.value = s.substr(i,r.value()-i);
		i=r.value()+1;
	} else {
		result.value = s.substr(i);
		i=s.length();
	}
	return optional<KeyValue>(result);
}

void readHeaders(int s) {
	optional<Header> header;
	while((header = readheader(s)).hasValue()) {
		Header h = header.value();
		//fprintf(stderr,"%s: %s\n", h.name.c_str(),h.value.c_str());
		// TODO: Do something with headers.
	}
}

void readYBegin(int s, YEncHead &yencHead) { // Parsing ybegin
	string dataline = readline(s);
	if (!startsWith(dataline, "=ybegin ")) {
		DIE();
	}
	optional<KeyValue> okv; int i = 8;
	while ((okv = readKeyValue(dataline,i)).hasValue()) {
		KeyValue kv = okv.value();
		if (kv.key.compare("part") == 0) { yencHead.part = parseD<int>(kv.value); }
		else if (kv.key.compare("line") == 0) { yencHead.line = parseD<int>(kv.value); }
		else if (kv.key.compare("size") == 0) { yencHead.size = parseD<int>(kv.value); }
		else if (kv.key.compare("total") == 0) { yencHead.total = parseD<int>(kv.value); }
		else if (kv.key.compare("name") == 0) { yencHead.name = kv.value; /* TODO: Fix? */ }
	}
}

void readYPart(int s,YEncPart &yencPart) { // Parsing ypart
	string dataline = readline(s);
	if (!startsWith(dataline, "=ypart ")) {
		DIE();
	}
	optional<KeyValue> okv; int i = 7;
	while ((okv = readKeyValue(dataline,i)).hasValue()) {
		KeyValue kv = okv.value();
		if (kv.key.compare("begin") == 0) { yencPart.begin = parseD<int>(kv.value); }
		else if (kv.key.compare("end") == 0) { yencPart.end = parseD<int>(kv.value); }
	}
}



void readYEnc(int s, YEncHead &head, YEncPart &part) { // Parsing yEnc lines
	int partSize = part.end - (part.begin - 1);
	int offset = part.begin;
	YEncEnd yencEnd;

	unsigned long checksum = crc32_init();

	optional<char> oc;
	while((oc = readchar(s, socketbuffer)).hasValue()) {
		switch(oc.value()) {
			case '\0': { DIE(); } break;
			case '=': {

			} break;
			case '\r': break;
			case '\n': {
				const char *match = "=yend ";
				int i;
				for (i=0;i<6;i++) {
					if (!(oc = readchar(s, socketbuffer)).hasValue()) {
						DIE();
					}
					if (oc.value() != match[i]) { break; }
				}
				if (i == 6) { // "=yend " matched
					string line = readline(s);
					optional<KeyValue> okv; int i = 0;
					while ((okv = readKeyValue(line,i)).hasValue()) {
						KeyValue kv = okv.value();
						if (kv.key.compare("crc32") == 0) { yencEnd.crc32 = strtoull(kv.value.c_str(), (char **)NULL, 16); }
					}
				} else if (i > 1) {
					//handleByte(match[j] - 64 - 42, offset, head, part);
					for(int j=0;j<i;j++) {
						handleByte(match[j] - 64 - 42, offset, head, part);
					}
				}
			} break;
			default:
				handleByte(oc.value() - 42, offset, head, part);
				break;
		}
		oc = readchar(s, socketbuffer);
	}
	checksum = crc32_finish(checksum);

	// TODO: Fix crc32
	if (yencEnd.crc32 != checksum) {
		DIE();
	}
}

int main(int argc, const char **args) {
	Nzb *nzb = ParseNZB();

	socketbuffer = Buffer();
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) { DIE(); }
	
	hostent* server = gethostbyname("eu.news.bintube.com");
	if (server == NULL) {
		DIE();
	}

	struct sockaddr_in serv_addr;
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(119);

	if (connect(s,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		DIE();
	}
	if (readstatus(s) != 200) {
		DIE();
	}
	writeline(s,"AUTHINFO USER Afosick");
	if (readstatus(s) != 381) {
		DIE();
	}
	writeline(s,"AUTHINFO PASS m96xn7");
	if (readstatus(s) != 281) {
		DIE();
	}

{ // Download file-segments
	vector<File*> &files = nzb->files;
	for(int i=0;i<files.size();i++) {
		File &file = *files[i];
		string group = file.groups[0];
		writeline(s,"GROUP "+group);
		if (readstatus(s) != 211) {
			DIE();
		}

		vector<Segment*> & segments = file.segments;
		for(int j=0;j<segments.size();j++) {
			Segment &segment = *segments[j];

			writeline(s,"ARTICLE <"+segment.article+">");
			if (readstatus(s) != 220) {
				DIE();
			}

			readHeaders(s);

			YEncHead yencHead;
			readYBegin(s,yencHead);

			YEncPart yencPart;
			readYPart(s,yencPart);

			readYEnc(s,yencHead,yencPart);

			string dot = readline(s);
			if (dot != ".") {
				//printf("%s\n",dot.c_str());
				DIE();
			}
		}
	}
}
	close(s);
	return 0;
}
