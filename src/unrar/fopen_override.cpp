#include "fopen_override.hpp"
#undef fopen
#undef ftell
#undef ftello

void *custom_fopen = NULL;
void *custom_ftello = NULL;

typedef FILE *(*FOPEN_CALLBACK)(const char *filename, const char *mode);
typedef __off_t (*FTELLO_CALLBACK)(FILE*);

FILE *_custom_fopen(const char *filename, const char *mode) {
	if (custom_fopen) {
		printf("invoke custom!\n");
		return ((FOPEN_CALLBACK)custom_fopen)(filename, mode);
	} else {
		printf("invoke default!\n");
		return fopen(filename, mode);
	}
}

__off_t _custom_ftello(FILE *__stream) {
	if (custom_ftello != NULL) {
		return ((FTELLO_CALLBACK)custom_ftello)(__stream);
	} else {
		return ftello(__stream);
	}
}

long int _custom_ftell (FILE *__stream) {
	if (custom_ftello != NULL) {
		return ((FTELLO_CALLBACK)custom_ftello)(__stream);
	} else {
		return ftell(__stream);
	}
}