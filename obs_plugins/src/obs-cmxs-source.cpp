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
 * This file creates a CMXS receiver, receive stream from cmxs and send it to OBS
 * You can use CMake to generate makefile and make it.
 */

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/profiler.h>
#include <util/circlebuf.h>
#include <vector>
#include <unordered_map>
#include "plugin-main.h"
#include "main-output.h"
#include "obs-cmxs-tool.h"
#include "Config.h"
#include "plugin-support.h"
#include "obs.h"
#include "forms/output-settings.h"
#include "util/config-file.h"
#include <obs-frontend-api.h>
#include "Config.h"
#include "main-output.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QLibrary>
#include <QMainWindow>
#include <QAction>
#include <QMessageBox>
#include <QString>
#include <QStringList>
#include "obs-output.h"
#include <util/dstr.h>
#include <chrono>
#include <iomanip>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#endif
#include <pthread.h>
#include <cstring>




#define PROP_KEY "cmxs_streamKey_pull"
#define PROP_NETINTERFACES "net_interfaces"
#define PROP_START_PULL "cmxs_startpulling"
#define PROP_START_INTPORT "cmxs_intport"
#define PROP_HOST "host"
#define PROP_DEVICEID "device"


// static int s_g_connecting_state = 0;
static constexpr uint32_t MAX_PACKET_SIZE = 1316;
extern int s_g_cmxs_init;

const char* s_g_host = nullptr;
const char* s_g_deviceId = nullptr;

#define OBS_FFMPEG_INTERNAL_PORT 11001
#ifdef _WINDOWS
#define MY_SLEEP(_t) Sleep((_t) * 1000)
#define usleep(_t) Sleep((_t)/1000)
#else
#define MY_SLEEP sleep
#endif

class MyListener : public CMXSListener {
 public:
    MyListener() : connecting_state(0) {}
    void onMessage(uint32_t message,
        uint32_t param1,
        const void * param2) override {
        (void)param2;
        switch (message) {
        case CMXSMSG_ServerConnected:
            {
                connecting_state = 0;
            }
            break;
        case CMXSMSG_ServerConnectFailed:
            {
                blog(LOG_INFO, "platform report connect failed%u(%s)", param1, cmxssdk_error_str(param1));
                connecting_state = -1;
            }
            break;
        case CMXSMSG_Stat:
        case CMXSMSG_ERROR:
        case CMXSMSG_WARNING:
            break;
        case CMXSMSG_StreamParamChanged:
            blog(LOG_INFO, "Received CMXSMSG_StreamParamChanged\n");
            break;
        default:
            break;
        }
    }
    int connecting_state;
};

typedef std::list<AVPacket *> packet_queue_t;
typedef struct cmxs_source {
    obs_source_t *obs_source;
    volatile bool active;
    AVFormatContext *cmxs_ffmpeg_source;
    // const char *host;
    const char *streamKey;

    Receiver *receiver;
    uint32_t internalPort;
    bool running;
    bool dataArrived;
    packet_queue_t* audioQ;
    packet_queue_t* videoQ;
    std::mutex* audio_mtx;
    std::mutex* video_mtx;
    // bool connecting;

    void* listener;
    pthread_t av_thread;
    pthread_t cmxs_thread;
    pthread_t video_thread;
    pthread_t audio_thread;
    std::unordered_map<std::string, CMXSLinkDeviceType_t>* netDeviceList;
    AVCodecContext *videoCodecContext;
    int videoStreamIndex;
    std::unordered_map<int, AVCodecContext*>* audioCodecContextMap;
    std::list<int>* audioStreamIndices;
} cmxs_source_t;

