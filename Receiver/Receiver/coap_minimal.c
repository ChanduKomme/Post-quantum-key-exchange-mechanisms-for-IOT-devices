#include "coap_minimal.h"
#include <string.h>

#define COAP_HEADER_LEN 4


bool coap_parse(CoapMessage *msg,
                const uint8_t *buf,
                size_t len)
{
    if (!msg || !buf || len < COAP_HEADER_LEN) {
        return false;
    }

    memset(msg, 0, sizeof(*msg));

    /* Parse Header */
    uint8_t b0 = buf[0];
    msg->ver       = (b0 >> 6) & 0x03;
    msg->type      = (b0 >> 4) & 0x03;
    msg->token_len =  b0       & 0x0F;
    msg->code      = buf[1];
    msg->msg_id    = ((uint16_t)buf[2] << 8) | buf[3];

    if (msg->ver != 1 || msg->token_len > 8) {
        return false;
    }

    size_t offset = COAP_HEADER_LEN;

    if (offset + msg->token_len > len) {
        return false;
    }

    /* Parse Token */
    if (msg->token_len) {
        memcpy(msg->token, buf + offset, msg->token_len);
        offset += msg->token_len;
    }

    /* Parse options, but we only care about Uri-Path (option number 11) */
    uint16_t last_opt_num = 0;
    size_t uri_written = 0;

    while (offset < len) {
        uint8_t byte = buf[offset];
        if (byte == 0xFF) {
            /* Payload marker */
            offset++;
            break;
        }

        uint8_t delta = (byte >> 4) & 0x0F;
        uint8_t opt_len = byte & 0x0F;
        offset++;

        /* Extended delta */
        if (delta == 13) {
            if (offset >= len) return false;
            delta = 13 + buf[offset++];
        } else if (delta == 14) {
            if (offset + 1 >= len) return false;
            delta = 269 + ((uint16_t)buf[offset] << 8) + buf[offset + 1];
            offset += 2;
        } else if (delta == 15) {
            /* reserved */
            return false;
        }

        /* Extended length */
        if (opt_len == 13) {
            if (offset >= len) return false;
            opt_len = 13 + buf[offset++];
        } else if (opt_len == 14) {
            if (offset + 1 >= len) return false;
            opt_len = 269 + ((uint16_t)buf[offset] << 8) + buf[offset + 1];
            offset += 2;
        } else if (opt_len == 15) {
            /* reserved */
            return false;
        }

        uint16_t opt_num = last_opt_num + delta;
        if (offset + opt_len > len) {
            return false;
        }

        if (opt_num == 11) { // Uri-Path
            /* Append segment to uri_path, separated by '/' if needed */
            if (uri_written && uri_written < COAP_MAX_URI_PATH - 1) {
                msg->uri_path[uri_written++] = '/';
            }

            size_t copy = opt_len;
            if (uri_written + copy >= COAP_MAX_URI_PATH) {
                if (uri_written < COAP_MAX_URI_PATH) {
                    copy = (COAP_MAX_URI_PATH - 1) - uri_written;
                } else {
                    copy = 0;
                }
            }
            if (copy > 0) {
                memcpy(msg->uri_path + uri_written, buf + offset, copy);
                uri_written += copy;
            }
        }

        offset += opt_len;
        last_opt_num = opt_num;
    }

    if (uri_written < COAP_MAX_URI_PATH) {
        msg->uri_path[uri_written] = '\0';
    } else {
        msg->uri_path[COAP_MAX_URI_PATH - 1] = '\0';
    }
    msg->uri_path_len = (uint8_t)((uri_written < COAP_MAX_URI_PATH)
                                  ? uri_written
                                  : (COAP_MAX_URI_PATH - 1));

    if (offset <= len) {
        msg->payload     = buf + offset;
        msg->payload_len = len - offset;
    }

    return true;
}

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
                         size_t payload_len)
{
    if (!buf || maxlen < COAP_HEADER_LEN) {
        return 0;
    }

    size_t offset = 0;
    uint8_t token_len = 0; // no token

    /* Header: ver=1, type, TKL, code, msg_id */
    buf[offset++] = (uint8_t)((1u << 6) | ((type & 0x03u) << 4) | (token_len & 0x0Fu));
    buf[offset++] = code;
    buf[offset++] = (uint8_t)(msg_id >> 8);
    buf[offset++] = (uint8_t)(msg_id & 0xFF);

    uint16_t last_opt_num = 0;

    /* Encode Uri-Path as one or more segments */
    if (uri_path && uri_path[0] != '\0') {
        const char *segment = uri_path;
        while (*segment) {
            // Skip leading '/'
            while (*segment == '/') {
                segment++;
            }
            if (!*segment) {
                break;
            }

            const char *end = segment;
            while (*end && *end != '/') {
                end++;
            }

            uint16_t opt_num = 11; // Uri-Path
            uint16_t delta = opt_num - last_opt_num;
            uint16_t opt_len = (uint16_t)(end - segment);

            uint8_t delta_nibble;
            uint8_t len_nibble;
            uint8_t ext[4];
            size_t  ext_len = 0;

            /* Encode delta */
            if (delta < 13) {
                delta_nibble = (uint8_t)delta;
            } else if (delta < 269) {
                delta_nibble = 13;
                ext[ext_len++] = (uint8_t)(delta - 13);
            } else {
                delta_nibble = 14;
                uint16_t tmp = delta - 269;
                ext[ext_len++] = (uint8_t)(tmp >> 8);
                ext[ext_len++] = (uint8_t)(tmp & 0xFF);
            }

            /* Encode length */
            if (opt_len < 13) {
                len_nibble = (uint8_t)opt_len;
            } else if (opt_len < 269) {
                len_nibble = 13;
                ext[ext_len++] = (uint8_t)(opt_len - 13);
            } else {
                len_nibble = 14;
                uint16_t tmp = opt_len - 269;
                ext[ext_len++] = (uint8_t)(tmp >> 8);
                ext[ext_len++] = (uint8_t)(tmp & 0xFF);
            }

            if (offset + 1 + ext_len + opt_len > maxlen) {
                return 0;
            }

            buf[offset++] = (uint8_t)((delta_nibble << 4) | (len_nibble & 0x0F));
            if (ext_len) {
                memcpy(buf + offset, ext, ext_len);
                offset += ext_len;
            }

            memcpy(buf + offset, segment, opt_len);
            offset += opt_len;

            last_opt_num = opt_num;
            segment = end;
        }
    }

    /* Payload */
    if (payload && payload_len > 0) {
        if (offset + 1 + payload_len > maxlen) {
            return 0;
        }
        buf[offset++] = 0xFF; // payload marker
        memcpy(buf + offset, payload, payload_len);
        offset += payload_len;
    }

    return offset;
}
