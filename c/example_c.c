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

 /* This is a simple example of showing how to use CMXSSDK.
 * Compiling:
 * We can use CMakeList.txt to generate a makefile and make it.
 *
 * Running:
 * Execute it as the following:
 * For sending data:
 * ./cmxs_c_example -s hello.caton.cloud -d hello_device -k hello_key -m send
 * For receiving data:
 * ./cmxs_c_example -s hello.caton.cloud -d hello_device2 -k hello_key2 -m receive
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

#include <cmxssdk/cmxssdk_c.h>

// The server url provided by Caton MediaX Stream.
static char * s_g_server = NULL;
// The device id unique in our app. It is the same as filled on Caton MediaX Stream platform.
static char * s_g_device = NULL;
// The stream key established on Caton Media XStream platform.
static char * s_g_key = NULL;
// > 0: as sender. = 0: as receiver. < 0: unknown
static int s_g_mode = -1;

// 1: connecting
// 0: connected
// -1: connect failed
static int s_g_connecting_state = 0;

#define MAX_PACKET_SIZE (uint32_t)1316

#ifdef _WIN32
#define MY_SLEEP(_t) Sleep((_t) * 1000)
#define strcasecmp _stricmp
#define strdup _strdup
#else
#define MY_SLEEP sleep
#endif

static void my_callback(uint32_t message,
    uint32_t param1,
    const void * param2,
    void * userData) {
    switch (message) {
    case CMXSMSG_ServerConnected:
        {
            printf("connect success\n");
            s_g_connecting_state = 0;
        }
        break;
    case CMXSMSG_ServerConnectFailed:
        {
            const CMXSServerConnectFailedMsgData_t * data = (const CMXSServerConnectFailedMsgData_t *)param2;
            printf("connect failed: %u(%s)\n", param1, data->mErrorInfo);
            s_g_connecting_state = -1;
        }
    case CMXSMSG_DataReceived:
    case CMXSMSG_Stat:
    case CMXSMSG_ERROR:
    case CMXSMSG_WARNING:
    default:
        break;
    }
}

void run() {
    // Step 1: Init CMXSSDK.
    {
        CMXSConfig_t cmxsCfg;
        memset(&cmxsCfg, 0, sizeof(CMXSConfig_t));
        cmxsCfg.mServer = s_g_server;
        cmxsCfg.mDeviceId = s_g_device;
        CMXSErr err = cmxssdk_init(&cmxsCfg);
        if (err != CMXSERR_OK) {
            printf("Failed to init CMXSSDK: %u(%s)", err, cmxssdk_error_str(err));
            goto done;
        }
    }

    // Step 2: Create Sender or Reciever.
    //         We can create one or more Sender/Receiver.
    {
        CMXSStreamParam_t streamCfg;
        memset(&streamCfg, 0, sizeof(CMXSStreamParam_t));
        streamCfg.mStreamkey = s_g_key;
        s_g_connecting_state = 1;
        if (s_g_mode > 0) {
            CMXS_SENDER_HANDLE senderHandle = cmxssdk_create_sender(&streamCfg, my_callback, NULL);
            if (senderHandle == CMXSSDK_INVALID_HANDLE) {
                printf("failed to create handle\n");
                goto done;
            }

            while (s_g_connecting_state == 1) {
                MY_SLEEP(1);
            }
            if (s_g_connecting_state != 0) {
                printf("Connect failed\n");
                cmxssdk_destroy_sender(senderHandle);
                senderHandle = CMXSSDK_INVALID_HANDLE;
                goto done;
            }

            // Step 3-1: Send data.
            while (1) {
                uint8_t buf[MAX_PACKET_SIZE];
                memset(buf, 0, sizeof(buf));
                CMXSErr err = cmxssdk_sender_send(senderHandle, buf, MAX_PACKET_SIZE, 0);
                switch (err) {
                case CMXSERR_OK:
                    printf("%u bytes sent\n", MAX_PACKET_SIZE);
                    break;
                case CMXSERR_ServiceUnavailable:
                    // Service unavailable now. We can check the flow on Caton Media XStream platform.
                    printf("ServiceUnavailable\n");
                    break;
                case CMXSERR_NoMem:
                    break;
                case CMXSERR_Again:
                    break;
                case CMXSERR_InvalidArgs:
                case CMXSERR_NotFound:
                default:
                    // This is a code error. We need check our code.
                    break;
                }

                // here simply seep 1 second and send next packet.
                MY_SLEEP(1);
            }

            cmxssdk_destroy_sender(senderHandle);
            senderHandle = CMXSSDK_INVALID_HANDLE;
        } else {
            CMXS_RECEIVER_HANDLE receiverHandle = cmxssdk_create_receiver(&streamCfg, my_callback, NULL);
            if (receiverHandle == CMXSSDK_INVALID_HANDLE) {
                printf("failed to create handle\n");
                goto done;
            }

            while (s_g_connecting_state == 1) {
                MY_SLEEP(1);
            }
            if (s_g_connecting_state != 0) {
                printf("Connect failed\n");
                cmxssdk_destroy_receiver(receiverHandle);
                receiverHandle = CMXSSDK_INVALID_HANDLE;
                goto done;
            }

            // Step 3-2: Receive data.
            while (1) {
                uint8_t buf[MAX_PACKET_SIZE];
                uint32_t size = MAX_PACKET_SIZE;
                CMXSErr err = cmxssdk_receiver_receive(receiverHandle, buf, &size, 0);
                switch (err) {
                case CMXSERR_OK:
                    printf("%u bytes received\n", size);
                    break;
                case CMXSERR_ServiceUnavailable:
                    // Service unavailable now. We can check the flow on Caton Media XStream platform.
                    printf("ServiceUnavailable\n");
                    break;
                case CMXSERR_BufferNotEnough:
                    printf("buffer not enough\n");
                    // the needed buffer size is receiveSize
                    // re-allocate our buffer to enough
                    break;
                case CMXSERR_Again:
                    // Here simply sleep 1 second
                    // We can check the message C3SDKMSG_DataReceived for next cmxssdk_receiver_receive() calling
                    //   or call it periodly.
                    MY_SLEEP(1);
                    break;
                case CMXSERR_InvalidArgs:
                case CMXSERR_NotFound:
                default:
                    // This is a code error. We need check our code.
                    break;
                }
            }

            cmxssdk_destroy_receiver(receiverHandle);
            receiverHandle = CMXSSDK_INVALID_HANDLE;
        }
    }

done:
    cmxssdk_uninit();
}

static void showUsage(int argc, char *argv[]) {
    printf("Usage:\n");
    printf("%s -s -d -k -m\n", argv[0]);
    printf("  -s: The server url provided by Caton MediaX Stream.\n");
    printf("  -d: The device id unique in our app. It is the same as filled on Caton MediaX Stream platform.\n");
    printf("  -k: The stream key established on Caton Media XStream platform.\n");
    printf("  -m: send|receive. \"send\" means running as sender. \"receive\" means running as receiver.\n");
}

static void cleanParams() {
#define FREE(_p) do { if (_p) { free(_p); (_p) = NULL; } } while (0)
    FREE(s_g_server);
    FREE(s_g_device);
    FREE(s_g_key);
}

int main(int argc, char *argv[]) {
    int o;
    const char * optstring =  "s:d:k:m:";
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
            case 'm':
                if (!strcasecmp(optarg, "send")) {
                    s_g_mode = 1;
                } else if (!strcasecmp(optarg, "receive")) {
                    s_g_mode = 0;
                }
                break;
            default:
                break;
        }
    }

    if (!s_g_server || !s_g_device || !s_g_key || s_g_mode < 0) {
        showUsage(argc, argv);
        cleanParams();
        return 1;
    }

    run();
    cleanParams();

    return 0;
}
