extern "C" {
    #include <FreeRTOS.h>
    #include <task.h>

    #include <stdio.h>
    #include <stdint.h>
    #include <string.h>

    void vInitializeBL602(void);

    #include <lwip/sockets.h>
    #include <lwip/inet.h>
    #include <lwip/tcpip.h>

    #include <mbedtls/ccm.h>
    #include <mbedtls/hkdf.h>
    #include <mbedtls/md.h>

    #include <bl_gpio.h>

    #include <looprt.h>
    #include "suas_ssd1306.h"

    int bl_flash_read(uint32_t addr, uint8_t *dst, uint32_t len);
    int bl_flash_write(uint32_t addr, const uint8_t *src, uint32_t len);
    int bl_flash_erase(uint32_t addr, uint32_t len);

    extern int errno;
}

#include "pqkem_kem.h"
#include "coap_minimal.h"

extern "C" void task_wifi(void *param);
extern "C" volatile uint8_t g_wifi_ready;

/* CoAP codes fallback */
#ifndef COAP_CODE_BAD_REQUEST
#define COAP_CODE_BAD_REQUEST 0x80  // 4.00
#endif
#ifndef COAP_CODE_UNAUTHORIZED
#define COAP_CODE_UNAUTHORIZED 0x81 // 4.01
#endif

/* Helpers */

static void print_hex(const char *label, const uint8_t *buf, size_t len)
{
    printf("%s = ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X", buf[i]);
        if ((i + 1) % 16 == 0) printf("\r\n");
        else printf(" ");
    }
    if (len % 16 != 0) printf("\r\n");
}

#define WIFI_STACK_SIZE 512
static StackType_t wifi_stack[WIFI_STACK_SIZE];
static StaticTask_t wifi_task;

static constexpr uint8_t LED_PIN  = 5;
static constexpr uint8_t LED_ON   = 1;
static constexpr uint8_t LED_OFF  = 0;

static void led_init()
{
    bl_gpio_enable_output(LED_PIN, 0, 0);
    bl_gpio_output_set(LED_PIN, LED_OFF);
}

