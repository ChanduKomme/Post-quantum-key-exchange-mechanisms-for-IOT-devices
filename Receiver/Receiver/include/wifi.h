#pragma once

#ifndef SUAS_WIFI_H
#define SUAS_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Wi-Fi configuration */
/* Use your 2.4 GHz SSID and password here */
#define WIFI_SSID "Chandu"
#define WIFI_PW   "12345678910"

/* FreeRTOS Wi-Fi task entry */
void task_wifi(void *param);

#ifdef __cplusplus
}
#endif

#endif // SUAS_WIFI_H

