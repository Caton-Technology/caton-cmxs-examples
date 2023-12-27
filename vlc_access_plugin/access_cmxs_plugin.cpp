/*
 * Copyright(c) 2023, Caton Technology.
 *
 * This code is licensed under the MIT License (MIT).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * This is a simple example of showing how to use CMXSSDK on VLC.
 *
 * Compiling:
 * You can use CMakeList.txt to generate a makefile and make it.
 *
 * Install:
 * Following the VLC plugin install method.
 *
 * Running:
 * The url: // cmxs://[[server]?[device=xx&][key=xx&][data_len=xx]]
 * You can provide the some or all of server, device, key and data_len parameters by url or dialog settings.
 * This is a url example: cmxs://hello.caton.cloud?device=hello_device&key=hello_key&data_len=1234
 *
 */

#include <string.h>
#include <string>
#include <unordered_map>
#include <list>
#include <ctype.h>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define poll(u, n, t) vlc_poll(u, n, t)
#define strcasecmp _stricmp
#endif

#include <stdint.h>

#define MODULE_STRING "cmxs"
#define __PLUGIN__ 1
#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_interface.h>
#include <vlc_plugin.h>

#include <cmxssdk/cmxssdk.h>


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


namespace cmxs_plugin {

using namespace caton::cmxs;

class CxsReceiver : public CMXSListener {
 public:
    static constexpr char * SETTING_ITEM_SERVER = "server";
    static constexpr char * SETTING_ITEM_DEVICE = "device";
    static constexpr char * SETTING_ITEM_KEY = "key";
    static constexpr char * SETTING_ITEM_DATA_LEN = "data_len";

    CxsReceiver() = delete;
    explicit CxsReceiver(vlc_object_t * obj)
        : mVlcObj(obj),
        mVlcIntF(reinterpret_cast<intf_thread_t *>(obj)),
        mDataLen(0),
        mReceiver(nullptr),
        mConnected(false),
        mReserved{0} {
        cmxssdk_set_log_callback(cmxsLogCb);
        stream_t *access = reinterpret_cast<stream_t *>(obj);
        access->p_sys = nullptr;
        access->pf_read = nullptr;
        access->pf_block = block;
        access->pf_seek = nullptr;
        access->pf_control = control;
        if (!start(access)) {
            throw 0;
        }
        access->p_sys = this;
    }

    ~CxsReceiver() {
        stop();
    }

 private:
    void onMessage(uint32_t message,
        uint32_t param1,
        const void * param2) override {
        switch (message) {
            case CMXSMSG_DataReceived:
                // Here ignore this message. Receive data when VLC call the pf_block function.
                break;
            case CMXSMSG_ServerConnected:
                // CMXSSDK says server connected, now we can receive data
                mConnected = true;
                break;
            case CMXSMSG_ServerConnectFailed:
                {
                    const CMXSServerConnectFailedMsgData_t * data(
                        reinterpret_cast<const CMXSServerConnectFailedMsgData_t *>(param2));
                    // param1 is error code, data->mErrorInfo is the detailed reason.
                    // If data->mFailedNetDevicesCount is none zero,
                    //   it meas there are data->mFailedNetDevicesCount devices failed.
                    vlc_dialog_display_error(mVlcObj, "Failed to connect cmxs", data->mErrorInfo);
                    msg_Err(mVlcIntF, "Failed to connect cmxs: %u(%s)", param1, data->mErrorInfo);
                    switch (param1) {
                        default:
                            // We can check param1.
                            // For example, if param1 is CMXSERR_ServiceUnavailable,
                            // We can modify the setting on Caton Media XStream platform and then open the media again.
                            break;
                    }
                }
                break;
            case CMXSMSG_Stat:
                break;
            case CMXSMSG_Reconnecting:
                break;
            case CMXSMSG_ERROR:
                {
                    switch (param1) {
                        case CMXSERR_ServiceUnavailable:
                            // service unavailable.
                            // We can modify the setting on Caton Media XStream platform and then open the media again.
                            vlc_dialog_display_error(mVlcObj, "Service unavailable", "Please check your settings");
                            msg_Err(mVlcIntF, "Service unavailable");
                            break;
                        case CMXSERR_NoMem:
                            vlc_dialog_display_error(mVlcObj, "No mem", "No mem");
                            msg_Err(mVlcIntF, "No mem");
                            break;
                        case CMXSERR_DataServerLost:
                        case CMXSERR_DataPortDetectFailed:
                        default:
                        break;
                    }
                    break;
                }
            case CMXSMSG_WARNING:
                {
                    switch (param1) {
                        case CMXSERR_SockCannotReadEmpty:
                        case CMXSERR_Congestion:
                        case CMXSERR_NotReceiveDataInTime:
                        default:
                            break;
                    }
                }
        }
    }

