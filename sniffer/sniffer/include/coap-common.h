#ifndef __COAP__COMMON_H
#define __COAP__COMMON_H  // Define URI to request data from
#ifdef COAP_URI
#undef COAP_URI  // undefine standard URI
#endif

#ifdef WITH_COAPS
#define COAP_URI "coaps://192.168.169.1/random"
#define COAPS_PSK "topSecret"
#define COAPS_ID "suas"
#else
#define COAP_URI "coap://192.168.169.1/random"
#endif

#endif