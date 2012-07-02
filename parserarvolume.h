#ifndef PARSERARVOLUME_H
#define PARSERARVOLUME_H

#include "common.h"
#include <string>

#define RARTYPE_PART 1
#define RARTYPE_EXT 2
struct RarVolume {
	int nr;
	int type;
	std::string prefix;
	RarVolume() : nr(0), type(0), prefix("") { }
	RarVolume(int nr, int type) : nr(nr), type(type) {}
	RarVolume(int nr, int type, std::string prefix) : nr(nr), type(type), prefix(prefix) {}
	std::string toString();
};

optional<RarVolume> parseRarVolume(std::string filename);
std::string guessFilename(std::string subject);

#endif