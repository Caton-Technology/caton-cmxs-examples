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
 * This file defines the Config structure.
 * You can use CMake to generate makefile and make it.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <obs-module.h>
#include <cmxssdk/cmxs_type.h>
#include <unordered_map>

class Config {
 public:
    Config();
    static void OBSSaveCallback(obs_data_t *save_data, bool saving, void *private_data);
    static Config *Current();
    void Load();
    void Save();
    QString host;
    QString deviceId;
    QString streamName;
    QString streamKey;
    std::unordered_map<std::string, CMXSLinkDeviceType_t> mSelectedNic;
    bool isStart;      // enable streaming checked
    bool isConnected;   // cmxs connected
 private:
    static Config *_instance;
};

#endif  // CONFIG_H
