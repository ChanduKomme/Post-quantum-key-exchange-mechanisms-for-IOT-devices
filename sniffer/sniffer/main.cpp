/*
 * Simple Wi-Fi sniffer firmware for BL602 PineCone (3rd board).
 *
 * It uses the built-in sniffer support in the Wi-Fi driver when compiled
 * with -DWITH_SNIFFER (see proj_config.mk).
 *
 * What it does:
 *   - initialize BL602
 *   - start lwIP
 *   - start Wi-Fi task from wifi.cpp (connects to WIFI_SSID / WIFI_PW)
 *   - wait for g_wifi_ready and then just idle
 *
 * All the actual sniffing is done by the Wi-Fi driver + tools/monitor/monitor.py
 * on your PC, which exposes /tmp/sniff as a capture interface for Wireshark.
 */

extern "C" {
    void vInitializeBL602(void);

    // FreeRTOS
    #include <FreeRTOS.h>
    #include <task.h>

    // C libs
    #include <stdio.h>
    #include <stdint.h>

    // lwIP
    #include <lwip/tcpip.h>
}

// Wi-Fi task + ready flag from wifi.cpp (same as your sender/gateway)
extern "C" void task_wifi(void *param);
extern "C" volatile uint8_t g_wifi_ready;

// WiFi task static storage
#define WIFI_STACK_SIZE 512
static StackType_t wifi_stack[WIFI_STACK_SIZE];
static StaticTask_t wifi_task;

// -----------------------------------------------------------------------------
// Sniffer task: just waits for Wi-Fi to be ready and then idles
// -----------------------------------------------------------------------------

static void task_sniffer(void *param)
{
    (void)param;

    printf("[sniffer] Waiting for Wi-Fi to be ready...\r\n");
    while (!g_wifi_ready) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    printf("[sniffer] Wi-Fi ready.\r\n");
    printf("[sniffer] Now run ../../tools/monitor/monitor.py on the PC\r\n");
    printf("[sniffer] and capture with Wireshark on interface /tmp/sniff.\r\n");

    // Nothing else to do here; the Wi-Fi driver streams sniffed frames
    // to the monitor tool over UART.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// -----------------------------------------------------------------------------
// app_main + bfl_main
// -----------------------------------------------------------------------------

extern "C" void app_main(void)
{
    // Initialize BL602 (UART, clocks, interrupts, etc.)
    vInitializeBL602();

    printf("\r\n=== BL602 Wi-Fi Sniffer === %s %s ===\r\n",
           __DATE__, __TIME__);

    // Start lwIP TCP/IP stack (needed for DHCP etc. in wifi.cpp)
    printf("[main] Starting TCP/IP stack\r\n");
    tcpip_init(NULL, NULL);

    // Start Wi-Fi task (from wifi.cpp; same as your gateway/sender)
    printf("[main] Starting Wi-Fi task\r\n");
    xTaskCreateStatic(
        task_wifi,
        "wifi",
        WIFI_STACK_SIZE,
        NULL,
        16,          // priority
        wifi_stack,
        &wifi_task
    );

    // Start sniffer helper task
    BaseType_t rc = xTaskCreate(
        task_sniffer,
        "sniffer",
        1024,
        NULL,
        10,
        NULL
    );
    printf("[main] sniffer xTaskCreate rc=%ld\r\n", (long)rc);

    printf("[main] Starting scheduler\r\n");
    vTaskStartScheduler();

    // Should never get here
    printf("[main] vTaskStartScheduler returned (unexpected)\r\n");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// BL602 startup entry point
extern "C" int bfl_main(void)
{
    app_main();
    return 0;
}

