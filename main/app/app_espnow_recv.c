//
// Created by Hessian on 2023/9/28.
//
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "app.h"


static const char *TAG = "APP_ESPNOW_RECV";

#define ESPNOW_MAXDELAY 512
#define MAC_STR "%02x%02x%02x%02x%02x%02x"

static QueueHandle_t s_app_espnow_queue;

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


static void app_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    wt_homegw_event_t evt;
    wt_homegw_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = WT_HOMEGW_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_app_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

/* Parse received ESPNOW data. */
int app_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, int *magic) {
    espnow_userdata_t *buf = (espnow_userdata_t *) data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(espnow_userdata_t)) {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *) buf, data_len);

    if (crc_cal == crc) {
        return buf->type;
    }

    return -1;
}

static void app_espnow_task(void *pvParameter) {
    wt_homegw_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    int recv_magic = 0;
    int ret;
    char dev_id[32] = {0};

    ESP_LOGI(TAG, "Start receiving broadcast data");

    while (xQueueReceive(s_app_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case WT_HOMEGW_ESPNOW_RECV_CB: {
                wt_homegw_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                ret = app_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                espnow_userdata_t *buf = (espnow_userdata_t *) recv_cb->data;
                wt_homegw_report_data_t report = {0};
                memcpy(&report, buf->payload, sizeof(report));
                free(recv_cb->data);

                ESP_LOGI(TAG,
                         "Report{version: %d, battery: %d, temperature: %.2f, humidity: %.2f, light: %d, earthHumidity: %d}",
                         report.version, report.battery, report.temperature, report.humidity, report.light,
                         report.earthHumidity);

                if (ret == WT_HOMEGW_ESPNOW_DATA_BROADCAST) {

                    sprintf(dev_id, "gardener"MAC_STR, MAC2STR(recv_cb->mac_addr));

                    ha_gardener_value_push(dev_id, &report);

                    ESP_LOGI(TAG, "Receive %dth broadcast data from: "MACSTR", len: %d", recv_seq,
                             MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If MAC address does not exist in peer list, add it to peer list. */
                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        if (peer == NULL) {
                            ESP_LOGE(TAG, "Malloc peer information fail");
                            app_espnow_deinit();
                            vTaskDelete(NULL);
                        }
                        memset(peer, 0, sizeof(esp_now_peer_info_t));
                        peer->channel = CONFIG_ESPNOW_CHANNEL;
                        peer->ifidx = ESP_IF_WIFI_STA;
                        peer->encrypt = true;
                        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                        memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        ESP_ERROR_CHECK(esp_now_add_peer(peer));
                        free(peer);
                    }

                } else if (ret == WT_HOMEGW_ESPNOW_DATA_UNICAST) {
                    ESP_LOGI(TAG, "Receive %dth unicast data from: "MACSTR", len: %d", recv_seq,
                             MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                } else {
                    ESP_LOGI(TAG, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                }
                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
}

esp_err_t app_espnow_init(void) {
    s_app_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(wt_homegw_event_t));
    if (s_app_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(app_espnow_recv_cb));
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *) CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(s_app_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    xTaskCreatePinnedToCore(app_espnow_task, "app_espnow_task", 4096, NULL, 4, NULL, 1);

    return ESP_OK;
}

void app_espnow_deinit(void) {
    vSemaphoreDelete(s_app_espnow_queue);
    esp_now_deinit();
}