static inline enum video_format convert_pixel_format(int f) {
    switch (f) {
    case AV_PIX_FMT_NONE:
        return VIDEO_FORMAT_NONE;
    case AV_PIX_FMT_YUV420P:
        return VIDEO_FORMAT_I420;
    case AV_PIX_FMT_YUYV422:
        return VIDEO_FORMAT_YUY2;
    case AV_PIX_FMT_YUV422P:
        return VIDEO_FORMAT_I422;
    case AV_PIX_FMT_YUV422P10LE:
        return VIDEO_FORMAT_I210;
    case AV_PIX_FMT_YUV444P:
        return VIDEO_FORMAT_I444;
    case AV_PIX_FMT_YUV444P12LE:
        return VIDEO_FORMAT_I412;
    case AV_PIX_FMT_UYVY422:
        return VIDEO_FORMAT_UYVY;
    case AV_PIX_FMT_YVYU422:
        return VIDEO_FORMAT_YVYU;
    case AV_PIX_FMT_NV12:
        return VIDEO_FORMAT_NV12;
    case AV_PIX_FMT_RGBA:
        return VIDEO_FORMAT_RGBA;
    case AV_PIX_FMT_BGRA:
        return VIDEO_FORMAT_BGRA;
    case AV_PIX_FMT_YUVA420P:
        return VIDEO_FORMAT_I40A;
    case AV_PIX_FMT_YUV420P10LE:
        return VIDEO_FORMAT_I010;
    case AV_PIX_FMT_YUVA422P:
        return VIDEO_FORMAT_I42A;
    case AV_PIX_FMT_YUVA444P:
        return VIDEO_FORMAT_YUVA;
#if LIBAVUTIL_BUILD >= AV_VERSION_INT(56, 31, 100)
    case AV_PIX_FMT_YUVA444P12LE:
        return VIDEO_FORMAT_YA2L;
#endif
    case AV_PIX_FMT_BGR0:
        return VIDEO_FORMAT_BGRX;
    case AV_PIX_FMT_P010LE:
        return VIDEO_FORMAT_P010;
    default:{};
    }
    return VIDEO_FORMAT_NONE;
}

static inline enum audio_format convert_sample_format(int f) {
    switch (f) {
    case AV_SAMPLE_FMT_U8:
        return AUDIO_FORMAT_U8BIT;
    case AV_SAMPLE_FMT_S16:
        return AUDIO_FORMAT_16BIT;
    case AV_SAMPLE_FMT_S32:
        return AUDIO_FORMAT_32BIT;
    case AV_SAMPLE_FMT_FLT:
        return AUDIO_FORMAT_FLOAT;
    case AV_SAMPLE_FMT_U8P:
        return AUDIO_FORMAT_U8BIT_PLANAR;
    case AV_SAMPLE_FMT_S16P:
        return AUDIO_FORMAT_16BIT_PLANAR;
    case AV_SAMPLE_FMT_S32P:
        return AUDIO_FORMAT_32BIT_PLANAR;
    case AV_SAMPLE_FMT_FLTP:
        return AUDIO_FORMAT_FLOAT_PLANAR;
    default:{};
    }

    return AUDIO_FORMAT_UNKNOWN;
}

static inline enum video_colorspace
convert_color_space(enum AVColorSpace s, enum AVColorTransferCharacteristic trc,
            enum AVColorPrimaries color_primaries) {
    switch (s) {
    case AVCOL_SPC_BT709:
        return (trc == AVCOL_TRC_IEC61966_2_1) ? VIDEO_CS_SRGB
                               : VIDEO_CS_709;
    case AVCOL_SPC_FCC:
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_SMPTE240M:
        return VIDEO_CS_601;
    case AVCOL_SPC_BT2020_NCL:
        return (trc == AVCOL_TRC_ARIB_STD_B67) ? VIDEO_CS_2100_HLG
                               : VIDEO_CS_2100_PQ;
    default:
        return (color_primaries == AVCOL_PRI_BT2020)
                      ? ((trc == AVCOL_TRC_ARIB_STD_B67)
                      ? VIDEO_CS_2100_HLG
                      : VIDEO_CS_2100_PQ)
                   : VIDEO_CS_DEFAULT;
    }
}

static inline enum speaker_layout convert_speaker_layout(uint8_t channels) {
    switch (channels) {
    case 0:
        return SPEAKERS_UNKNOWN;
    case 1:
        return SPEAKERS_MONO;
    case 2:
        return SPEAKERS_STEREO;
    case 3:
        return SPEAKERS_2POINT1;
    case 4:
        return SPEAKERS_4POINT0;
    case 5:
        return SPEAKERS_4POINT1;
    case 6:
        return SPEAKERS_5POINT1;
    case 8:
        return SPEAKERS_7POINT1;
    default:
        return SPEAKERS_UNKNOWN;
    }
}

static inline enum video_range_type convert_color_range(enum AVColorRange r) {
    return r == AVCOL_RANGE_JPEG ? VIDEO_RANGE_FULL : VIDEO_RANGE_DEFAULT;
}

const char *cmxs_source_getname(void *) {
    return obs_module_text("CMXSPlugin.CMXSSourceName");
}

void cmxs_source_thread_stop(cmxs_source_t *s) {
    if (s->running) {
        s->running = false;
        pthread_join(s->cmxs_thread, NULL);
        pthread_join(s->av_thread, NULL);
        pthread_join(s->video_thread, NULL);
        pthread_join(s->audio_thread, NULL);
        blog(LOG_INFO, "stop pulling done");
        if (s->videoCodecContext != NULL) {
            avcodec_close(s->videoCodecContext);
            avcodec_free_context(&s->videoCodecContext);
        }
        for (auto& audioContextPair : *(s->audioCodecContextMap)) {
            avcodec_close(audioContextPair.second);
            avcodec_free_context(&audioContextPair.second);
        }
    }
}

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

