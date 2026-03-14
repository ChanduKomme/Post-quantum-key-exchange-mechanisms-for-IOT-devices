#ifndef PQKEM_KEM_H
#define PQKEM_KEM_H

/* Standard includes */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ML-KEM / Kyber parameter selection */

#ifndef PQKEM_VARIANT
#define PQKEM_VARIANT 512
#endif

#if   PQKEM_VARIANT == 512
    #define PQKEM_PUBLIC_KEY_BYTES      800u
    #define PQKEM_SECRET_KEY_BYTES     1632u
    #define PQKEM_CIPHERTEXT_BYTES      768u
    #define PQKEM_SHARED_SECRET_BYTES    32u
    #define PQKEM_STRENGTH_LEVEL          1u
#elif PQKEM_VARIANT == 768
    #define PQKEM_PUBLIC_KEY_BYTES     1184u
    #define PQKEM_SECRET_KEY_BYTES     2400u
    #define PQKEM_CIPHERTEXT_BYTES     1088u
    #define PQKEM_SHARED_SECRET_BYTES    32u
    #define PQKEM_STRENGTH_LEVEL          3u
#elif PQKEM_VARIANT == 1024
    #define PQKEM_PUBLIC_KEY_BYTES     1568u
    #define PQKEM_SECRET_KEY_BYTES     3168u
    #define PQKEM_CIPHERTEXT_BYTES     1568u
    #define PQKEM_SHARED_SECRET_BYTES    32u
    #define PQKEM_STRENGTH_LEVEL          5u
#else
    #error "Unsupported PQKEM_VARIANT (must be 512, 768, or 1024)"
#endif

/* KEM API */
bool pqkem_keygen(uint8_t *pk, uint8_t *sk);

bool pqkem_encapsulate(const uint8_t *pk,
                       uint8_t *ct,
                       uint8_t *ss);

bool pqkem_decapsulate(const uint8_t *ct,
                       const uint8_t *sk,
                       uint8_t *ss);

/* DRBG-backed random generator exported for the rest of the app */
void pqkem_random_bytes(uint8_t *buf, size_t len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PQKEM_KEM_H
