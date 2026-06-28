#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#include "HttpServer.h"
#include "EnvironmentData.h"


static bool g_http_server_started = false;
static struct tcp_pcb * g_http_pcb = nullptr;

#define HTTP_BODY_SIZE      2048
#define HTTP_RESPONSE_SIZE  3072

static char g_http_body[HTTP_BODY_SIZE];
static char g_http_response[HTTP_RESPONSE_SIZE];

static void make_json_body(char *body, size_t body_size);
static void make_html_body(char *body, size_t body_size);
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t http_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static int make_http_response(char *response, size_t response_size, const char* content_type, const char* body);
static bool http_request_is_api(struct pbuf *p);


static void make_html_body(char *body, size_t body_size){
    snprintf(body, body_size,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>PicoW Logger</title>"
        "</head>"
        "<body>"
        "<h1>PicoW Environment Logger</h1>"
        "<p>Temperature: <span id=\"temperature\">--</span> &deg;C</p>"
        "<p>Humidity: <span id=\"humidity\">--</span> %%</p>"
        "<p>Pressure: <span id=\"pressure\">--</span> hPa</p>"
        "<p>Status: <span id=\"status\">starting...</span></p>"
        "<script>"
        "function updateEnv() {"
        "fetch('/api/env')"
        ".then(function(response){"
        "return response.json();"
        "})"
        ".then(function(data) {"
        "if (data.valid !== true){"
        "document.getElementById('status').textContent = 'Sensor data is not ready';"
        "return;"
        "}"
        "const temperature = Number(data.temperature);"
        "const humidity = Number(data.humidity);"
        "const pressure = Number(data.pressure);"
        "document.getElementById('temperature').textContent = temperature.toFixed(2);"
        "document.getElementById('humidity').textContent = humidity.toFixed(2);"
        "document.getElementById('pressure').textContent = pressure.toFixed(2);"
        "document.getElementById('status').textContent = 'OK';"
        "})"
        ".catch(function(error) {"
        "document.getElementById('status').textContent = 'HTTP error';"
        "});"
        "}"
        "updateEnv();"
        "setInterval(updateEnv, 5000);"
        "</script>"
        "</body>"
        "</html>");
}
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
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
    
    tcp_recved(tpcb, p->tot_len);
    
    char *body = g_http_body;
    char *response = g_http_response;
    body[0] = '\0';
    response[0] = '\0';

    if(http_request_is_api(p)){
        make_json_body(body, HTTP_BODY_SIZE);
        make_http_response(response, HTTP_RESPONSE_SIZE, "application/json; charset=UTF-8", body);
    } else {
        make_html_body(body, HTTP_BODY_SIZE);
        make_http_response(response, HTTP_RESPONSE_SIZE, "text/html; charset=UTF-8", body);
    }

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

static int make_http_response(char *response, size_t response_size, const char* content_type, const char* body){
    int n = snprintf(response, response_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        content_type,
        (unsigned)strlen(body),
        body
    );
    if(n < 0 || (size_t)n >= response_size){
        response[0] = '\0';
        return false;
    }
    return true;
}

static bool http_request_is_api(struct pbuf *p){
    char req[96] = {0};

    if(p == nullptr){
        return false;
    }

    uint16_t len = pbuf_copy_partial(p, req, sizeof(req) - 1, 0);
    req[len] = '\0';

    if(strncmp(req, "GET /api ", 9) == 0){
        return true;
    }

    if(strncmp(req, "GET /api?", 9) == 0){
        return true;
    }

    if(strncmp(req, "GET /api/env ", 13) == 0){
        return true;
    }

    if(strncmp(req, "GET /api/env?", 13) == 0){
        return true;
    }

    return false;
}

static void make_json_body(char *body, size_t body_size){
    EnvironmentData env = environment_data_get();

    if(env.valid){
        snprintf(body, body_size,
            "{"
            "\"valid\":true,"
            "\"temperature\":%.2f,"
            "\"humidity\":%.2f,"
            "\"pressure\":%.2f"
            "}",
            env.temperature,
            env.humidity,
            env.pressure
        );
    } else {
        snprintf(body, body_size,
            "{"
            "\"valid\":false"
            "}"
        );
    }
}







