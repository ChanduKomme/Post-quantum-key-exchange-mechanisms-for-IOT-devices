COMPONENT_ADD_INCLUDEDIRS := . include kyber/kyber512_ref

# App sources
C_SOURCES += coap_minimal.c
C_SOURCES += pqkem_kem.c

# DO NOT list kyber/*.c here anymore – the unity build in pqkem_kem.c includes them itself
# (so comment/remove any previous lines like these:)
# C_SOURCES += kyber/kyber512_ref/cbd.c
# C_SOURCES += kyber/kyber512_ref/consts.c
# C_SOURCES += kyber/kyber512_ref/fips202.c
# C_SOURCES += kyber/kyber512_ref/indcpa.c
# C_SOURCES += kyber/kyber512_ref/kem.c
# C_SOURCES += kyber/kyber512_ref/ntt.c
# C_SOURCES += kyber/kyber512_ref/poly.c
# C_SOURCES += kyber/kyber512_ref/polyvec.c
# C_SOURCES += kyber/kyber512_ref/reduce.c
# C_SOURCES += kyber/kyber512_ref/rejsample.c
# C_SOURCES += kyber/kyber512_ref/symmetric-shake.c
# C_SOURCES += kyber/kyber512_ref/verify.c
# C_SOURCES += kyber/kyber512_ref/fips202x4.c
# C_SOURCES += kyber/kyber512_ref/KeccakP-1600-times4-SIMD256.c
# etc.

# Select Kyber512 / ML-KEM-512
CFLAGS += -DKYBER_K=2