void initObsData(void *data) {
    cmxs_source_t* s = reinterpret_cast<cmxs_source_t*>(data);
    s->video_mtx = new std::mutex();
    s->audio_mtx = new std::mutex();
    s->audioQ = new packet_queue_t();
    s->videoQ = new packet_queue_t();
    s->audioStreamIndices = new std::list<int>();
    s->audioCodecContextMap = new std::unordered_map<int, AVCodecContext*>;
    s->netDeviceList = new std::unordered_map<std::string, CMXSLinkDeviceType_t>();
}

void destroyObsData(void *data) {
    cmxs_source_t* s = reinterpret_cast<cmxs_source_t*>(data);

    if (s->video_mtx) {
        delete s->video_mtx;
        s->video_mtx = nullptr;
    }

    if (s->audio_mtx) {
        delete s->audio_mtx;
        s->audio_mtx = nullptr;
    }

    if (s->audioQ) {
        delete s->audioQ;
        s->audioQ = nullptr;
    }

    if (s->videoQ) {
        delete s->videoQ;
        s->videoQ = nullptr;
    }

    if (s->audioStreamIndices) {
        delete s->audioStreamIndices;
        s->audioStreamIndices = nullptr;
    }

    if (s->audioCodecContextMap) {
        for (auto& audioContextPair : *(s->audioCodecContextMap)) {
            avcodec_close(audioContextPair.second);
            avcodec_free_context(&audioContextPair.second);
        }

        // Assuming AVCodecContext* is managed elsewhere, do not delete them here
        delete s->audioCodecContextMap;
        s->audioCodecContextMap = nullptr;
    }

    if (s->netDeviceList) {
        delete s->netDeviceList;
        s->netDeviceList = nullptr;
    }
}

static void *cmxs_source_create(obs_data_t *settings, obs_source_t *source) {
    blog(LOG_INFO,
         "cmxs_source_create: starting CMXS main source");
    (void)settings;
    struct cmxs_source *stream = static_cast<struct cmxs_source *>(bzalloc(sizeof(struct cmxs_source)));
    stream->obs_source = source;
    initObsData(stream);
    #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
    #endif

    return stream;
}

static void cmxs_source_destroy(void *data) {
    struct cmxs_source *stream = static_cast<struct cmxs_source *>(data);
    cmxs_source_thread_stop(stream);
    free(const_cast<char*>(stream->streamKey));
    stream->streamKey = NULL;

    if (stream->receiver) {
        blog(LOG_INFO, "destroy receiver");
        Receiver::destroy(stream->receiver);
        stream->receiver = nullptr;
    }
    if (stream->listener) {
        blog(LOG_INFO, "destroy listener");
        MyListener* myListenerPtr = static_cast<MyListener*>(stream->listener);
        delete myListenerPtr;
        stream->listener = nullptr;
    }
    destroyObsData(stream);
    bfree(data);
}

