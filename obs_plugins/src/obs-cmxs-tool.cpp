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
 * This file provides some tool functions.
 * You can use CMake to generate makefile and make it.
 */
#include "obs-cmxs-tool.h"


void fillStreamParam(const std::unordered_map<std::string,
                     CMXSLinkDeviceType_t>& netDevicelist,
                     CMXSStreamParam_t& streamParam) {
    streamParam.mNetDevicesCount = static_cast<uint32_t>(netDevicelist.size());
    if (0 == streamParam.mNetDevicesCount) {
        return;
    }
    const char** netDevice = new const char*[streamParam.mNetDevicesCount];
    CMXSLinkDeviceType_t* deviceTypes = new CMXSLinkDeviceType_t[streamParam.mNetDevicesCount];


    size_t idx = 0;
    for (const auto& entry : netDevicelist) {
        netDevice[idx] = strdup(entry.first.c_str());
        deviceTypes[idx] = entry.second;
        ++idx;
    }
    streamParam.mNetDevices = netDevice;
    streamParam.mNetDeviceTypes = (const CMXSLinkDeviceType_t *)deviceTypes;
}

void releaseStreamParamMemory(CMXSStreamParam_t& streamParam) {
    if (0 == streamParam.mNetDevicesCount) {
        return;
    }
    for (size_t i = 0; i < streamParam.mNetDevicesCount; ++i) {
        free(const_cast<char*>(streamParam.mNetDevices[i]));
    }
    delete[] streamParam.mNetDevices;
    delete[] streamParam.mNetDeviceTypes;
}


#ifdef __APPLE__
bool getNicType(const std::string& bsdDeviceName, bool& isWifi) {
    CFStringRef name_CFString = CFStringCreateWithCString(kCFAllocatorDefault,
        bsdDeviceName.c_str(), kCFStringEncodingUTF8);
    if (name_CFString == NULL) {
        return false;
    }
    CFArrayRef networkInterfaces = SCNetworkInterfaceCopyAll();
    if (networkInterfaces != nullptr) {
        CFIndex count = CFArrayGetCount(networkInterfaces);
        for (CFIndex i = 0; i < count; ++i) {
            SCNetworkInterfaceRef this_if = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(networkInterfaces, i);;
            CFStringRef this_if_name = SCNetworkInterfaceGetBSDName(this_if);
            if (this_if_name != nullptr) {
                if (!CFEqual(name_CFString, this_if_name)) {
                    continue;
                }
                CFStringRef hardwareType = SCNetworkInterfaceGetInterfaceType(this_if);
                // const char *cs = CFStringGetCStringPtr(hardwareType, kCFStringEncodingMacRoman);
                if (hardwareType != nullptr) {
                    // name of wifi device contain "Wi-Fi" or "802"
                    isWifi = (CFStringFind(hardwareType, CFSTR("Wi-Fi"), 0).location != kCFNotFound) ||
                    (CFStringFind(hardwareType, CFSTR("802"), 0).location != kCFNotFound);
                    CFRelease(networkInterfaces);
                    return true;
                }
            }
        }
        CFRelease(networkInterfaces);
    }
    // no active device found
    return false;
}

// Function to get network interfaces and their IP addresses
void getNetworkInterfacesInfo(std::unordered_map<std::string, std::string>& nicMap) {
    struct ifaddrs *ifap;

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        return;
    }

    struct ifaddrs *interface;

    for (interface = ifap; interface; interface = interface->ifa_next) {
        if (!(interface->ifa_flags & IFF_UP)) {
            continue;   // deeply nested code harder to read
        }
        std::string interfaceName(interface->ifa_name);
        if (interfaceName.compare(0, 2, "en") != 0) {
            continue;
        }
        const struct sockaddr_in *addr = (const struct sockaddr_in*)interface->ifa_addr;
        char addrBuf[ INET6_ADDRSTRLEN ];
        if (addr && (addr->sin_family == AF_INET || addr->sin_family == AF_INET6)) {
            const char *type = nullptr;
            if (addr->sin_family == AF_INET) {
                if (inet_ntop(AF_INET, &addr->sin_addr, addrBuf, INET_ADDRSTRLEN)) {
                    type = "ip4";
                }
            } else {
                const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6*)interface->ifa_addr;
                if (inet_ntop(AF_INET6, &addr6->sin6_addr, addrBuf, INET6_ADDRSTRLEN)) {
                    type = "ip6";
                }
            }
            if (type) {
                std::string ipAddress = getIPAddress(interface->ifa_addr);
                nicMap[interfaceName] = ipAddress;
            }
        }
    }

    freeifaddrs(ifap);
}


// Function to get the IP address from sockaddr structure
std::string getIPAddress(const struct sockaddr* sa) {
    char buffer[INET6_ADDRSTRLEN];
    const void* addr;
    if (sa->sa_family == AF_INET) {
        addr = &((reinterpret_cast<const struct sockaddr_in*>(sa))->sin_addr);
    } else {
        addr = &((reinterpret_cast<const struct sockaddr_in6*>(sa))->sin6_addr);
    }
    inet_ntop(sa->sa_family, addr, buffer, sizeof(buffer));
    return std::string(buffer);
}
#endif
