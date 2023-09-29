//
// Created by Hessian on 2023/9/28.
//

#ifndef WT_HOMEGW_WIFI_MGR_H
#define WT_HOMEGW_WIFI_MGR_H

#include <esp_err.h>
#include <wifi_provisioning/manager.h>
#include <freertos/event_groups.h>

void wifi_mgr_start(void);
void smartconfig_initialise_wifi();

#endif //WT_HOMEGW_WIFI_MGR_H