static void led_blink(uint32_t on_ms, uint32_t off_ms, uint8_t times)
{
    for (uint8_t i = 0; i < times; i++) {
        bl_gpio_output_set(LED_PIN, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        bl_gpio_output_set(LED_PIN, LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

static uint32_t monotonic_ms()
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}

/* AEAD (AES-CCM) */

static const size_t AEAD_KEY_LEN      = 16;  // AES-128
static const size_t AEAD_NONCE_LEN    = 12;
static const size_t AEAD_TAG_LEN      = 16;
static const size_t MAX_PLAINTEXT_LEN = 64;

struct AeadKey {
    uint8_t key[AEAD_KEY_LEN];
};

static bool aead_decrypt(const AeadKey &key,
                         const uint8_t *nonce,
                         const uint8_t *ciphertext,
                         size_t ct_len,
                         const uint8_t *tag,
                         uint8_t *plaintext)
{
    mbedtls_ccm_context ctx;
    mbedtls_ccm_init(&ctx);

    int rc = mbedtls_ccm_setkey(&ctx,
                                MBEDTLS_CIPHER_ID_AES,
                                key.key,
                                (unsigned int)(AEAD_KEY_LEN * 8));
    if (rc != 0) {
        printf("[Receiver] mbedtls_ccm_setkey failed, rc=%d\r\n", rc);
        mbedtls_ccm_free(&ctx);
        return false;
    }

    rc = mbedtls_ccm_auth_decrypt(&ctx,
                                 ct_len,
                                 nonce, AEAD_NONCE_LEN,
                                 NULL, 0,
                                 ciphertext, plaintext,
                                 tag, AEAD_TAG_LEN);

    mbedtls_ccm_free(&ctx);

    if (rc != 0) {
        printf("[Receiver] AEAD AUTH FAIL (tamper or wrong key), rc=%d\r\n", rc);
    }
    return rc == 0;
}

/* HKDF-SHA256 */

static bool hkdf_sha256(const uint8_t *ikm, size_t ikm_len,
                        const uint8_t *salt, size_t salt_len,
                        const uint8_t *info, size_t info_len,
                        uint8_t *okm, size_t okm_len)
{
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) {
        printf("[Receiver] mbedtls_md_info_from_type returned NULL\r\n");
        return false;
    }

    int rc = mbedtls_hkdf(md,
                          salt, salt_len,
                          ikm, ikm_len,
                          info, info_len,
                          okm, okm_len);
    if (rc != 0) {
        printf("[Receiver] mbedtls_hkdf failed, rc=%d\r\n", rc);
    }
    return rc == 0;
}

/* Flash storage for gateway KEM keys */

static constexpr uint32_t GW_KEM_FLASH_ADDR        = 0x001F0000u;
static constexpr uint32_t GW_KEM_FLASH_ERASE_BYTES = 8192u;
static constexpr uint32_t GW_KEM_MAGIC             = 0x474B454Du; // 'GKEM'

struct GwKemStore {
    uint32_t magic;
    uint16_t variant;
    uint8_t  strength;
    uint8_t  reserved;
    uint8_t  pk[PQKEM_PUBLIC_KEY_BYTES];
    uint8_t  sk[PQKEM_SECRET_KEY_BYTES];
};

static bool gw_keys_load(uint8_t *pk, uint8_t *sk)
{
    GwKemStore store;
    int rc = bl_flash_read(GW_KEM_FLASH_ADDR,
                           reinterpret_cast<uint8_t*>(&store),
                           (uint32_t)sizeof(store));
    if (rc != 0) return false;

    if (store.magic     != GW_KEM_MAGIC)                  return false;
    if (store.variant   != (uint16_t)PQKEM_VARIANT)       return false;
    if (store.strength  != (uint8_t)PQKEM_STRENGTH_LEVEL) return false;

    memcpy(pk, store.pk, PQKEM_PUBLIC_KEY_BYTES);
    memcpy(sk, store.sk, PQKEM_SECRET_KEY_BYTES);
    return true;
}

static bool gw_keys_save(const uint8_t *pk, const uint8_t *sk)
{
    GwKemStore store;
    memset(&store, 0, sizeof(store));

    store.magic    = GW_KEM_MAGIC;
    store.variant  = (uint16_t)PQKEM_VARIANT;
    store.strength = (uint8_t)PQKEM_STRENGTH_LEVEL;
    memcpy(store.pk, pk, PQKEM_PUBLIC_KEY_BYTES);
    memcpy(store.sk, sk, PQKEM_SECRET_KEY_BYTES);

    if (bl_flash_erase(GW_KEM_FLASH_ADDR, GW_KEM_FLASH_ERASE_BYTES) != 0) return false;
    if (bl_flash_write(GW_KEM_FLASH_ADDR,
                       reinterpret_cast<const uint8_t*>(&store),
                       (uint32_t)sizeof(store)) != 0) return false;
    return true;
}



static constexpr size_t NONCE_CACHE_SIZE = 32;

static uint8_t  g_nonce_cache[NONCE_CACHE_SIZE][AEAD_NONCE_LEN];
static uint8_t  g_nonce_cache_valid[NONCE_CACHE_SIZE];
static uint32_t g_nonce_cache_time_ms[NONCE_CACHE_SIZE];
static size_t   g_nonce_cache_next = 0;

static bool nonce_seen_before(const uint8_t *nonce)
{
    for (size_t i = 0; i < NONCE_CACHE_SIZE; i++) {
        if (!g_nonce_cache_valid[i]) continue;
        if (memcmp(g_nonce_cache[i], nonce, AEAD_NONCE_LEN) == 0) {
            return true;
        }
    }
    return false;
}

static void remember_nonce(const uint8_t *nonce)
{
    memcpy(g_nonce_cache[g_nonce_cache_next], nonce, AEAD_NONCE_LEN);
    g_nonce_cache_valid[g_nonce_cache_next] = 1;
    g_nonce_cache_time_ms[g_nonce_cache_next] = monotonic_ms();
    g_nonce_cache_next = (g_nonce_cache_next + 1) % NONCE_CACHE_SIZE;
}

/* Message framing (inside CoAP payload) */

static const uint16_t DEMO_PORT = 5683;
static const size_t   MAX_UDP   = 1024;

enum MsgType : uint8_t {
    MSG_DATA = 3
};

struct DataMsg {
    uint8_t type;
    uint8_t reserved;
    uint16_t kem_ct_len;
    uint16_t text_len;
    uint8_t nonce[AEAD_NONCE_LEN];
    uint8_t buf[PQKEM_CIPHERTEXT_BYTES + MAX_PLAINTEXT_LEN + AEAD_TAG_LEN];
} __attribute__((packed));

/* Gateway main task */

static void task_gateway(void *param)
{
    (void)param;

    printf("[Receiver] Task started (ML-KEM-%u, level %u)\r\n",
           (unsigned)PQKEM_VARIANT,
           (unsigned)PQKEM_STRENGTH_LEVEL);

    /* looprt required by SSD1306 driver */
    constexpr uint16_t LOOPRT_STACKS_SIZE = 256;
    static StackType_t proc_stack_looprt[LOOPRT_STACKS_SIZE];
    static StaticTask_t proc_task_looprt;
    looprt_start(proc_stack_looprt, LOOPRT_STACKS_SIZE, &proc_task_looprt);

    /* OLED init */
    suas_ssd1306_init();
    suas_ssd1306_clear();
    suas_ssd1306_set_cursor(0, 0);
    suas_ssd1306_print_text((char *)"Waiting...");

    /* Load/generate gateway KEM keys */
    uint8_t gw_pk[PQKEM_PUBLIC_KEY_BYTES];
    uint8_t gw_sk[PQKEM_SECRET_KEY_BYTES];

    if (gw_keys_load(gw_pk, gw_sk)) {
        printf("[Receiver] Loaded KEM keypair from flash\r\n");
    } else {
        printf("[Receiver] Generating new KEM keypair\r\n");
        if (!pqkem_keygen(gw_pk, gw_sk)) {
            printf("[Receiver] pqkem_keygen failed\r\n");
            vTaskDelete(NULL);
            return;
        }
        if (!gw_keys_save(gw_pk, gw_sk)) {
            printf("[Receiver] WARNING: failed to save KEM keys to flash\r\n");
        }
    }

    print_hex("[Receiver] PUBLIC KEY", gw_pk, PQKEM_PUBLIC_KEY_BYTES);

    /* UDP socket */
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        printf("[Receiver] socket() failed, errno=%d\r\n", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(DEMO_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[Receiver] bind() failed, errno=%d\r\n", errno);
        vTaskDelete(NULL);
        return;
    }

    printf("[Receiver] Listening on UDP port %u\r\n", (unsigned)DEMO_PORT);

    uint8_t buf[MAX_UDP];

    uint32_t auth_fail_count = 0;
    uint32_t replay_drop_count = 0;

    while (1) {
        struct sockaddr_in src;
        socklen_t srclen = sizeof(src);

        int n = recvfrom(sock_fd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&src, &srclen);
        if (n <= 0) continue;

        CoapMessage cm;
        if (!coap_parse(&cm, buf, (size_t)n)) {
            printf("[Receiver] Non-CoAP UDP packet (%d bytes)\r\n", n);
            continue;
        }

        printf("[Receiver] CoAP: code=0x%02X, uri=\"%s\", payload_len=%u\r\n",
               (unsigned)cm.code, cm.uri_path, (unsigned)cm.payload_len);

        /*  POST /pqkem-pk  => reply with public key */
      
        if (cm.code == COAP_CODE_POST && strcmp(cm.uri_path, "pqkem-pk") == 0) {
            printf("[Receiver] PK request received\r\n");

            uint8_t resp_buf[MAX_UDP];
            size_t resp_len = coap_build_simple(
                resp_buf, sizeof(resp_buf),
                COAP_TYPE_ACK,
                COAP_CODE_CHANGED,          // 2.04
                cm.msg_id,
                "pqkem-pk",
                gw_pk,
                PQKEM_PUBLIC_KEY_BYTES
            );

            sendto(sock_fd, resp_buf, resp_len, 0,
                   (struct sockaddr *)&src, srclen);
            continue;
        }

        
        /* POST /pqkem-data => protected payload */
       
        if (cm.code == COAP_CODE_POST && strcmp(cm.uri_path, "pqkem-data") == 0) {

            const size_t header_min = 1u + 1u + 2u + 2u + AEAD_NONCE_LEN;
            if (cm.payload_len < header_min) {
                printf("[Receiver] MSG_DATA payload too short (%u)\r\n",
                       (unsigned)cm.payload_len);

                uint8_t resp_buf[MAX_UDP];
                const uint8_t msg[] = "BAD_LEN";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    msg, sizeof(msg) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

            DataMsg msg;
            memset(&msg, 0, sizeof(msg));
            size_t copy_len = cm.payload_len;
            if (copy_len > sizeof(msg)) copy_len = sizeof(msg);
            memcpy(&msg, cm.payload, copy_len);

            if (msg.type != MSG_DATA) {
                printf("[Receiver] MSG_DATA wrong type=%u\r\n", msg.type);
                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "BAD_TYPE";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

            if (msg.kem_ct_len != PQKEM_CIPHERTEXT_BYTES) {
                printf("[Receiver] kem_ct_len=%u != expected %u\r\n",
                       (unsigned)msg.kem_ct_len,
                       (unsigned)PQKEM_CIPHERTEXT_BYTES);
                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "BAD_CT";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

            if (msg.text_len > MAX_PLAINTEXT_LEN) {
                printf("[Receiver] text_len=%u > MAX=%u\r\n",
                       (unsigned)msg.text_len,
                       (unsigned)MAX_PLAINTEXT_LEN);
                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "BAD_TEXTLEN";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

           
            if (nonce_seen_before(msg.nonce)) {
                replay_drop_count++;
                printf("[Receiver] REPLAY DETECTED! Dropping packet. (replay_count=%lu)\r\n",
                       (unsigned long)replay_drop_count);

                suas_ssd1306_clear();
                suas_ssd1306_set_cursor(0, 0);
                suas_ssd1306_print_text((char *)"REPLAY DROP");

                
                led_blink(200, 200, 2);

                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "REPLAY";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_UNAUTHORIZED,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

            /* Layout: buf = [KEM ct || ciphertext || tag] */
            uint8_t *kem_ct   = msg.buf;
            uint8_t *ciphertext = msg.buf + msg.kem_ct_len;
            uint8_t *tag        = ciphertext + msg.text_len;

            uint8_t ss[PQKEM_SHARED_SECRET_BYTES];
            AeadKey aead_key;

            uint32_t t0 = monotonic_ms();
            if (!pqkem_decapsulate(kem_ct, gw_sk, ss)) {
                printf("[Receiver] pqkem_decapsulate failed\r\n");
                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "KEM_FAIL";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }
            uint32_t kem_ms = monotonic_ms() - t0;

            const uint8_t info[] = "ML-KEM-AEAD";
            if (!hkdf_sha256(ss, PQKEM_SHARED_SECRET_BYTES,
                             NULL, 0,
                             info, sizeof(info) - 1,
                             aead_key.key, sizeof(aead_key.key))) {
                printf("[Receiver] HKDF failed\r\n");
                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "HKDF_FAIL";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

            uint8_t plaintext[MAX_PLAINTEXT_LEN + 1];
            uint32_t t1 = monotonic_ms();
            bool ok = aead_decrypt(aead_key,
                                   msg.nonce,
                                   ciphertext,
                                   msg.text_len,
                                   tag,
                                   plaintext);
            uint32_t aead_ms = monotonic_ms() - t1;

            if (!ok) {
                auth_fail_count++;
                printf("[Receiver] AUTH FAIL (tamper?) auth_fail_count=%lu\r\n",
                       (unsigned long)auth_fail_count);

                suas_ssd1306_clear();
                suas_ssd1306_set_cursor(0, 0);
                suas_ssd1306_print_text((char *)"AUTH FAIL");

               
                led_blink(80, 80, 3);

                uint8_t resp_buf[MAX_UDP];
                const uint8_t r[] = "AUTH_FAIL";
                size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                    COAP_TYPE_ACK,
                                                    COAP_CODE_BAD_REQUEST,
                                                    cm.msg_id,
                                                    "pqkem-data",
                                                    r, sizeof(r) - 1);
                sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);
                continue;
            }

            /* Successful decrypt */
            remember_nonce(msg.nonce);

            size_t pt_len = msg.text_len;
            if (pt_len > MAX_PLAINTEXT_LEN) pt_len = MAX_PLAINTEXT_LEN;
            plaintext[pt_len] = 0;

            printf("[Receiver] KEM=%lu ms, AEAD=%lu ms\r\n",
                   (unsigned long)kem_ms,
                   (unsigned long)aead_ms);
            printf("[Receiver] Decrypted msg: '%s'\r\n", (char *)plaintext);

            suas_ssd1306_clear();
            suas_ssd1306_set_cursor(0, 0);
            suas_ssd1306_print_text((char *)plaintext);

            
            led_blink(600, 100, 1);

         
            uint8_t resp_buf[MAX_UDP];
            const uint8_t r[] = "OK";
            size_t resp_len = coap_build_simple(resp_buf, sizeof(resp_buf),
                                                 COAP_TYPE_ACK,
                                                 COAP_CODE_CHANGED,
                                                 cm.msg_id,
                                                 "pqkem-data",
                                                 r, sizeof(r) - 1);
            sendto(sock_fd, resp_buf, resp_len, 0, (struct sockaddr *)&src, srclen);

            continue;
        }

        printf("[Receiver] Unknown CoAP request\r\n");
    }
}



static void wait_for_wifi_ready()
{
    while (!g_wifi_ready) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void task_pq_starter(void *param)
{
    (void)param;

    printf("[starter] Waiting for Wi-Fi...\r\n");
    wait_for_wifi_ready();
    printf("[starter] Wi-Fi ready, starting gateway task\r\n");

    BaseType_t rc = xTaskCreate(task_gateway, "gw", 8192, NULL, 10, NULL);
    printf("[starter] xTaskCreate rc=%ld\r\n", (long)rc);

    vTaskDelete(NULL);
}

/* app_main + bfl_main */

extern "C" void app_main(void)
{
    vInitializeBL602();
    led_init();

    printf("\r\n=== Receiver: ML-KEM-%u, level %u === %s %s ===\r\n",
           (unsigned)PQKEM_VARIANT,
           (unsigned)PQKEM_STRENGTH_LEVEL,
           __DATE__, __TIME__);

    printf("[main] Starting WiFi task\r\n");
    xTaskCreateStatic(task_wifi, "wifi", WIFI_STACK_SIZE, NULL, 16,
                      wifi_stack, &wifi_task);

    printf("[main] Starting TCP/IP stack\r\n");
    tcpip_init(NULL, NULL);

    BaseType_t rc = xTaskCreate(task_pq_starter, "pqstart", 2048, NULL, 9, NULL);
    printf("[main] starter xTaskCreate rc=%ld\r\n", (long)rc);

    printf("[main] Starting scheduler\r\n");
    vTaskStartScheduler();

    printf("[main] vTaskStartScheduler returned (unexpected)\r\n");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

extern "C" int bfl_main(void)
{
    app_main();
    return 0;
}
