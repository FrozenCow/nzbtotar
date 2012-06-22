#include "parserarvolume.h"
#include <string>
#include <sstream>
using namespace std;

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

string RarVolume::toString() {
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