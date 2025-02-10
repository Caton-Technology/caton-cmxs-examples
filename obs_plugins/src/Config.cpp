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
 * This file creates configuration items.
 * You can use CMake to generate makefile and make it.
 */

#include "Config.h"
#include "plugin-main.h"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#define SECTION_NAME "CMXSPlugin"
#define PARAM_GLOBAL_DEVICEID "GlobalDeviceId"
#define PARAM_GLOBAL_HOST "GlobalHost"
#define PARAM_STREAM_NAME "StreamName"
#define PARAM_STREAM_KEY "StreamKey"


Config *Config::_instance = nullptr;

Config::Config() {
        isConnected = false;
        isStart = false;
}

void Config::Load() {
    config_t *obs_config = obs_frontend_get_app_config();
    if (obs_config) {
        host = config_get_string(obs_config, SECTION_NAME,
            PARAM_GLOBAL_HOST);
        deviceId = config_get_string(obs_config, SECTION_NAME,
            PARAM_GLOBAL_DEVICEID);

        streamName = config_get_string(
            obs_config, SECTION_NAME, PARAM_STREAM_NAME);

        streamKey = config_get_string(
            obs_config, SECTION_NAME, PARAM_STREAM_KEY);
    }
}

void Config::Save() {
    config_t *obs_config = obs_frontend_get_app_config();
    if (obs_config) {
        config_set_string(obs_config, SECTION_NAME,
                PARAM_GLOBAL_HOST, host.toUtf8().constData());

        config_set_string(obs_config, SECTION_NAME,
                PARAM_GLOBAL_DEVICEID,
                deviceId.toUtf8().constData());
        config_set_string(obs_config, SECTION_NAME,
                PARAM_STREAM_NAME,
                streamName.toUtf8().constData());

        config_set_string(obs_config, SECTION_NAME,
                PARAM_STREAM_KEY,
                streamKey.toUtf8().constData());

        config_save(obs_config);
    }
}

Config *Config::Current() {
    if (!_instance) {
        _instance = new Config();
    }
    return _instance;
}
