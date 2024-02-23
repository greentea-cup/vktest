#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// macro for array allocation
// change malloc to anything needed (global)
#define ARR_INPLACE_ALLOC(type, count) (type *)malloc(count * sizeof(type))
#define ARR_ALLOC(type, name, count) type *name = ARR_INPLACE_ALLOC(type, count)

// only works for static arrays
#define ARR_LEN(arr) (sizeof(arr) / sizeof(*arr))

#define MSG_ERR_START "\033[31mERROR: "
#define MSG_WARN_START "\033[33mWARNING: "
#define MSG_INFO_START "\033[32m"
#define MSG_END "\033[0m\n"
#define MSG_ERROR(x) MSG_ERR_START x MSG_END
#define MSG_WARN(x) MSG_WARN_START x MSG_END
#define MSG_INFO(x) MSG_INFO_START x MSG_END
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define MSG_ERRORF(x) MSG_ERR_START "%s: " x MSG_END
#define MSG_WARNF(x) MSG_WATN_START "%s: " x MSG_END
#define MSG_INFOF(x) MSG_INFO_START "%s: " x MSG_END
// prepend function name information
#define eprintff(...) fprintf(stderr, HEAD(__VA_ARGS__), __func__ TAIL(__VA_ARGS__))
#define HEAD(a, ...) a
#define TAIL(a, ...) , ##__VA_ARGS__

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// #define BIG_INDEX_T

#ifdef BIG_INDEX_T
typedef uint32_t VertexIdx;
#define VulkanIndexType VK_INDEX_TYPE_UINT32
#else
typedef uint16_t VertexIdx;
#define VulkanIndexType VK_INDEX_TYPE_UINT16
#endif
#endif