obs_properties_t *cmxs_source_getproperties(void *) {
    obs_properties_t *props = obs_properties_create();
    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
    if (0 == s_g_cmxs_init) {
        obs_properties_add_text(props, PROP_HOST, obs_module_text("CMXSPlugin.host"), OBS_TEXT_DEFAULT);
        obs_properties_add_text(props, PROP_DEVICEID, obs_module_text("CMXSPlugin.deviceId"), OBS_TEXT_DEFAULT);
    }

    obs_properties_add_bool(
        props, PROP_START_PULL,
        obs_module_text("CMXSPlugin.CMXSSource.Start"));
    obs_properties_add_text(props, PROP_KEY, obs_module_text("CMXSPlugin.streamKey"), OBS_TEXT_DEFAULT);
    obs_properties_add_int(
        props, PROP_START_INTPORT,
        obs_module_text("CMXSPlugin.CMXSSource.Intport"), 10000, 65535, 1);
#ifdef __APPLE__
    std::unordered_map<std::string, std::string>   nics;
    getNetworkInterfacesInfo(nics);
    for (const auto& entry : nics) {
        bool nicIsWifi = false;
        if (!getNicType(entry.first, nicIsWifi)) {
            // Error, unknown Nic type
            continue;
        }

        const std::string labelStr = entry.first + ": " + entry.second;
        const std::string lableName = "netIntf_"+entry.first+"_enabled";
        obs_properties_add_bool(
        props, lableName.c_str(),
        labelStr.c_str());
        const std::string listName = entry.first+"_type";
        obs_property_t *source_list = obs_properties_add_list(
        props, listName.c_str(),
        obs_module_text("CMXSPlugin.NetinterfaceType"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

        if (nicIsWifi) {
            obs_property_list_add_int(source_list, obs_module_text("CMXSPlugin.wifi"),
                            kCMXSLinkDeviceTypeWiFi);
            obs_property_list_add_int(source_list, obs_module_text("CMXSPlugin.cable"),
                            kCMXSLinkDeviceTypeCable);
        } else {
            obs_property_list_add_int(source_list, obs_module_text("CMXSPlugin.cable"),
                            kCMXSLinkDeviceTypeCable);
            obs_property_list_add_int(source_list, obs_module_text("CMXSPlugin.wifi"),
                            kCMXSLinkDeviceTypeWiFi);
        }
        obs_property_list_add_int(source_list, obs_module_text("CMXSPlugin.cellular"),
                            kCMXSLinkDeviceTypeCellular);
        obs_property_list_add_int(source_list, obs_module_text("CMXSPlugin.unknown"),
                            kCMXSLinkDeviceTypeUnknown);
    }
#endif
    return props;
}

static int putPkt2Q(packet_queue_t *q, std::mutex *mtx, AVPacket *p) {
    AVPacket *pkt1;
    int ret = 0;

    pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(p);
        return -1;
    }
    av_packet_move_ref(pkt1, p);

    std::unique_lock<std::mutex> locker(*mtx);

    q->push_back(pkt1);

    return ret;
}


void* av_source_thread(void *data) {
    cmxs_source_t* s = reinterpret_cast<cmxs_source_t *>(data);
    char udp_url[50];
    snprintf(udp_url, sizeof(udp_url), "udp://0.0.0.0:%d", s->internalPort);
    AVDictionary* options = NULL;

    av_dict_set(&options, "overrun_nonfatal", "1", 0);
    av_dict_set(&options, "fifo_size", "278876", 0);   // 50MB
    av_dict_set(&options, "buffer_size", "5242880", 0);  // 5MB
    if (nullptr == s->cmxs_ffmpeg_source) {
        s->cmxs_ffmpeg_source = avformat_alloc_context();
    }

    while (s->running) {
        if (s->dataArrived) {
            if (avformat_open_input(&s->cmxs_ffmpeg_source, udp_url, NULL, &options) != 0) {
                blog(LOG_INFO, "Failed to open UDP input");
                continue;
            } else {
                break;
            }
        } else {
            usleep(100000);
        }
    }

    if (!s->running) {
        blog(LOG_INFO, "exit av_thread");
        return nullptr;
    }

    if (avformat_find_stream_info(s->cmxs_ffmpeg_source, NULL) < 0) {
        blog(LOG_INFO, "Failed to retrieve stream information");
        return nullptr;
    }
    s->videoStreamIndex = -1;

    for (int i = 0; i < static_cast<int>(s->cmxs_ffmpeg_source->nb_streams); i++) {
        if (s->cmxs_ffmpeg_source->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            s->videoStreamIndex = i;
            blog(LOG_INFO, "Video streamIdx is: %d", s->videoStreamIndex);
        }
        if (s->cmxs_ffmpeg_source->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            s->audioStreamIndices->push_back(i);
        }
    }
    if (s->audioStreamIndices->size() <= 0 && s->videoStreamIndex == -1) {
        blog(LOG_INFO, "No stream found in the input");
        return nullptr;
    }
    s->videoCodecContext = NULL;
    if (s->videoStreamIndex != -1) {
        s->videoCodecContext = avcodec_alloc_context3(NULL);
        if (NULL == s->videoCodecContext) {
            blog(LOG_INFO, "videoCodecContext is null");
        }
        if (0 > avcodec_parameters_to_context(s->videoCodecContext,
                                s->cmxs_ffmpeg_source->streams[s->videoStreamIndex]->codecpar)) {
            blog(LOG_INFO, "avcodec_parameters_to_context failed");
        }
        const AVCodec *videoCodec = avcodec_find_decoder(s->videoCodecContext->codec_id);
        if (NULL == videoCodec) {
            blog(LOG_INFO, "videoCodec is null");
        }
        if (0 > avcodec_open2(s->videoCodecContext, videoCodec, NULL)) {
            blog(LOG_INFO, "avcodec_open2 is null");
        }
    }
    if (s->audioStreamIndices->size() > 0) {
        for (int audioStreamIndex : *(s->audioStreamIndices)) {
            AVCodecContext *audioCodecContext = avcodec_alloc_context3(NULL);
            avcodec_parameters_to_context(audioCodecContext,
                                        s->cmxs_ffmpeg_source->streams[audioStreamIndex]->codecpar);
            const AVCodec *audioCodec = avcodec_find_decoder(audioCodecContext->codec_id);
            if (avcodec_open2(audioCodecContext, audioCodec, NULL) < 0) {
                blog(LOG_INFO, "avcodec_open2 failed for audio stream: %d", audioStreamIndex);
                continue;
            }
            (*s->audioCodecContextMap)[audioStreamIndex] = audioCodecContext;
        }
    }

    int ret = 0;
    AVPacket *packet = av_packet_alloc();
    while (s->running) {
        ret = av_read_frame(s->cmxs_ffmpeg_source, packet);
        if (!s->running) {
            break;
        }
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            usleep(10000);
            blog(LOG_INFO, "av_read_frame failed, Exit, %s, %d", errbuf, ret);
            continue;
        }
        if (packet->stream_index == s->videoStreamIndex && s->videoCodecContext != NULL) {
            putPkt2Q(s->videoQ, s->video_mtx, packet);
        } else {
            putPkt2Q(s->audioQ, s->audio_mtx, packet);
        }
        // av_packet_unref(packet);
    }
    {
        std::unique_lock<std::mutex> locker(*s->video_mtx);
        // blog(LOG_INFO,"Insert to s->audioQ");
        s->videoQ->clear();
    }

    {
        std::unique_lock<std::mutex> locker(*s->audio_mtx);
        // blog(LOG_INFO,"Insert to s->audioQ");
        s->audioQ->clear();
    }
    if (s->videoCodecContext) {
        avcodec_free_context(&s->videoCodecContext);
        s->videoCodecContext = nullptr;
    }
    for (int audioStreamIndex : *(s->audioStreamIndices)) {
    AVCodecContext *audioCodecContext = (*s->audioCodecContextMap)[audioStreamIndex];
    if (audioCodecContext) {
            avcodec_free_context(&audioCodecContext);
            s->audioCodecContextMap->erase(audioStreamIndex);
        }
    }
    av_packet_free(&packet);
    avformat_close_input(&s->cmxs_ffmpeg_source);
    av_dict_free(&options);
    avformat_free_context(s->cmxs_ffmpeg_source);
    s->cmxs_ffmpeg_source = nullptr;
    blog(LOG_INFO, "exit av_thread");
    return nullptr;
}

