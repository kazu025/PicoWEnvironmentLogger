#ifndef WIFI_STATUS_H
#define WIFI_STATUS_H

enum WifiState {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_INIT,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_GOT_IP,
    WIFI_STATE_ERROR
};

struct WifiStatus {
    WifiState state;
    char ip[16];
    int last_error;
};

void wifi_status_set(WifiState state, const char *ip, int err);
WifiStatus wifi_status_get();
void wifi_task(void *param);

#endif // WIFI_STATUS_H