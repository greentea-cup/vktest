#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>
#include <stdio.h>

// macro for array allocation
// change malloc to anything needed (global)
#define ARR_INPLACE_ALLOC(type, count) (type *)malloc(count * sizeof(type))
#define ARR_ALLOC(type, name, count) type *name = ARR_INPLACE_ALLOC(type, count)

#define MSG_ERR_START "\033[31mERROR: "
#define MSG_INFO_START "\033[32m"
#define MSG_END "\033[0m\n"
#define MSG_ERROR(x) MSG_ERR_START x MSG_END
#define MSG_INFO(x) MSG_INFO_START x MSG_END
#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
