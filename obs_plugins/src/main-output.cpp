/*
Plugin Name obs-cmxs
Copyright (C) <2024> <Caton> <c3@catontechnology.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
/*
 * This is a simple example of showing how to use CMXSSDK on OBS.
 * This file init cmxs global parameters and the cmxs output parameters.
 * You can use CMake to generate makefile and make it.
 */

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "plugin-main.h"
#include "Config.h"
#include "main-output.h"
#include <unordered_map>
static obs_output_t *main_out = nullptr;
static bool main_output_running = false;
extern int s_g_cmxs_init;
extern const char* s_g_host;
extern const char* s_g_deviceId;
void main_output_init() {
    blog(LOG_INFO, "main_output_init");
    if (main_out)
        return;
    obs_data_t *settings = obs_data_create();
    main_out = obs_output_create("cmxs_output", "CMXS Main Output", settings, nullptr);
    obs_data_release(settings);
}
void main_output_deinit() {
    blog(LOG_INFO, "+main_output_deinit()");
    obs_output_release(main_out);
    main_out = nullptr;
    main_output_running = false;
    blog(LOG_INFO, "-main_output_deinit()");
}

void main_output_gbl_init() {
    if (s_g_cmxs_init) {
        return;
    }
    Config *conf = Config::Current();
    conf->Load();
    obs_data_t *settings = obs_output_get_settings(main_out);
    obs_data_set_string(settings, "server", conf->host.toUtf8().constData());
    obs_data_set_string(settings, "deviceId", conf->deviceId.toUtf8().constData());
    CMXSConfig_t cmxsCfg;
    cmxsCfg.mServer = obs_data_get_string(settings, "server");
    cmxsCfg.mDeviceId = obs_data_get_string(settings, "deviceId");
    if (s_g_host) {
        free((void*)s_g_host);  // NOLINT
        s_g_host = nullptr;
    }
    s_g_host = strdup(cmxsCfg.mServer);
    if (!s_g_host) {
        return;
    }
    if (s_g_deviceId) {
        free((void*)s_g_deviceId);  // NOLINT
        s_g_deviceId = nullptr;
    }
    s_g_deviceId = strdup(cmxsCfg.mDeviceId);
    if (!s_g_deviceId) {
        return;
    }
    CMXSErr err = CMXSSDK::init(&cmxsCfg);
    blog(LOG_INFO,
        "CMXSSDK::init: Init CMXS main output with param, %s, %s",
        cmxsCfg.mServer,
        cmxsCfg.mDeviceId);
    if (err != CMXSERR_OK) {
        blog(LOG_INFO, "Failed to init CMXSSDK: %u(%s)", err, cmxssdk_error_str(err));
        return;
    }
    s_g_cmxs_init = 1;
}
void main_output_start() {
    if (main_output_running || !main_out) {
        blog(LOG_INFO,
        "main_output_start: starting CMXS main output, %d, %p", main_output_running, main_out);
         return;
    }
    blog(LOG_INFO,
         "main_output_start: starting CMXS main output");
    Config *conf = Config::Current();
    conf->Load();

    obs_data_t *settings = obs_output_get_settings(main_out);
    obs_data_set_string(settings, "streamName", conf->streamName.toUtf8().constData());
    obs_data_set_string(settings, "streamKey", conf->streamKey.toUtf8().constData());

    obs_output_start(main_out);
    main_output_running = true;
}

void main_output_stop() {
    if (!main_output_running)
        return;
    obs_output_stop(main_out);
    main_output_running = false;
    blog(LOG_INFO, "main_output_stop: stopped CMXS main output");
}

bool main_output_is_running() {
    return main_output_running;
}
