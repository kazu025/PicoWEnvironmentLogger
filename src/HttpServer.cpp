#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#include "HttpServer.h"
#include "EnvironmentData.h"


static bool g_http_server_started = false;
static struct tcp_pcb * g_http_pcb = nullptr;

static void mk_response(char *res, size_t sz){
    EnvironmentData env = environment_data_get();
    char body[1024];
    if(env.valid){
    snprintf(body, sizeof(body),
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta http-equiv=\"refresh\" content=\"5\">"
        "<title>PicoW Logger</title></head>"
        "<body>"
        "<h1>PicoW Environment Logger</h1>"
        "<p>Temperature: %.2f &deg;C</p>"
        "<p>Humidity: %.2f %%</p>"
        "<p>Pressure: %.2f hPa</p>"
        "</body>"
        "</html>",
        env.temperature,
        env.humidity,
        env.pressure);
    } else {
    snprintf(body, sizeof(body),
        "<!DOCTYPE html>"
        "<html>"
        "<head><meta charset=\"UTF-8\"><title>PicoW Logger</title></head>"
        "<body>"
        "<h1>PicoW Environment Logger</h1>"
        "<p>Sensor data is not ready.</p>"
        "</body>"
        "</html>");
    }
    snprintf(res, sz,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        (unsigned)strlen(body),
        body
    );
}
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    char response[1024];
    (void)arg;

    if(p == nullptr){
        tcp_close(tpcb);
        return ERR_OK;
    }    
    if(err != ERR_OK){
        pbuf_free(p);
        tcp_close(tpcb);
        return err;
    }

    mk_response(response, sizeof(response));

    tcp_recved(tpcb, p->tot_len);

    err_t wr_err = tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);

    if (wr_err == ERR_OK) {
        tcp_output(tpcb);
    } else {
        printf("HTTP tcp_write failed err=%d\n", wr_err);
        pbuf_free(p);
        tcp_abort(tpcb);
        return ERR_ABRT;
    }

    pbuf_free(p);

    err_t close_err = tcp_close(tpcb);
    if (close_err != ERR_OK) {
        printf("HTTP tcp_close failed err=%d\n", close_err);
        tcp_abort(tpcb);
        return ERR_ABRT;
    }

    return ERR_OK;
}


static err_t http_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err){
    (void)arg;
    if(err != ERR_OK || newpcb == nullptr){
        return ERR_VAL;
    }
    tcp_recv(newpcb, http_recv_callback);
    return ERR_OK;
}

void http_server_init(void){
    if(g_http_server_started){
        printf("HTTP server already started\n" );
        return;
    }
    printf("HTTP server init\n");

    cyw43_arch_lwip_begin();

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if(pcb == nullptr){
        cyw43_arch_lwip_end();
        printf("HTTP tcp_new failed\n");
        return;
    }

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, 80);
    if(err != ERR_OK){
        tcp_close(pcb);
        cyw43_arch_lwip_end();
        printf("HTTP tcp_bind failed err=%d\n", err);
        return;
    }
    
    pcb = tcp_listen(pcb);
    if(pcb == nullptr){
        cyw43_arch_lwip_end();
        printf("HTTP tcp_listen failed\n");
        return;
    }

    tcp_accept(pcb, http_accept_callback);
    
    g_http_pcb = pcb;
    g_http_server_started = true;

    cyw43_arch_lwip_end();
    printf("HTTP server started on port 80\n");
}











