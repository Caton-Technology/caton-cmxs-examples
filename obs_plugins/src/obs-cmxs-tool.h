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
 * You can use CMake to generate makefile and make it.
 */

#ifndef OBSCMXS_TOOL_H
#define OBSCMXS_TOOL_H
#include <unordered_map>
#include <string>
#include <cmxssdk/cmxssdk.h>

#ifdef __APPLE__
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#endif
#include <cstring>
#include <cmxssdk/cmxs_type.h>

void fillStreamParam(const std::unordered_map<std::string,
                     CMXSLinkDeviceType_t>& netDevicelist,
                     CMXSStreamParam_t& streamParam);

void releaseStreamParamMemory(CMXSStreamParam_t& streamParam);
    #ifdef __APPLE__
    void getNetworkInterfacesInfo(std::unordered_map<std::string, std::string>& nicMap);
    std::string getIPAddress(const struct sockaddr* sa);
    bool getNicType(const std::string& bsdDeviceName, bool& isWifi);
    #endif
#endif
