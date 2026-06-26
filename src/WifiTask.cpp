#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#include "wifi_config.h"
#include "WifiStatus.h"
#include "HttpServer.h"

static WifiStatus g_wifi_status = {
    WIFI_STATE_IDLE,
    "0.0.0.0",
    0
};

void wifi_status_set(WifiState state, const char* ip, int err){
    taskENTER_CRITICAL();
    g_wifi_status.state = state;
    g_wifi_status.last_error = err;
    if(ip != nullptr){
        strncpy(g_wifi_status.ip, ip, sizeof(g_wifi_status.ip) - 1);
        g_wifi_status.ip[sizeof(g_wifi_status.ip) - 1] = '\0';
    }
    taskEXIT_CRITICAL();
}

WifiStatus wifi_status_get()
{
    WifiStatus copy;

    taskENTER_CRITICAL();
    copy = g_wifi_status;
    taskEXIT_CRITICAL();

    return copy;
}


static void copy_current_ip(char *buf, size_t len)
{
    if (buf == nullptr || len == 0) {
        return;
    }

    snprintf(buf, len, "0.0.0.0");

    cyw43_arch_lwip_begin();

    if (netif_default != nullptr) {
        const ip4_addr_t *ip = netif_ip4_addr(netif_default);
        if (ip != nullptr) {
            ip4addr_ntoa_r(ip, buf, len);
        }
    }

    cyw43_arch_lwip_end();
}

void wifi_task(void *param)
{
    (void)param;

    printf("WiFiTask start\n");
    wifi_status_set(WIFI_STATE_INIT, "0.0.0.0", 0);

    if (cyw43_arch_init() != 0) {
        printf("WiFi init failed\n");
        wifi_status_set(WIFI_STATE_ERROR, "0.0.0.0", -100);
        vTaskDelete(nullptr);
        return;
    }

    cyw43_arch_enable_sta_mode();

    while (true) {
        printf("WiFi connecting ...\n");
        //printf("WiFi connecting to SSID=%s\n", WIFI_SSID);
        wifi_status_set(WIFI_STATE_CONNECTING, "0.0.0.0", 0);

        int ret = cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000
        );

        if (ret == 0) {
            char ip[16];
            copy_current_ip(ip, sizeof(ip));

            printf("WiFi connected. IP=%s\n", ip);
            wifi_status_set(WIFI_STATE_GOT_IP, ip, 0);

            http_server_init();

            while (true) {
                int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

                if (link != CYW43_LINK_UP) {
                    printf("WiFi link down. status=%d\n", link);
                    wifi_status_set(WIFI_STATE_ERROR, "0.0.0.0", link);
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(10000));
            }
        } else {
            printf("WiFi connect failed. ret=%d\n", ret);
            wifi_status_set(WIFI_STATE_ERROR, "0.0.0.0", ret);

            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}