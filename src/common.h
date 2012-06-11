#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#define DIE() { fprintf(stderr, "Failed! %s:%d",__FILE__ ,__LINE__); exit(1); }

#endif