    bool start(stream_t *access) {
        // parse url
        parseLocation(access->psz_location);

        // If paramter not provided by url, see if it is provided by setting dialog.
        if (!checkSetting(SETTING_ITEM_SERVER) ||
            !checkSetting(SETTING_ITEM_DEVICE) ||
            !checkSetting(SETTING_ITEM_KEY) ||
            !checkSetting(SETTING_ITEM_DATA_LEN)) {
            msg_Err(mVlcIntF, "check settings failed");
            return false;
        }

        // data_len can be the max data length of the packets in bytes.
        const std::string & dataLenStr = mSettings.at(SETTING_ITEM_DATA_LEN);
        for (uint32_t i = 0; i < dataLenStr.size(); ++i) {
            if (dataLenStr[i] > '9' || dataLenStr[i] < '0') {
                vlc_dialog_display_error(mVlcObj, "Setting error", "data len must be number");
                return false;
            }
        }
        sscanf(dataLenStr.c_str(), "%zu", &mDataLen);
        if (!mDataLen) {
            vlc_dialog_display_error(mVlcObj, "Setting error", "data len must large than 0");
            return false;
        }
        mSettings.erase(SETTING_ITEM_DATA_LEN);

        // Now, we create receiver.
        // Step 1: init the global configs.
        CMXSConfig_t cmxsCfg;
        memset(&cmxsCfg, 0, sizeof(CMXSConfig_t));
        cmxsCfg.mServer = mSettings.at(SETTING_ITEM_SERVER).c_str();
        cmxsCfg.mDeviceId = mSettings.at(SETTING_ITEM_DEVICE).c_str();
        CMXSErr err = CMXSSDK::init(&cmxsCfg);
        if (err != CMXSERR_OK) {
            vlc_dialog_display_error(mVlcObj, "Failed to init cmxs", cmxssdk_error_str(err));
            msg_Err(mVlcIntF, "Failed to init cmxs: %s", cmxssdk_error_str(err));
            return false;
        }

        // Step 2: create a receiver
        CMXSStreamParam_t streamCfg;
        memset(&streamCfg, 0, sizeof(CMXSStreamParam_t));
        streamCfg.mStreamkey = mSettings.at(SETTING_ITEM_KEY).c_str();
        mReceiver = Receiver::create(&streamCfg, this);
        if (!mReceiver) {
            vlc_dialog_display_error(mVlcObj, "Failed to create receiver", cmxssdk_error_str(err));
            msg_Err(mVlcIntF, "Failed to create receiver: %s", cmxssdk_error_str(err));
            CMXSSDK::uninit();
            return false;
        }

        // now, after CMXSMSG_ServerConnected message received, we can receive data
        return true;
    }

    void stop() {
        if (mReceiver) {
            mConnected = false;
            // destroy receiver
            Receiver::destroy(mReceiver);
            mReceiver = nullptr;
        }

        // uninit global configs.
        CMXSSDK::uninit();
    }

 private:
    static void cmxsLogCb(int level, const char * format, ...) {
        // If you want to make this log works,
        // you should modify VLC code and let vlc_vaLog(nullptr, ...) works.
        va_list var;
        va_start(var, format);
        switch (level) {
            case CMXSLOG_LEVEL_F:
            case CMXSLOG_LEVEL_E:
                vlc_vaLog(nullptr, VLC_MSG_ERR, "cmxs", "", 0, "", "%s", var);
                break;
            case CMXSLOG_LEVEL_I:
                vlc_vaLog(nullptr, VLC_MSG_INFO, "cmxs", "", 0, "", "%s", var);
                break;
            case CMXSLOG_LEVEL_W:
                vlc_vaLog(nullptr, VLC_MSG_WARN, "cmxs", "", 0, "", "%s", var);
                break;
            default:
                vlc_vaLog(nullptr, VLC_MSG_DBG, "cmxs", "", 0, "", "%s", var);
                break;
        }
        va_end(var);
    }

    static block_t * block(stream_t *access, bool *eof) {
        CxsReceiver * me = reinterpret_cast<CxsReceiver *>(access->p_sys);
        if (!me->mConnected) {
            return 0;
        }
        uint32_t dataLen = me->mDataLen;
        block_t * pkt = ::block_Alloc(dataLen);
        if (!pkt) {
            return nullptr;
        }

        CMXSErr ret = me->mReceiver->receive(pkt->p_buffer, dataLen, 100);
        switch (ret) {
            case CMXSERR_OK:
                pkt->i_buffer = dataLen;
                return pkt;
            case CMXSERR_BufferNotEnough:
                // should increase dataLen, modify the url or setting dialog's data_len.
                ::block_Release(pkt);
                vlc_dialog_display_error(me->mVlcObj,
                    "error", "too short data len(%zu), need: %zu", me->mDataLen, dataLen);
                return nullptr;
            case CMXSERR_Again:
                // It meas no data currently, try again later.
                ::block_Release(pkt);
                return nullptr;
            case CMXSERR_ServiceUnavailable:
                // service unavailable.
                // We can modify the setting on Caton Media XStream platform and then open the media again.
                vlc_dialog_display_error(me->mVlcObj, "error", "service unavailable");
                ::block_Release(pkt);
                return nullptr;
            case CMXSERR_InvalidArgs:
            case CMXSERR_NotFound:
            default:
                ::block_Release(pkt);
                vlc_dialog_display_error(me->mVlcObj, "error", cmxssdk_error_str(ret));
                return nullptr;
        }
    }