void *cmxs_source_thread(void *data) {
    cmxs_source_t* s = reinterpret_cast<cmxs_source_t *>(data);

    int sockfd = -1;
    struct sockaddr_in server_addr;
    uint32_t currentBufsize = MAX_PACKET_SIZE;
    uint8_t* buf = nullptr;
    uint32_t size = MAX_PACKET_SIZE;
    if (1 == inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr)) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(s->internalPort);
    }
    if (s->listener) {
        MyListener* myListenerPtr = static_cast<MyListener*>(s->listener);
        delete myListenerPtr;
        s->listener = nullptr;
        s->running = false;
        blog(LOG_INFO, "free listener");
    }
    MyListener * myListener = nullptr;

    try {
        myListener = new MyListener();
    } catch (...) {
        blog(LOG_INFO, "No mem");
        s->running = false;
        return nullptr;
    }
    s->listener = myListener;

    CMXSStreamParam_t streamCfg;
    memset(&streamCfg, 0, sizeof(CMXSStreamParam_t));
    streamCfg.mStreamkey = s->streamKey;
    streamCfg.mConnectTimeOut = 3000;
    fillStreamParam(*s->netDeviceList, streamCfg);
    s->receiver = Receiver::create(&streamCfg, myListener);
    if (!s->receiver) {
        blog(LOG_INFO, "failed to create instance");
        s->running = false;
        goto done;
    }

    while (myListener->connecting_state == 1) {
        MY_SLEEP(1);
    }
    if (myListener->connecting_state != 0) {
        blog(LOG_INFO, "Connect failed");
        s->running = false;
        Receiver::destroy(s->receiver);
        s->receiver = nullptr;
        goto done;
    }

    buf = new uint8_t[MAX_PACKET_SIZE];
    while (s->running) {
        // blog(LOG_INFO, "Enter while, %p", s->receiver);
        int bytesSent = 0;
        size = currentBufsize;
        CMXSErr err = s->receiver->receive(buf, size, 1000);
        // blog(LOG_INFO, "Enter while, receive return %d", err);
        switch (err) {
        case CMXSERR_OK:
            s->dataArrived = true;
            bytesSent = sendto(sockfd, (const char *)buf, size, 0,
                           (struct sockaddr*)&server_addr, sizeof(server_addr));
            if (bytesSent == -1) {
                blog(LOG_INFO, "Error sending data");
            }
            break;
        case CMXSERR_ServiceUnavailable:
            // Service unavailable now. We can check the flow on Caton Media XStream platform.
            blog(LOG_INFO, "ServiceUnavailable");
            break;
        case CMXSERR_BufferNotEnough:
            blog(LOG_INFO, "buffer not enough, need %d", size);
            currentBufsize = size;
            delete [] buf;
            buf = new uint8_t[size];
            // the needed buffer size is receiveSize
            // re-allocate our buffer to enough
            break;
        case CMXSERR_Again:
            // Here simply sleep 1 second
            // We can check the message C3SDKMSG_DataReceived for next receive() calling
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
    sendto(sockfd, (const char *)buf, 1, 0,
                           (struct sockaddr*)&server_addr, sizeof(server_addr));
    delete [] buf;
    blog(LOG_INFO, "exit source_thread");

    done:
    if (s->receiver) {
        Receiver::destroy(s->receiver);
        s->receiver = nullptr;
    }
    if (myListener) {
        delete myListener;
        s->listener = nullptr;
        blog(LOG_INFO, "free listener");
    }
    if (sockfd != -1) {
        #ifdef _WIN32
        closesocket(sockfd);
        #else
        close(sockfd);
        #endif
        sockfd = -1;
    }
    s->dataArrived = false;
    return nullptr;
}

void* av_video_thread(void *data) {
    cmxs_source_t* s = reinterpret_cast<cmxs_source_t *>(data);
    packet_queue_t msgQSwap;
    int ret;
    while (s->running) {
        if (!s->dataArrived) {
            usleep(10000);
            continue;
        }
        if (msgQSwap.empty()) {
            std::unique_lock<std::mutex> locker(*s->video_mtx);
            if (s->videoQ->empty()) {
                locker.unlock();
                usleep(10000);
                continue;
            }
            if (!s->running) {
                locker.unlock();
                break;
            }
            msgQSwap.swap(*(s->videoQ));
            locker.unlock();
            while (!msgQSwap.empty()) {
                AVPacket* packet = msgQSwap.front();
                ret = avcodec_send_packet(s->videoCodecContext, packet);
                if (ret < 0) {
                    blog(LOG_INFO, "Error submitting the packet to the decoder");
                }
                AVFrame *videoFrame = av_frame_alloc();
                ret = avcodec_receive_frame(s->videoCodecContext, videoFrame);
                if (ret == 0) {
                    struct obs_source_frame video = {0};
                    video.format = convert_pixel_format(videoFrame->format);
                    for (size_t i = 0; i < MAX_AV_PLANES; i++) {
                        video.data[i] = videoFrame->data[i];
                        video.linesize[i] = videoFrame->linesize[i];
                    }
                    video.width = videoFrame->width;
                    video.height = videoFrame->height;

                    video.timestamp = videoFrame->pts;
                    video_format_get_parameters(convert_color_space(
                        videoFrame->colorspace,
                        videoFrame->color_trc,
                        videoFrame->color_primaries),
                        convert_color_range(videoFrame->color_range),
                        video.color_matrix,
                        video.color_range_min,
                        video.color_range_max);
                    obs_source_output_video(s->obs_source, &video);
                    av_frame_free(&videoFrame);
                } else if (ret == AVERROR(EAGAIN)) {
                    av_frame_free(&videoFrame);
                    blog(LOG_INFO, "No video frame available, waiting for more data...");
                } else {
                    av_frame_free(&videoFrame);
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
                    blog(LOG_INFO, "Error receiving video frame: %d - %s, Exit", ret, errbuf);
                    break;
                }

                msgQSwap.pop_front();
                av_packet_unref(packet);
                av_packet_free(&packet);
            }
            usleep(10000);
        }
    }
    blog(LOG_INFO, "Exit av_video_thread");
    return nullptr;
}

void* av_audio_thread(void *data) {
    cmxs_source_t* s = reinterpret_cast<cmxs_source_t *>(data);
    packet_queue_t msgQSwap;
    int ret;
    while (s->running) {
        if (!s->dataArrived) {
            usleep(10000);
            continue;
        }
        if (msgQSwap.empty()) {
            std::unique_lock<std::mutex> locker(*s->audio_mtx);
            if (s->audioQ->empty()) {
                locker.unlock();
                usleep(10000);
                continue;
            }
            if (!s->running) {
                locker.unlock();
                break;
            }
            msgQSwap.swap(*(s->audioQ));
            locker.unlock();
            while (!msgQSwap.empty()) {
                AVPacket* packet = msgQSwap.front();
                for (int audioStreamIndex : *(s->audioStreamIndices)) {
                    if (packet->stream_index == audioStreamIndex) {
                        // Audio frame
                        auto audioContextIt = s->audioCodecContextMap->find(audioStreamIndex);
                        if (audioContextIt != s->audioCodecContextMap->end()) {
                            AVCodecContext* audioCodecContext = audioContextIt->second;
                            ret = avcodec_send_packet(audioCodecContext, packet);
                            if (ret < 0) {
                                blog(LOG_INFO, "Error submitting the packet to the decoder");
                            }
                            AVFrame *audioFrame = av_frame_alloc();
                            ret = avcodec_receive_frame(audioCodecContext, audioFrame);
                            if (ret == 0) {
                                struct obs_source_audio audio = {0};
                                for (size_t i = 0; i < MAX_AV_PLANES; i++) {
                                    audio.data[i] = audioFrame->data[i];
                                }

                                audio.samples_per_sec = audioFrame->sample_rate;
                                audio.speakers = convert_speaker_layout(audioFrame->ch_layout.nb_channels);
                                audio.format = convert_sample_format(audioFrame->format);
                                audio.frames = audioFrame->nb_samples;
                                audio.timestamp =  audioFrame->pts;
                                obs_source_output_audio(s->obs_source, &audio);
                                av_frame_free(&audioFrame);
                            } else if (ret == AVERROR(EAGAIN)) {
                                av_frame_free(&audioFrame);
                                blog(LOG_INFO, "No audio frame available, waiting for more data...");
                            } else {
                                av_frame_free(&audioFrame);
                                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                                av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
                                blog(LOG_INFO, "Error receiving audio frame: %d - %s, Exit", ret, errbuf);
                                break;
                            }
                        } else {
                            blog(LOG_INFO, "Audio stream with index %d is not found in the map", packet->stream_index);
                        }
                    }
                }
                msgQSwap.pop_front();
                av_packet_unref(packet);
                av_packet_free(&packet);
            }
            usleep(10);
        }
    }
    blog(LOG_INFO, "Exit av_audio_thread");
    return nullptr;
}

void cmxs_source_thread_start(cmxs_source_t *s) {
    s->running = true;
    s->dataArrived = false;
    pthread_create(&s->video_thread, nullptr, av_video_thread, s);
    pthread_create(&s->audio_thread, nullptr, av_audio_thread, s);
    pthread_create(&s->av_thread, nullptr, av_source_thread, s);
    pthread_create(&s->cmxs_thread, nullptr, cmxs_source_thread, s);
}

void cmxs_source_update(void *data, obs_data_t *settings) {
    if (0 == s_g_cmxs_init) {
        CMXSConfig_t cmxsCfg;
        cmxsCfg.mServer = obs_data_get_string(settings, PROP_HOST);
        cmxsCfg.mDeviceId = obs_data_get_string(settings, PROP_DEVICEID);
        QString message = obs_module_text("CMXSPlugin.CMXSSource.ConfigMissing");;
        if (strlen(cmxsCfg.mServer) < 1 || strlen(cmxsCfg.mDeviceId) < 1) {
            blog(LOG_INFO, "Please fill correct host and deviceId");
            return;
        }
        if (s_g_host) {
            free((void*)s_g_host);  // NOLINT
            s_g_host = nullptr;
        }
        s_g_host = strdup(cmxsCfg.mServer);
        if (s_g_host == NULL) {
            return;
        }

        if (s_g_deviceId) {
            free((void*)s_g_deviceId);  // NOLINT
            s_g_deviceId = nullptr;
        }

        s_g_deviceId = strdup(cmxsCfg.mDeviceId);
        if (s_g_deviceId == NULL) {
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
    cmxs_source_t * s = reinterpret_cast<cmxs_source_t *>(data);
    auto obs_source = s->obs_source;
    auto name = obs_source_get_name(obs_source);
    blog(LOG_INFO, "[obs-cmxs] +cmxs_source_update('%s'...)", name);

    const char* streamKey = obs_data_get_string(settings, PROP_KEY);
    size_t streamKeyLength = strlen(streamKey);
    s->streamKey = reinterpret_cast<char*>(malloc((streamKeyLength + 1) * sizeof(char)));
    snprintf(const_cast<char*>(s->streamKey), streamKeyLength+1,
                                                            "%s", const_cast<char*>(streamKey));

    s->internalPort = obs_data_get_int(settings, PROP_START_INTPORT);
    #ifdef __APPLE__
    std::unordered_map<std::string, std::string>   nics;
    getNetworkInterfacesInfo(nics);
    for (const auto& entry : nics) {
        bool nicIsWifi = false;
        if (!getNicType(entry.first, nicIsWifi)) {
            continue;
        }
        const std::string labelStr = entry.first;
        const std::string lableName = "netIntf_"+labelStr+"_enabled";
        const std::string listName = labelStr+"_type";
        blog(LOG_INFO, "Get config for: %s", labelStr.c_str());
        bool enabled = obs_data_get_bool(settings, lableName.c_str());
        if (enabled) {
            int value = obs_data_get_int(settings, listName.c_str());
            blog(LOG_INFO, "%s, Device : %s ebabled, devicetype is: %d", listName.c_str(), lableName.c_str(), value);
            CMXSLinkDeviceType_t linkdevicetype = \
                            static_cast<CMXSLinkDeviceType_t>(obs_data_get_int(settings, listName.c_str()));
            if (s->netDeviceList->find(entry.first) == s->netDeviceList->end()) {
                s->netDeviceList->insert({labelStr, linkdevicetype});
            } else {
                (*s->netDeviceList)[labelStr] = linkdevicetype;
            }
        }
    }
    #endif

    bool startPulling = obs_data_get_bool(settings, PROP_START_PULL);
    if (startPulling) {
        if (!s->running) {
            cmxs_source_thread_start(s);
        }
    } else {
        blog(LOG_INFO, "try to stop pulling");
        cmxs_source_thread_stop(s);
    }

    blog(LOG_INFO, "[obs-cmxs] -cmxs_source_update('%s'...)", name);
}

void cmxs_source_shown(void *data) {
    auto s = (cmxs_source_t *)data;  // NOLINT
    auto name = obs_source_get_name(s->obs_source);
    blog(LOG_INFO, "[obs-cmxs] cmxs_source_shown('%s'...)", name);
}

void cmxs_source_hidden(void *data) {
    auto s = (cmxs_source_t *)data;  // NOLINT
    auto name = obs_source_get_name(s->obs_source);
    blog(LOG_INFO, "[obs-cmxs] cmxs_source_hidden('%s'...)", name);
}

void cmxs_source_activated(void *data) {
    auto s = (cmxs_source_t *)data;  // NOLINT
    auto name = obs_source_get_name(s->obs_source);
    blog(LOG_INFO, "[obs-cmxs] cmxs_source_activated('%s'...)", name);
}

void cmxs_source_deactivated(void *data) {
    auto s = (cmxs_source_t *)data;  // NOLINT
    auto name = obs_source_get_name(s->obs_source);
    blog(LOG_INFO, "[obs-cmxs] cmxs_source_deactivated('%s'...)", name);
}

void cmxs_source_get_defaults(obs_data_t *settings) {
    blog(LOG_INFO, "Enter cmxs_source_get_defaults");
    obs_data_set_default_bool(settings, PROP_START_PULL,
                  false);
}

obs_source_info create_cmxs_source_info() {
    obs_source_info cmxs_source_info = {};
    cmxs_source_info.id = "cmxs_source",
    cmxs_source_info.type = OBS_SOURCE_TYPE_INPUT;
    cmxs_source_info.output_flags = OBS_SOURCE_ASYNC_VIDEO |
                       OBS_SOURCE_AUDIO |
                       OBS_SOURCE_DO_NOT_DUPLICATE;
    cmxs_source_info.get_name = cmxs_source_getname;
    cmxs_source_info.get_properties = cmxs_source_getproperties;
    cmxs_source_info.get_defaults = cmxs_source_get_defaults;

    cmxs_source_info.create = cmxs_source_create;
    cmxs_source_info.activate = cmxs_source_activated;
    cmxs_source_info.show = cmxs_source_shown;
    cmxs_source_info.update = cmxs_source_update;
    cmxs_source_info.hide = cmxs_source_hidden;
    cmxs_source_info.deactivate = cmxs_source_deactivated;
    cmxs_source_info.destroy = cmxs_source_destroy;
    return cmxs_source_info;
}

#ifdef _WIN32
struct WinSockIniter {
    WinSockIniter() {
        WORD wsa_version = MAKEWORD(2, 2);
        WSADATA wsa_data;
        int wsa_err = WSAStartup(wsa_version, &wsa_data);
        if (wsa_err != 0) {
            blog(LOG_INFO, "WSAStartup failed: %d\n", wsa_err);
        }
    }
    ~WinSockIniter() {
        WSACleanup();
    }
};
static WinSockIniter sgWinSockIniter;
#endif
