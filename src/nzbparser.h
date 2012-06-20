#ifndef NZBPARSER_H
#define NZBPARSER_H

#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

struct NzbSegment {
	size_t bytes;
	int number;
	string article;
};

struct NzbFile {
	string subject;
	string poster;
	vector<string> groups;
	vector<NzbSegment*> segments;
};

struct Nzb {
	vector<NzbFile*> files;
};

Nzb ParseNZB(const char *filename);

#endif