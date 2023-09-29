//
// Created by Hessian on 2023/9/28.
//

#ifndef WT_HOMEGW_APP_H
#define WT_HOMEGW_APP_H

#include <esp_now.h>

#define ESPNOW_QUEUE_SIZE           6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum {
    WT_HOMEGW_ESPNOW_SEND_CB,
    WT_HOMEGW_ESPNOW_RECV_CB,
} wt_homegw_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} wt_homegw_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} wt_homegw_event_recv_cb_t;

typedef union {
    wt_homegw_event_send_cb_t send_cb;
    wt_homegw_event_recv_cb_t recv_cb;
} wt_homegw_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    wt_homegw_event_id_t id;
    wt_homegw_event_info_t info;
} wt_homegw_event_t;

enum {
    WT_HOMEGW_ESPNOW_DATA_BROADCAST,
    WT_HOMEGW_ESPNOW_DATA_UNICAST,
    WT_HOMEGW_ESPNOW_DATA_MAX,
};

typedef struct {
    uint8_t type;                         //Broadcast or unicast ESPNOW data.
    uint16_t crc;                         //CRC16 value of ESPNOW data.
    uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
    uint8_t payload[0];                   //Real payload of ESPNOW data.
} __attribute__((packed)) espnow_userdata_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
    bool unicast;                         //Send unicast ESPNOW data.
    bool broadcast;                       //Send broadcast ESPNOW data.
    int len;                              //Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];   //MAC address of destination device.
} wt_homegw_send_param_t;


typedef struct {
    uint8_t version;
    uint8_t battery;
    float temperature;
    float humidity;
    uint16_t light;
    uint16_t earthHumidity;
} __attribute__((packed)) wt_homegw_report_data_t;


esp_err_t app_espnow_init(void);
void app_espnow_deinit(void);

void ha_gardener_value_push(char *dev_id, wt_homegw_report_data_t *report);

#endif //WT_HOMEGW_APP_H
