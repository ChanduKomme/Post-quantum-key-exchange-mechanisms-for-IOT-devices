extern "C" {
/* Standard includes */
#include <stdio.h>
#include <string.h>

/* SDK includes */
#include "bl_sys.h"
#include "bl_wifi.h"
#include "hal_sys.h"
#include "hal_wifi.h"

#include "aos/kernel.h"
#include "easyflash.h"
#include "vfs.h"
#include "wifi_mgmr_ext.h"

/* FreeRTOS includes */
#include <FreeRTOS.h>
#include <task.h>

/* LwIP includes */
#include <lwip/netifapi.h>
#include <lwip/ip4_addr.h>
#include <lwip/inet.h>

/* Global flag: set to 1 when Wi-Fi is fully up (IP address acquired) */
volatile uint8_t g_wifi_ready = 0;
}

#include "include/wifi.h"   // defines WIFI_SSID / WIFI_PW

/* Wi-Fi background configuration */

/* IMPORTANT: no (char *) cast here! */
static wifi_conf_t wifi_mgmr_conf = {
    .country_code = "CN",
    .channel_nums = 13
};

/* FreeRTOS Wi-Fi task
 * - Starts firmware
 * - Starts wifi_mgmr background
 * - Connects STA to WIFI_SSID / WIFI_PW
 * - Prints IP/GW/MASK once when DHCP completes
 * - Sets g_wifi_ready = 1 after that
 */

extern "C" void task_wifi(void *param)
{
    (void)param;

    printf("[wifi] Wi-Fi task starting\r\n");
    printf("[wifi] Target SSID: \"%s\"\r\n", WIFI_SSID);

    /* Basic init (same pattern as BL602 Wi-Fi examples) */
    bl_sys_init();
    easyflash_init();
    vfs_init();

    /* Start Wi-Fi firmware & background */
    printf("[wifi] Starting Wi-Fi firmware task\r\n");
    hal_wifi_start_firmware_task();
    vTaskDelay(pdMS_TO_TICKS(2000));   // give firmware time

    printf("[wifi] Starting Wi-Fi manager background\r\n");
    wifi_mgmr_start_background(&wifi_mgmr_conf);
    vTaskDelay(pdMS_TO_TICKS(1000));   // let background task settle

    /* Optionally enable autoconnect (nice to have) */
    wifi_mgmr_sta_autoconnect_enable();

    /* Enable STA interface and connect */
    printf("[wifi] Enabling STA interface\r\n");
    wifi_interface_t wifi_iface = wifi_mgmr_sta_enable();

    printf("[wifi] Connecting to SSID \"%s\"...\r\n", WIFI_SSID);
    int rc = wifi_mgmr_sta_connect(
        &wifi_iface,
        (char *)WIFI_SSID,   // ssid
        (char *)WIFI_PW,     // psk
        NULL,                // pmk (auto)
        NULL,                // mac / bssid (auto)
        0,                   // band (auto)
        0                    // freq (auto)
    );

    if (rc != 0) {
        printf("[wifi] wifi_mgmr_sta_connect failed, rc=%d\r\n", rc);
    }

    ip4_addr_t ip;
    ip4_addr_t gw;
    ip4_addr_t mask;
    bool printed = false;

    printf("[wifi] Waiting for DHCP IP...\r\n");

    while (1) {
        memset(&ip,   0, sizeof(ip));
        memset(&gw,   0, sizeof(gw));
        memset(&mask, 0, sizeof(mask));

        /* BL602 API: int wifi_mgmr_sta_ip_get(uint32_t *ip, uint32_t *gw, uint32_t *mask); */
        uint32_t ip_raw   = 0;
        uint32_t gw_raw   = 0;
        uint32_t mask_raw = 0;

        int rc_ip = wifi_mgmr_sta_ip_get(&ip_raw, &gw_raw, &mask_raw);
        if (rc_ip == 0) {
            ip.addr   = ip_raw;
            gw.addr   = gw_raw;
            mask.addr = mask_raw;
        } else {
            ip.addr = 0;  // not ready yet
        }

        if (!printed && ip.addr != 0) {
            printed = true;

            printf("[wifi] Connected to \"%s\"!\r\n", WIFI_SSID);
            printf("[wifi] IP  : %s\r\n", ip4addr_ntoa(&ip));
            printf("[wifi] GW  : %s\r\n", ip4addr_ntoa(&gw));
            printf("[wifi] MASK: %s\r\n", ip4addr_ntoa(&mask));

            /* Wi-Fi fully ready now – wake up PQ tasks */
            g_wifi_ready = 1;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
