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
 * This is a simple example of showing how to use CMXSSDK.
 * Compiling:
 * We can use CMakeList.txt to generate a makefile and make it.
 *
 * Running:
 * Execute it as the following:
 * For sending data:
 * ./cmxs_cpp_example -s https://hello.caton.cloud -d hello_device -k hello_key -m send
 * For receiving data:
 * ./cmxs_cpp_example -s https://hello.caton.cloud -d hello_device2 -k hello_key2 -m receive
 */

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define __GNU_LIBRARY__
#include <getopt_win.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif

#include <cmxssdk/cmxssdk.h>
#include <cmxssdk/cmxssdk_selector.h>

// The server url provided by Caton MediaX Stream.
static char * s_g_server = NULL;
// The device id unique in our app. It is the same as filled on Caton MediaX Stream platform.
static char * s_g_device = NULL;
// The stream key established on Caton Media XStream platform.
static char * s_g_key = NULL;

// 1: connecting
// 0: connected
// -1: connect failed
static int s_g_connecting_state = 0;

static constexpr uint32_t MAX_PACKET_SIZE = 1316;

using namespace caton::cmxs;

#ifdef _WIN32
#define MY_SLEEP(_t) Sleep((_t) * 1000)
#define strcasecmp _stricmp
#define strdup _strdup
#else
#define MY_SLEEP sleep
#endif

static void selector_cb(void * userData) {
    Receiver * receiver = reinterpret_cast<Receiver *>(userData);
    uint8_t buf[MAX_PACKET_SIZE];
    uint32_t size = MAX_PACKET_SIZE;
    while (1) {
        CMXSErr err = receiver->receive(buf, &size, 0);
        switch (err) {
        case CMXSERR_OK:
            printf("%u bytes received\n", size);
            break;
        case CMXSERR_Again:
            // No more data, wait select
            return;
        case CMXSERR_ServiceUnavailable:
            // Service unavailable now. We can check the flow on Caton Media XStream platform.
            printf("ServiceUnavailable\n");
            return;
        case CMXSERR_BufferNotEnough:
            printf("buffer not enough\n");
            // the needed buffer size is receiveSize
            // re-allocate our buffer to enough
            return;
        case CMXSERR_InvalidArgs:
        case CMXSERR_NotFound:
        default:
            // This is a code error. We need check our code.
            return;
        }
    }
}

// Write our CMXSSDK listener:
// All the notification messages will be send by this listener.
class MyListener : public CMXSListener {
 public:
    void onMessage(uint32_t message,
        uint32_t param1,
        const void * param2) override {
        switch (message) {
        case CMXSMSG_ServerConnected:
            {
                printf("connect success\n");
                s_g_connecting_state = 0;
            }
            break;
        case CMXSMSG_ServerConnectFailed:
            {
                const CMXSServerConnectFailedMsgData_t * data(
                    reinterpret_cast<const CMXSServerConnectFailedMsgData_t *>(param2));
                printf("connect failed: %u(%s)\n", param1, data->mErrorInfo);
                s_g_connecting_state = -1;
            }
            break;
        case CMXSMSG_Stat:
        case CMXSMSG_ERROR:
        case CMXSMSG_WARNING:
        default:
            break;
        }
    }
};

static void run() {
    MyListener * myListener = nullptr;
    CMXSSDKSelectorHandle_t selectorHandle = CMXSSDK_INVALID_SELECTOR;
    Receiver * receiver = nullptr;
    CMXSErr err = CMXSERR_OK;

    // Step 1: Init CMXSSDK.
    {
        CMXSConfig_t cmxsCfg;
        memset(&cmxsCfg, 0, sizeof(CMXSConfig_t));
        cmxsCfg.mServer = s_g_server;
        cmxsCfg.mDeviceId = s_g_device;
        CMXSErr err = CMXSSDK::init(&cmxsCfg);
        if (err != CMXSERR_OK) {
            printf("Failed to init CMXSSDK: %u(%s)", err, cmxssdk_error_str(err));
            return;
        }
    }

    // Setp 2: Create a selector
    selectorHandle = cmxssdk_selector_create();

    // Step 3: Create Reciever and add to selector.
    //         We can create one or more Receiver.
    {
        try {
            myListener = new MyListener();
        } catch (...) {
            printf("No mem\n");
            goto done;
        }

        CMXSStreamParam_t streamCfg;
        memset(&streamCfg, 0, sizeof(CMXSStreamParam_t));
        streamCfg.mStreamkey = s_g_key;

        s_g_connecting_state = 1;
        receiver = Receiver::create(&streamCfg, myListener);
        if (!receiver) {
            printf("failed to create instance\n");
            goto done;
        }

        while (s_g_connecting_state == 1) {
            MY_SLEEP(1);
        }
        if (s_g_connecting_state != 0) {
            printf("Connect failed\n");
            Receiver::destroy(receiver);
            receiver = nullptr;
            goto done;
        }

        err = cmxssdk_selector_add_receiver(selectorHandle, receiver, reinterpret_cast<void *>(receiver));
        if (err != CMXSERR_OK) {
            printf("failed to add receiver: %u\n", err);
            goto done;
        }
    }

    // Setp 4: select data
    while (1) {
        err = cmxssdk_selector_select(selectorHandle, 1000, selector_cb);
        switch (err) {
            case CMXSERR_OK:
                break;
            case CMXSERR_InvalidArgs:
            case CMXSERR_SockIO:
            default:
                printf("err: %u\n", err);
                break;
        }
    }

    cmxssdk_selector_remove_receiver(selectorHandle, reinterpret_cast<void *>(receiver));

done:
    if (selectorHandle != CMXSSDK_INVALID_SELECTOR) {
        cmxssdk_selector_destroy(selectorHandle);
    }
    if (receiver) {
        Receiver::destroy(receiver);
        receiver = nullptr;
    }
    if (myListener) {
        delete myListener;
        myListener = nullptr;
    }

    CMXSSDK::uninit();
}

static void showUsage(int argc, char *argv[]) {
    printf("Usage:\n");
    printf("%s -s -d -k\n", argv[0]);
    printf("  -s: The server url provided by Caton MediaX Stream.\n");
    printf("  -d: The device id unique in our app. It is the same as filled on Caton MediaX Stream platform.\n");
    printf("  -k: The stream key established on Caton Media XStream platform.\n");
}

static void cleanParams() {
#define FREE(_p) do { if (_p) { free(_p); (_p) = NULL; } } while (0)
    FREE(s_g_server);
    FREE(s_g_device);
    FREE(s_g_key);
}

int main(int argc, char *argv[]) {
    int o;
    const char * optstring =  "s:d:k:";
    while ((o = getopt(argc, argv, optstring)) != -1) {
        switch (o) {
            case 's':
                s_g_server = strdup(optarg);
                break;
            case 'd':
                s_g_device = strdup(optarg);
                break;
            case 'k':
                s_g_key = strdup(optarg);
                break;
            default:
                break;
        }
    }

    if (!s_g_server || !s_g_device || !s_g_key) {
        showUsage(argc, argv);
        cleanParams();
        return 1;
    }

    run();
    cleanParams();

    return 0;
}
