#ifndef __ZEROIZE_H__
#define __ZEROIZE_H__

#include <stdlib.h>

#define ZEROIZE_STACK_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

void zeroize(unsigned char* b, size_t len);

void zeroize_stack();

#ifdef __cplusplus
}
#endif

#endif
