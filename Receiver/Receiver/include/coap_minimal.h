#ifndef COAP_MINIMAL_H
#define COAP_MINIMAL_H

/* Standard includes */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CoAP types */
typedef enum {
    COAP_TYPE_CON = 0,
    COAP_TYPE_NON = 1,
    COAP_TYPE_ACK = 2,
    COAP_TYPE_RST = 3
} CoapType;

/* CoAP codes */
#define COAP_CODE_EMPTY    0x00
#define COAP_CODE_GET      0x01
#define COAP_CODE_POST     0x02
#define COAP_CODE_CREATED  0x41  // 2.01
#define COAP_CODE_CHANGED  0x44  // 2.04

#define COAP_MAX_URI_PATH  32

/* CoAP Message Structure */
typedef struct {
    uint8_t  ver;
    uint8_t  type;        // CoapType
    uint8_t  token_len;
    uint8_t  code;        // (class << 5) | detail
    uint16_t msg_id;
    uint8_t  token[8];

    char     uri_path[COAP_MAX_URI_PATH];
    uint8_t  uri_path_len;

    const uint8_t *payload;
    size_t   payload_len;
} CoapMessage;

/* Function prototypes */

/* Parse a CoAP message from raw UDP payload.
 * Fills CoapMessage, including uri_path (null-terminated) and payload pointer.
 */
bool coap_parse(CoapMessage *msg,
                const uint8_t *buf,
                size_t len);

/* Build a simple CoAP message with given type, code, msg_id, URI path and payload.
 * No Token is used (token_len = 0).
 * Returns length written to buf, or 0 on failure.
 */
size_t coap_build_simple(uint8_t *buf,
                         size_t maxlen,
                         uint8_t type,
                         uint8_t code,
                         uint16_t msg_id,
                         const char *uri_path,
                         const uint8_t *payload,
                         size_t payload_len);

/* Convenience wrapper for Confirmable POST with no Token. */
static inline size_t coap_build_post(uint8_t *buf,
                                     size_t maxlen,
                                     uint16_t msg_id,
                                     const char *uri_path,
                                     const uint8_t *payload,
                                     size_t payload_len)
{
    return coap_build_simple(buf, maxlen,
                             COAP_TYPE_CON,
                             COAP_CODE_POST,
                             msg_id,
                             uri_path,
                             payload,
                             payload_len);
}

#ifdef __cplusplus
}
#endif

#endif // COAP_MINIMAL_H