    static int control(stream_t *access, int query, va_list args) {
        CxsReceiver * me = reinterpret_cast<CxsReceiver *>(access->p_sys);
        switch (query) {
            case STREAM_CAN_SEEK:
            case STREAM_CAN_FASTSEEK:
            case STREAM_CAN_PAUSE:
            case STREAM_CAN_CONTROL_PACE:
            {
                bool *b = va_arg(args, bool *);
                *b = true;
                return VLC_SUCCESS;
            }
            case STREAM_GET_SIZE:
            {
                *va_arg(args, uint64_t *) = me->mDataLen;
                return VLC_SUCCESS;
            }
            case STREAM_GET_PTS_DELAY:
            {
                int64_t *dp = va_arg(args, int64_t *);
                *dp = DEFAULT_PTS_DELAY;
                return VLC_SUCCESS;
            }
            case STREAM_SET_PAUSE_STATE:
                return VLC_SUCCESS;
            default:
                return VLC_EGENERIC;
        }
    }

    // cmxs://[[server]?[device=xx&][key=xx&][data_len=xx]]
    void parseLocation(const char * location) {
        #define SET_SERVER_SETTING(_p) do {\
            mSettings[SETTING_ITEM_SERVER] = std::string("https://") + _p;\
        } while (0)
        msg_Info(mVlcIntF, "url: %s", location);
        if (!location || *location == '\0') {
            return;
        }

#ifdef _WIN32
        char * str = _strdup(location);
#else
        char * str = strdup(location);
#endif
        if (!str) {
            return;
        }

        char * p = str;
        while (*p != '\0') {
            if (*p != '?') {
                ++p;
                continue;
            }

            if (p == str) {
                break;
            }

            *p = '\0';
            SET_SERVER_SETTING(str);
            ++p;

            break;
        }

        if (*p == '\0' && p != str) {
            SET_SERVER_SETTING(str);
            return;
        }

        while (1) {
            const char * key = p;
            while (*p && *p != '=') {
                ++p;
            }
            if (p == key || *p != '=') {
                free(str);
                return;
            }
            *p = '\0';

            ++p;
            const char * value = p;
            while (*p && *p != '&') {
                ++p;
            }

            if (p == value) {
                free(str);
                return;
            }

            if (*p == '\0') {
                mSettings[key] = value;
                free(str);
                return;
            }
            *p = '\0';
            ++p;
            mSettings[key] = value;
        }
        #undef SET_SERVER_SETTING
    }

    bool checkSetting(const char * key) {
        std::unordered_map<std::string, std::string>::iterator i = mSettings.find(key);
        if (i != mSettings.end()) {
            return true;
        }

        char * moduleValue = var_InheritString(mVlcIntF, key);
        if (!moduleValue) {
            return false;
        }

        mSettings[key] = moduleValue;
        return true;
    }

    vlc_object_t * mVlcObj;
    intf_thread_t * mVlcIntF;

    std::unordered_map<std::string, std::string> mSettings;
    size_t mDataLen;
    Receiver * mReceiver;
    std::list<block_t *> mData;
    bool mConnected;
    uint8_t mReserved[3];
};

}  // namespace cmxs_plugin

using namespace cmxs_plugin;

#ifdef __cplusplus
extern "C" {
#endif

static int cmxsOpen(vlc_object_t *obj) {
    stream_t *access = reinterpret_cast<stream_t *>(obj);
    intf_thread_t *intf = reinterpret_cast<intf_thread_t *>(obj);
    try {
        CxsReceiver * receiver = new CxsReceiver(obj);
    } catch (...) {
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;  //
}

static void cmxsClose(vlc_object_t *obj) {
    stream_t *access = reinterpret_cast<stream_t *>(obj);
    CxsReceiver *sys = reinterpret_cast<CxsReceiver *>(access->p_sys);
    if (sys) {
        delete sys;
        access->p_sys = nullptr;
    }
}


#ifdef __cplusplus
}
#endif

vlc_module_begin();
set_shortname("cmxs");
set_description("CMXS Receive Stream");
set_category(CAT_INPUT);
set_subcategory(SUBCAT_INPUT_ACCESS);
set_capability("access", 10);
set_callbacks(cmxsOpen, cmxsClose);
add_shortcut("cmxs");
add_string(CxsReceiver::SETTING_ITEM_SERVER, "", "server", "cmxs server url provided by Caton.", false)
add_string(CxsReceiver::SETTING_ITEM_DEVICE, "", "device", "unique device id in your Caton Id.", false)
add_string(CxsReceiver::SETTING_ITEM_KEY, "", "key", "cmxs key provided by Caton.", false)
add_string(CxsReceiver::SETTING_ITEM_DATA_LEN, "1316", "data length(bytes)", "Data max length in bytes.", false)
vlc_module_end();
