#include "pqkem_kem.h"

/* Standard includes */
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/*  Kyber-512 (ML-KEM-512-style) */
#ifndef KYBER_K
#define KYBER_K 2
#endif

/* Kyber headers */
#include "params.h"
#include "kem.h"
#include "indcpa.h"
#include "polyvec.h"
#include "poly.h"
#include "ntt.h"
#include "symmetric.h"
#include "verify.h"
#include "fips202.h"
#include "randombytes.h"

/* mbedTLS DRBG for randomness */
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

/* Global mbedTLS CTR-DRBG */
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;
static int g_drbg_ready = 0;

static void ensure_drbg(void)
{
    if (g_drbg_ready) {
        return;
    }

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    const char *pers = "pqkem_demo";
    int ret = mbedtls_ctr_drbg_seed(&g_ctr_drbg,
                                    mbedtls_entropy_func,
                                    &g_entropy,
                                    (const unsigned char *)pers,
                                    strlen(pers));
    if (ret == 0) {
        g_drbg_ready = 1;
    } else {
        g_drbg_ready = 0;
        printf("[pqkem] mbedtls_ctr_drbg_seed failed, ret=%d\r\n", ret);
    }
}

void pqkem_random_bytes(uint8_t *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }

    ensure_drbg();
    int ret = mbedtls_ctr_drbg_random(&g_ctr_drbg, buf, len);
    if (ret != 0) {
        printf("[pqkem] ctr_drbg_random failed, ret=%d\r\n", ret);
    }
}

/* Upstream Kyber code expects a global randombytes() symbol.
 * We provide it and map to the same mbedTLS CTR-DRBG.
 */
void randombytes(uint8_t *out, size_t outlen)
{
    pqkem_random_bytes(out, outlen);
}

/* Unity build: pull in Kyber512 reference implementation .c files
 * (only scalar code, no AVX2 / immintrin.h)
 */
#include "kyber/kyber512_ref/cbd.c"
#include "kyber/kyber512_ref/fips202.c"
#include "kyber/kyber512_ref/indcpa.c"
#include "kyber/kyber512_ref/kem.c"
#include "kyber/kyber512_ref/ntt.c"
#include "kyber/kyber512_ref/poly.c"
#include "kyber/kyber512_ref/polyvec.c"
#include "kyber/kyber512_ref/reduce.c"
#include "kyber/kyber512_ref/symmetric-shake.c"
#include "kyber/kyber512_ref/verify.c"

/* KEM wrapper: expose Kyber through our pqkem_* API */

bool pqkem_keygen(uint8_t *pk, uint8_t *sk)
{
    if (!pk || !sk) {
        return false;
    }

    ensure_drbg();

    int ret = crypto_kem_keypair(pk, sk);
    if (ret != 0) {
        printf("[pqkem] crypto_kem_keypair ret=%d\r\n", ret);
        return false;
    }
    return true;
}

bool pqkem_encapsulate(const uint8_t *pk,
                       uint8_t *ct,
                       uint8_t *ss)
{
    if (!pk || !ct || !ss) {
        return false;
    }

    ensure_drbg();

    int ret = crypto_kem_enc(ct, ss, pk);
    if (ret != 0) {
        printf("[pqkem] crypto_kem_enc ret=%d\r\n", ret);
        return false;
    }
    return true;
}

bool pqkem_decapsulate(const uint8_t *ct,
                       const uint8_t *sk,
                       uint8_t *ss)
{
    if (!ct || !sk || !ss) {
        return false;
    }

    ensure_drbg();

    int ret = crypto_kem_dec(ss, ct, sk);
    if (ret != 0) {
        printf("[pqkem] crypto_kem_dec ret=%d\r\n", ret);
        return false;
    }
    return true;
}
