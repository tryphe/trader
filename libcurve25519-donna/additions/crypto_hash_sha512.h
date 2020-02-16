#ifndef crypto_hash_sha512_H
#define crypto_hash_sha512_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern int crypto_hash_sha512(unsigned char *,const unsigned char *,uint64_t);

#ifdef __cplusplus
}
#endif

#endif
