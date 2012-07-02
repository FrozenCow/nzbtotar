#ifndef FOPEN_OVERRIDE_HPP
#define FOPEN_OVERRIDE_HPP
#include <stdio.h>

#define fopen _custom_fopen
#define ftello _custom_ftello
#define ftell _custom_ftell

extern void *custom_fopen;
extern void *custom_ftello;

__off_t _custom_ftello(FILE *__stream);
long int _custom_ftell (FILE *__stream);
FILE *_custom_fopen(const char *filename, const char *mode);
#endif