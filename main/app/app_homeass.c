//
// Created by Hessian on 2023/9/29.
//

#include <sys/queue.h>
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_check.h>
#include "mqtt.h"
#include "app.h"

static const char *TAG = "APP_HOMEASS";

typedef struct ha_configured_dev_item_ {
    char dev_id[32];
    SLIST_ENTRY(ha_configured_dev_item_) next;
} ha_configured_dev_item_t;

static SLIST_HEAD(ha_configured_dev_list_, ha_configured_dev_item_) ha_configured_dev_list;

/**
 * 查找已注册设备
 * @param dev_id
 * @return
 */
static const ha_configured_dev_item_t *find_configured_dev(const char *dev_id)
{
    const ha_configured_dev_item_t *cmd = NULL;
    ha_configured_dev_item_t *it;
    size_t len = strlen(dev_id);
    SLIST_FOREACH(it, &ha_configured_dev_list, next) {
        if (strlen(it->dev_id) == len &&
            strcmp(dev_id, it->dev_id) == 0) {
            cmd = it;
            break;
        }
    }

    return cmd;
}


typedef struct {
    char *id;
    char *name;
} ha_sensor_dev_t;

typedef struct {
    ha_sensor_dev_t *dev;
    char *deviceClass;
    char *name;
    char *unit;
    char *valueName;
} ha_sensor_ent_t;

/**
 * 注册传感器实体
 * @param ent
 */
static void ha_entity_config_push(ha_sensor_ent_t *ent)
{
    char ent_id[34];
    sprintf(ent_id, "%s-%s", ent->dev->id, ent->valueName);

    char topic[64] = {0};
    sprintf(topic, "homeassistant/sensor/%s/config", ent_id);

    char *fmt = "{"
                "\"name\": \"%s\""
                ",\"device_class\": \"%s\""
                ",\"state_topic\": \"homeassistant/sensor/%s/state\""
                ",\"unit_of_measurement\": \"%s\""
                ",\"value_template\": \"{{value_json.%s}}\""
                ",\"unique_id\": \"%s\""
                ",\"device\": {\"identifiers\": [\"%s\"], \"name\": \"%s\" }"
                "}";

    char *json = calloc(1, 512);
    sprintf(json, fmt, ent->name, ent->deviceClass, ent->dev->id, ent->unit, ent->valueName, ent_id, ent->dev->id, ent->dev->name);
    mqtt_publish(topic, json);
    free(json);
}

/**
 * 注册传感器设备
 * @param dev_id
 */
static void ha_gardener_config_push(char *dev_id)
{
    ha_sensor_dev_t dev = {
            .id = dev_id,
            .name = "Gardener"
    };

    // 气温
    ha_sensor_ent_t ent = {
            .dev = &dev,
            .deviceClass = "temperature",
            .name = "气温",
            .unit = "°C",
            .valueName = "temperature"
    };
    ha_entity_config_push(&ent);

    // 空气湿度
    ent.deviceClass = "humidity";
    ent.name = "空气湿度";
    ent.unit = "%";
    ent.valueName = "humidity";
    ha_entity_config_push(&ent);

    // 土壤湿度
    ent.deviceClass = "humidity";
    ent.name = "土壤湿度";
    ent.unit = "";
    ent.valueName = "earthHumidity";
    ha_entity_config_push(&ent);

    // 光照强度
    ent.deviceClass = "illuminance";
    ent.name = "光照强度";
    ent.unit = "lx";
    ent.valueName = "illuminance";
    ha_entity_config_push(&ent);

    // 电池电量
    ent.deviceClass = "battery";
    ent.name = "电量";
    ent.unit = "%";
    ent.valueName = "battery";
    ha_entity_config_push(&ent);
}

/**
 * 推送花园传感器读数
 * @param dev_id
 * @param report
 */
void ha_gardener_value_push(char *dev_id, wt_homegw_report_data_t *report)
{
    ha_configured_dev_item_t *item;

    item = (ha_configured_dev_item_t *)find_configured_dev(dev_id);
    if (!item) {
        item = calloc(1, sizeof(ha_configured_dev_item_t));

        if (item == NULL) {
            // just break down if no memory
            ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        } else {
            strcpy(item->dev_id, dev_id);
            SLIST_INSERT_HEAD(&ha_configured_dev_list, item, next);

            ESP_LOGI(TAG, "Register new device %s", dev_id);
            ha_gardener_config_push(dev_id);
        }
    }

    char topic[64] = {0};
    sprintf(topic, "homeassistant/sensor/%s/state", dev_id);

    char *fmt = "{"
                "\"version\": %d"
                ",\"battery\": %d"
                ",\"temperature\": %.2f"
                ",\"humidity\": %.2f"
                ",\"illuminance\": %d"
                ",\"earthHumidity\": %d"
                "}";

    char *json = calloc(1, 512);
    sprintf(json, fmt, report->header.version, report->battery, report->temperature, report->humidity, report->light, report->earthHumidity);
    mqtt_publish(topic, json);
    free(json);
}