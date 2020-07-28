
#include "freertos/FreeRTOS.h"
#include "string.h"
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include "tcpip_adapter.h"
#include "esp_camera.h"
#include "cJSON.h"
#include "watcher.h"

extern const uint8_t webinterface_html_start[] asm("_binary_webinterface_html_start");
extern const uint8_t webinterface_html_end[] asm("_binary_webinterface_html_end");

//GET: /picture
//Usage mostly for debug purposes, preferably rely on upload
esp_err_t get_picture_handler(httpd_req_t *req){
    printf("GET: /picture \n");
    camera_fb_t* fb = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if(!fb)
    {
        printf("Picture Fail");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    res = httpd_resp_set_type(req, "image/jpeg");
    if(res==ESP_OK)
        res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    if(res == ESP_OK)
    {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
    }
    if(res == ESP_OK)
    {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    printf("Picture JPG in %uKB %ums", (uint32_t)(fb_len / 1024), (uint32_t)((fr_end - fr_start) / 1000));

    return res;
};
httpd_uri_t get_picture = {
    .uri    = "/picture",
    .method = HTTP_GET,
    .handler= get_picture_handler,
    .user_ctx = NULL
};

//GET: /
//Returns a web interface in case required..
esp_err_t get_html_handler2(httpd_req_t *req){
    printf("GET: / \n");
    char* pointer = (char*)webinterface_html_start;
    httpd_resp_send(req,pointer , (webinterface_html_end - webinterface_html_start));
    return ESP_OK;
};
httpd_uri_t get_html2 = {
    .uri    = "/",
    .method = HTTP_GET,
    .handler= get_html_handler2,
    .user_ctx = NULL
};

//GET: /settings
esp_err_t get_settings_handler(httpd_req_t* req){
    printf("GET: /settings \n");

    WatcherSettings* ws = getWatcherSettings();
    WatcherServerSettings* wss = getWatcherServerSettings();

    cJSON* json = cJSON_CreateObject();

    if(ws){
        cJSON* wsJson = serializeWatcherSettings(ws);
        cJSON_AddItemToObject(json, "watcher", wsJson);
    }
    if(wss){
        cJSON* wssJson = serializeWatcherServerSettings(wss);
        cJSON_AddItemToObject(json, "server", wssJson);
    }

    char* resultJson = cJSON_Print(json);
    httpd_resp_send(req, resultJson, strlen(resultJson));
    free(resultJson);
    cJSON_Delete(json);
    return ESP_OK;
}
httpd_uri_t get_settings = {
    .uri    = "/settings",
    .method = HTTP_GET,
    .handler= get_settings_handler,
    .user_ctx = NULL
};
//POST: /settings
esp_err_t set_settings_handler(httpd_req_t* req){
    printf("POST: /settings \n");
    char body[255];
    int length = sizeof(body);
    if(req->content_len < length)
        length = req->content_len;

    size_t received = httpd_req_recv(req, &body, length);
    cJSON* json = cJSON_Parse(body);
    
    cJSON* wsets = cJSON_GetObjectItem(json, "watcher");
    cJSON* ssets = cJSON_GetObjectItem(json, "server");
    if(wsets){
        updateWatcherSettings(wsets);
    }
    if(ssets){
        updateWatcherServerSettings(ssets);
    }
    cJSON_Delete(wsets);
    cJSON_Delete(ssets);

    return ESP_OK;
}
httpd_uri_t set_settings = {
    .uri    = "/settings",
    .method = HTTP_POST,
    .handler= set_settings_handler,
    .user_ctx = NULL
};

httpd_handle_t server = NULL;

void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if(httpd_start(&server, &config) == ESP_OK){
        httpd_register_uri_handler(server, &get_html2);
        httpd_register_uri_handler(server, &get_picture);
        httpd_register_uri_handler(server, &get_settings);
        httpd_register_uri_handler(server, &set_settings);
        printf("Started WebInterface\n");
    }
    else
        printf("Didn't start WebInterface due to failure\n");
    while(1){
        //printf("WebServer Loop\n");
        vTaskDelay(5000/portTICK_RATE_MS);
    }
}

void stopWebServer(){
    if(server)
        httpd_stop(server);
}