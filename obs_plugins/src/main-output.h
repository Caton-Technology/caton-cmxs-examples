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

#pragma once

#include <cmxssdk/cmxssdk.h>

using namespace caton::cmxs;

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
}


// CMXS 相关头文件
void main_output_gbl_init();
void main_output_init();
void main_output_start();
void main_output_stop();
bool main_output_is_running();
void main_output_deinit();

// Global listener. All the global notification messages will be send by this listener.
class MyGlobalListener : public CMXSListener {
 public:
    void onMessage(uint32_t message,
        uint32_t param1,
        const void * param2) noexcept override {
        switch (message) {
            case CMXSMSG_ERROR:
                {
                    switch (param1) {
                        case CMXSERR_DataPortDetectFailed:
                            {
                                const CMXSDataPortDetectFailedMsgData_t * data =
                                    reinterpret_cast<const CMXSDataPortDetectFailedMsgData_t *>(param2);
                                for (uint32_t i = 0; i < data->mPortsCount; ++i) {
                                    printf("data port detect failed, expect port: %u\n", data->mPorts[i]);
                                }
                            }
                            break;
                        case CMXSERR_NoMem:
                            {
                                printf("NoMem\n");
                            }
                            break;
                        default:
                            printf("error: %d(%s)\n", param1, cmxssdk_error_str(param1));
                            break;
                    }
                }
                break;
        default:
            printf("message: %u, param1: %u\n", message, param1);
            break;
        }
    }
};

// Global instance of MyGlobalListener, used by CMXSSDK::init
extern MyGlobalListener* g_globalListener;
