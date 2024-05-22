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
 * This file creates a CMXS sender and the vidoe/audio encoders for media stream.
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
#include "Config.h"
#include "plugin-support.h"
#include "obs.h"
#include "forms/output-settings.h"
#include "util/config-file.h"
#include <obs-frontend-api.h>
#include "obs-cmxs-tool.h"
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
#include <windows.h>
#else
#include <unistd.h>
#endif

static int s_g_connecting_state = 0;
extern int s_g_cmxs_init;
#ifdef _WINDOWS
#define MY_SLEEP(_t) Sleep((_t) * 1000)
#else
#define MY_SLEEP sleep
#endif
class MyListener : public CMXSListener {
 public:
    void onMessage(uint32_t message,
        uint32_t param1,
        const void * param2) override {
        (void)param2;
        switch (message) {
        case CMXSMSG_ServerConnected:
            {
                s_g_connecting_state = 0;
            }
            break;
        case CMXSMSG_ServerConnectFailed:
            {
                blog(LOG_INFO, "platform report output failed: %u(%s)\n", param1, cmxssdk_error_str(param1));
                s_g_connecting_state = -1;
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
};

MyListener * s_g_myListener;
struct ffmpeg_audio_info {
    AVStream *stream;
    AVCodecContext *ctx;
};

struct cmxs_output {
    obs_output_t *output;
    volatile bool active;
    AVFormatContext *cmxs_ffmpeg_output;
    int videoStreamIndex;
    int audioStreamIndex;
    const char *host;
    const char *streamKey;
    const char *deviceId;

    Sender *sender;
    AVStream *video;
    AVCodecContext *video_ctx;
    struct ffmpeg_audio_info *audio_infos;
    bool connecting;
    pthread_t start_thread;

    uint64_t total_bytes;

    uint64_t audio_start_ts;
    uint64_t video_start_ts;
    uint64_t stop_ts;
    volatile bool stopping;
    obs_encoder_t *videoEncoder = nullptr;
    std::vector<obs_encoder_t *> audioEncoders;
    DARRAY(AVPacket *) packets;
    bool got_headers;
    bool adv_out;
};

const char *cmxs_output_getname(void *) {
    return obs_module_text("CMXSPlugin.OutputName");
}

int write_buffer(void *opaque, uint8_t *buf, int buf_size) {
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(opaque);

    CMXSErr err = stream->sender->send(buf, buf_size, -1);
    if (err != CMXSERR_OK) {
        blog(LOG_INFO, "sent failed");
        return 0;
    }

    return buf_size;
}

static bool new_stream(struct cmxs_output *ffm, AVStream **stream,
               const char *name) {
    blog(LOG_INFO, "Enter avformat_new_stream for encoder '%s', %p, %p\n",
            name, ffm, ffm->cmxs_ffmpeg_output);
    *stream = avformat_new_stream(ffm->cmxs_ffmpeg_output, NULL);
    if (!*stream) {
        blog(LOG_INFO, "Couldn't create stream for encoder '%s'\n",
            name);
        return false;
    }
    blog(LOG_INFO, "streamid is '%d'\n",
            (ffm->cmxs_ffmpeg_output->nb_streams - 1));
    (*stream)->id = ffm->cmxs_ffmpeg_output->nb_streams - 1;
    return true;
}

static void videoenc_set_params(struct cmxs_output *stream) {
    obs_encoder_t *vencoder = obs_output_get_video_encoder(stream->output);
    obs_data_t *settings = obs_encoder_get_settings(vencoder);

    obs_data_set_string(settings, "preset", "ultrafast");

    obs_data_set_string(settings, "tune", "zerolatency");
    obs_data_set_int(settings, "keyint_sec", 1);
    obs_data_set_bool(settings, "use_bufsize", true);
    obs_data_set_string(settings, "rate_control", "CRF");
    obs_data_set_string(settings, "profile", "baseline");
    obs_encoder_update(vencoder, settings);

    obs_data_release(settings);
}

void CreateVideoEncoder(void *data) {
    blog(LOG_INFO,
         "Enter CreateVideoEncoder");
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(data);

    obs_encoder_t *encoder;
    std::string videoname;
    if (stream->adv_out)
        videoname = "advanced_video_stream";
        // encoder = obs_get_encoder_by_name("streaming_h264"); //OBS 27.2.4 Or Older
    else
        videoname = "simple_video_stream";
        // encoder = obs_get_encoder_by_name("simple_h264_stream"); //OBS 27.2.4 Or Older
    encoder = obs_get_encoder_by_name(videoname.c_str());
    if (!encoder) {
        blog(LOG_INFO,
         "obs_get_encoder_by_name failed");
    }

    obs_encoder_release(stream->videoEncoder);
    stream->videoEncoder = obs_video_encoder_create(
        obs_encoder_get_id(encoder), "cmxs_output_video",
        obs_encoder_get_settings(encoder), nullptr);

    obs_encoder_release(encoder);
    obs_encoder_set_video(stream->videoEncoder, obs_get_video());
    obs_output_set_video_encoder(stream->output, stream->videoEncoder);
    videoenc_set_params(stream);
    struct obs_video_info ovi;

    if (!obs_get_video_info(&ovi)) {
        blog(LOG_INFO, "No active video");
        return;
    }

    const AVCodecDescriptor *codec = avcodec_descriptor_get_by_name("h264");
    if (!codec) {
        blog(LOG_INFO, "Couldn't find codec '%s'", videoname.c_str());
        return;
    }
    blog(LOG_INFO,
         "Enter new_stream for video stream");
    if (!new_stream(stream, &stream->video, "h264")) {
        blog(LOG_INFO,
         "Exit CreateVideoEncoder, new_stream failed");
        return;
    }

    stream->video->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->video->codecpar->codec_id = codec->id;
    blog(LOG_INFO,
         "Exit CreateVideoEncoder, stream->codecpar->video_delay is: %d", stream->video->codecpar->video_delay);
}

static bool create_audio_stream(struct cmxs_output *stream,
                const char *name, int idx) {
    AVCodecContext *context;
    AVStream *avstream = NULL;
    struct obs_audio_info aoi;
    int channels = 2;
    (void)channels;
    blog(LOG_INFO, "Enter create_audio_stream for '%s', %d", name, idx);
    const AVCodecDescriptor *codec = avcodec_descriptor_get_by_name("aac");
    if (!codec) {
        blog(LOG_INFO, "Couldn't find codec '%s'", name);
        return false;
    }

    blog(LOG_INFO, "Start get aoi");
    if (!obs_get_audio_info(&aoi)) {
        blog(LOG_INFO, "No active audio");
        return false;
    }
    blog(LOG_INFO, "Start new_stream");
    if (!new_stream(stream, &avstream, name)) {
        blog(LOG_INFO, "new_stream failed");
        return false;
    }
    const AVCodec* audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec) {
        avformat_free_context(stream->cmxs_ffmpeg_output);
        return false;
    }
    blog(LOG_INFO, "Start avcodec_alloc_context3");

    context = avcodec_alloc_context3(audioCodec);
    if (!context) {
        avformat_free_context(stream->cmxs_ffmpeg_output);
        return false;
    }

    uint8_t *header;
    size_t size;
    if (obs_encoder_get_extra_data(stream->audioEncoders.front(), &header, &size)) {
        blog(LOG_INFO, "extra data is: %x, %x", header[0], header[1]);
        stream->audio_infos[0].ctx->extradata = static_cast<uint8_t*>(av_mallocz(size));
        if (!stream->audio_infos[0].ctx->extradata) {
            return false;
        }
        memcpy(stream->audio_infos[0].ctx->extradata, header, size);
        stream->audio_infos[0].ctx->extradata_size = static_cast<int>(size);
    } else {
        blog(LOG_INFO, "no extra data");
    }
    avstream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    avstream->codecpar->codec_id = audioCodec->id;
    avstream->codecpar->sample_rate = aoi.samples_per_sec;
    blog(LOG_INFO, "get_audio_channels, set sample rate to %d", avstream->codecpar->sample_rate);
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 24, 100)
    context->channels = get_audio_channels(aoi.speakers);
#endif
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
    context->channel_layout =
        av_get_default_channel_layout(context->channels);
    if (aoi.speakers == SPEAKERS_4POINT1)
        context->channel_layout = av_get_channel_layout("4.1");
#else
    av_channel_layout_default(&context->ch_layout, channels);
    if (aoi.speakers == SPEAKERS_4POINT1) {
        #ifdef _WIN32
        context->ch_layout.nb_channels = 5;
        context->ch_layout.order = AV_CHANNEL_ORDER_NATIVE;
        context->ch_layout.u.mask = AV_CH_LAYOUT_4POINT1;
        #else
        context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_4POINT1;
        #endif
    }
#endif
    context->sample_fmt = AV_SAMPLE_FMT_S16;
    stream->audio_infos[idx].stream = avstream;
    stream->audio_infos[idx].ctx = context;
    return true;
}

void CreateAudioEncoder(void *data) {
    blog(LOG_INFO,
         "Enter CreateAudioEncoder");
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(data);
    if (!stream) {
        blog(LOG_INFO,
         "stream failed");
    }
    std::string encoder_id = "ffmpeg_aac";
    obs_encoder_t *encoder = obs_get_encoder_by_name("adv_stream_audio");
    if (!encoder) {
        blog(LOG_INFO,
         "obs_get_encoder_by_name(adv_stream_audio) failed");
    } else {
        encoder_id = obs_encoder_get_id(encoder);
    }
    for (const auto audioEncoder : stream->audioEncoders)
        obs_encoder_release(audioEncoder);
    stream->audioEncoders.clear();
    stream->audio_infos = (struct ffmpeg_audio_info *)calloc(1,
                    sizeof(*stream->audio_infos));
    auto trackIndex = 0;
    blog(LOG_INFO,
         "OBS_OUTPUT_MULTI_TRACK is: %d", OBS_OUTPUT_MULTI_TRACK);
    for (auto idx = 0; idx < 1; idx++) {
        auto audioEncoder = obs_audio_encoder_create(
            encoder_id.c_str(),
            std::string("cmxs_output_audio_track")
                .append(std::to_string(idx + 1))
                .c_str(),
            obs_encoder_get_settings(encoder), idx, nullptr);
        obs_encoder_set_audio(audioEncoder,
                      // obs_output_audio(stream->output));
                      obs_get_audio());
        stream->audioEncoders.push_back(audioEncoder);
        obs_output_set_audio_encoder(stream->output, audioEncoder,
                         trackIndex++);
        if (!create_audio_stream(stream, "aac", idx)) {
            blog(LOG_INFO,
                 "create_audio_stream[%d] failed", idx);
            return;
        }
        stream->audio_infos[idx].stream->id = idx;  // 设置音频流的stream ID
    }
    obs_encoder_release(encoder);
    blog(LOG_INFO,
         "Exit CreateAudioEncoder");
}



static bool cmxs_output_start(void *data) {
    // struct cmxs_output *stream = data;
    blog(LOG_INFO,
         "cmxs_output_start: starting CMXS main output");
    if (!data) {
        blog(LOG_INFO, "data is null");
        return false;
    }
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(data);

    Config *conf = Config::Current();
    if (!conf) {
        blog(LOG_INFO, "conf is null");
        return false;
    }
    conf->Load();
    config_t *basicConfig = obs_frontend_get_profile_config();
    if (!basicConfig) {
        blog(LOG_INFO, "basicConfig is null");
        return false;
    }
    const char *mode = config_get_string(basicConfig, "Output", "Mode");
    if (!mode) {
        blog(LOG_INFO, "mode is null");
        return false;
    }
    stream->adv_out = astrcmpi(mode, "Advanced") == 0;

    obs_data_t *settings = obs_output_get_settings(stream->output);

    const char* streamKey = obs_data_get_string(settings, "streamKey");
    size_t streamKeyLength = strlen(streamKey);
    stream->streamKey = reinterpret_cast<char*>(malloc((streamKeyLength + 1) * sizeof(char)));
    // strcpy((char*)stream->streamKey, (char*)streamKey);
    snprintf(const_cast<char*>(stream->streamKey), streamKeyLength+1,
                                                            "%s", const_cast<char*>(streamKey));
    obs_data_release(settings);
    settings = nullptr;
    #if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    AVOutputFormat *output_format;
    #else
        const AVOutputFormat *output_format;
    #endif
    output_format = av_guess_format("mpegts", NULL, NULL);
    if (output_format == NULL) {
        blog(LOG_INFO,
                    "Couldn't set output format to mpegts");
        return false;
    } else {
        blog(LOG_INFO, "Output format name and long_name: %s, %s",
             output_format->name ? output_format->name : "unknown",
             output_format->long_name ? output_format->long_name
                          : "unknown");
    }


    blog(LOG_INFO, "avformat_alloc_output_context2\n");
    int ret = avformat_alloc_output_context2(&stream->cmxs_ffmpeg_output, NULL, "mpegts", NULL);
    if (ret < 0) {
        blog(LOG_INFO, "avformat_alloc_output_context2 failed\n");
        return false;
    }

    CreateAudioEncoder(data);
    CreateVideoEncoder(data);

    ret = avformat_write_header(stream->cmxs_ffmpeg_output, NULL);
    if (ret < 0) {
        blog(LOG_INFO,
         "avformat_write_header: exit failed, %d", ret);
        return false;
    }
    if (!obs_output_can_begin_data_capture(stream->output, 0)) {
        blog(LOG_INFO,
         "cmxs_output_start: exit, obs_output_can_begin_data_capture failed");
        return false;
    }

    if (!obs_output_initialize_encoders(stream->output, 0)) {
        blog(LOG_INFO,
         "cmxs_output_start: exit, obs_output_initialize_encoders failed");
        return false;
    }
    os_atomic_set_bool(&stream->stopping, false);

    CMXSStreamParam_t streamCfg;
    memset(&streamCfg, 0, sizeof(CMXSStreamParam_t));
    blog(LOG_INFO,
         "cmxs_output_start: starting CMXS main output with param, %s",
         // stream->streamId,
         stream->streamKey);

    streamCfg.mStreamkey = stream->streamKey;
    streamCfg.mConnectTimeOut = 3000;

    s_g_myListener = nullptr;
    try {
        s_g_myListener = new MyListener();
    } catch (...) {
        blog(LOG_INFO, "No mem\n");
        return false;
    }

    fillStreamParam(conf->mSelectedNic, streamCfg);
    s_g_connecting_state = 1;
    blog(LOG_INFO, "cmxs_output_start: fillStreamParam done");
    stream->sender = Sender::create(&streamCfg, s_g_myListener);
    if (!stream->sender) {
        blog(LOG_INFO, "Sender::create failed\n");
        releaseStreamParamMemory(streamCfg);
        return false;
    }
    blog(LOG_INFO, "cmxs_output_start: Sender::create done");

    while (s_g_connecting_state == 1) {
        MY_SLEEP(1);
    }
    if (s_g_connecting_state != 0) {
        blog(LOG_INFO, "Connect failed\n");
        Sender::destroy(stream->sender);
        releaseStreamParamMemory(streamCfg);
        stream->sender = nullptr;
        delete s_g_myListener;
        s_g_myListener = nullptr;
        return false;
    }
    releaseStreamParamMemory(streamCfg);
    unsigned char* outbuffer = NULL;
    outbuffer = (unsigned char*)av_malloc(1316);
    stream->cmxs_ffmpeg_output->pb = avio_alloc_context(outbuffer, 1316,
                                                AVIO_FLAG_WRITE, data, NULL, write_buffer, NULL);
    if (!stream->cmxs_ffmpeg_output->pb) {
        av_freep(&outbuffer);
        return false;
    }
    stream->cmxs_ffmpeg_output->pb->max_packet_size = 1316;
    stream->cmxs_ffmpeg_output->pb->opaque = data;

    stream->cmxs_ffmpeg_output->flags |= AVFMT_FLAG_CUSTOM_IO;
    stream->cmxs_ffmpeg_output->flags |= AVFMT_FLAG_FLUSH_PACKETS;
    stream->cmxs_ffmpeg_output->flags |= AVFMT_FLAG_NONBLOCK;

    stream->active = true;
    conf->isConnected = true;
    if (!obs_output_begin_data_capture(stream->output, 0)) {
        blog(LOG_INFO, "obs_output_begin_data_capture return false\n");
    }

    return true;
}
static void cmxs_output_stop(void *data, uint64_t ts) {
    blog(LOG_INFO, "cmxs_output_stop");
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(data);
    stream->active = false;
    stream->stop_ts = ts / 1000;
    os_atomic_set_bool(&stream->stopping, true);

    free(const_cast<char*>(stream->streamKey));
    stream->streamKey = NULL;

    Sender::destroy(stream->sender);
    s_g_connecting_state = 1;
    if (s_g_myListener) {
        delete s_g_myListener;
        s_g_myListener = nullptr;
    }
    Config *conf = Config::Current();
    if (!conf) {
        blog(LOG_INFO, "conf is null");
        return;
    }
    conf->isConnected = false;
    stream->sender = nullptr;
}
static AVCodecContext *get_codec_context(cmxs_output *ffm,
                     struct encoder_packet *encpacket) {
    if (encpacket->type == OBS_ENCODER_VIDEO) {
        if (ffm->video) {
            return ffm->video_ctx;
        }
    } else {
        if (static_cast<int>(encpacket->track_idx) < OBS_OUTPUT_MULTI_TRACK) {
            return ffm->audio_infos[static_cast<int>(encpacket->track_idx)].ctx;
        }
    }

    return NULL;
}

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>


void cmxs_write_packet(struct cmxs_output *stream,
             struct encoder_packet *encpacket) {
    AVPacket *packet = av_packet_alloc();
    packet->buf = nullptr;
    if (!stream || !encpacket || !packet) {
        blog(LOG_INFO, "cmxs_write_packet input is null");
        return;
    }
    if ((os_atomic_load_bool(&stream->stopping)) || !stream->video ||
          !stream->audio_infos) {
            blog(LOG_INFO, "return , %d, %p, %p, %p", os_atomic_load_bool(&stream->stopping),
                            stream->video,
                            stream->video_ctx,
                            stream->audio_infos);
            return;
        }

    if (!stream->audio_infos[encpacket->track_idx].stream) {
            blog(LOG_INFO, "return 2: %zu", encpacket->track_idx);
            return;
        }
    bool is_video = encpacket->type == OBS_ENCODER_VIDEO;
    AVStream *avstream =
        is_video ? stream->video
             : stream->audio_infos[encpacket->track_idx].stream;

    if (!avstream) {
        blog(LOG_INFO, "cmxs_write_packet avstream is null");
        return;
    }

    uint8_t* pData;
    if (is_video) {
        packet->data = static_cast<uint8_t *>(av_memdup(encpacket->data, static_cast<int>(encpacket->size)));
        packet->size = static_cast<int>(encpacket->size);
        pData = packet->data;
    } else {
        uint8_t adts_header[7];
        int new_packet_size = static_cast<int>(encpacket->size) + 7;

        int profile = 2;  // AAC LC
        int freq = 3;  // 48kHz
        int chan = 2;  // 2channel

        // ADTS header
        adts_header[0] = 0xFF;
        adts_header[1] = 0xF9;
        adts_header[2] = ((profile - 1) << 6) + (freq << 2) + (chan >> 2);
        adts_header[3] = ((chan & 0x3) << 6) + (new_packet_size >> 11);
        adts_header[4] = (new_packet_size & 0x7FF) >> 3;
        adts_header[5] = ((new_packet_size & 0x7) << 5) + 0x1F;
        adts_header[6] = 0xFC;

        uint8_t* new_data = static_cast<uint8_t*>(av_malloc(new_packet_size));
        if (new_data) {
            pData = new_data;
            memcpy(new_data + 7, encpacket->data, encpacket->size);

            memcpy(new_data, adts_header, 7);

            av_freep(&packet->data);

            packet->data = new_data;
            packet->size = new_packet_size;
        } else {
            blog(LOG_ERROR, "Failed to allocate memory for audio packet");
        }
    }
    int ret = 0;
    if (packet->data == NULL) {
        blog(LOG_INFO, "packet->data == NULL");
        av_packet_free(&packet);
        return;
    }

    packet->stream_index = avstream->id;
    packet->pts = av_rescale_q(encpacket->pts, {encpacket->timebase_num, encpacket->timebase_den},
                                                                    {avstream->time_base.num, avstream->time_base.den});
    packet->dts = av_rescale_q(encpacket->dts, {encpacket->timebase_num, encpacket->timebase_den},
                                                                    {avstream->time_base.num, avstream->time_base.den});

    if (encpacket->keyframe)
        packet->flags = AV_PKT_FLAG_KEY;
    ret = av_interleaved_write_frame(stream->cmxs_ffmpeg_output, packet);
    if (0 != ret) {
        blog(LOG_INFO, "av_interleaved_write_frame failed");
    }
    if (pData) {
        av_freep(&pData);
    }
    av_packet_free(&packet);
    return;
}

static void *cmxs_output_create(obs_data_t *settings, obs_output_t *output) {
    blog(LOG_INFO,
         "cmxs_output_create: starting CMXS main output");
    (void)settings;
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(bzalloc(sizeof(struct cmxs_output)));
    stream->output = output;
    #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
    #endif

    return stream;
}

static void cmxs_output_data(void *data, struct encoder_packet *packet) {
    struct cmxs_output *stream = static_cast<struct cmxs_output *>(data);
    int code = OBS_OUTPUT_ENCODE_ERROR;
    if (!stream->active) {
        blog(LOG_INFO, "cmxs_output_data, stream->active failed");
        goto fail;
    }

    if (!packet) {
        blog(LOG_INFO, "cmxs_output_data, packet is null");
        goto fail;
    }
    if (stream->stopping) {
        blog(LOG_INFO, "cmxs_output_data, stream->stopping");
        if (packet->sys_dts_usec >= (int64_t)stream->stop_ts) {
            return;
        }
    }
    cmxs_write_packet(stream, packet);
    return;
fail:
    obs_output_signal_stop(stream->output, code);
}

obs_properties_t *cmxs_properties_callback(void *data) {
    UNUSED_PARAMETER(data);
    obs_properties_t *props = obs_properties_create();
    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
    obs_properties_add_text(props, "streamKey", "Stream Key", OBS_TEXT_DEFAULT);
    obs_properties_add_text(props, "server", "Server", OBS_TEXT_DEFAULT);
    obs_properties_add_text(props, "deviceId", "Device ID", OBS_TEXT_DEFAULT);
    return props;
}
static void cmxs_output_destroy(void *data) {
    bfree(data);
}
obs_output_info create_cmxs_output_info() {
    obs_output_info cmxs_output_info = {};
    cmxs_output_info.id = "cmxs_output",
    cmxs_output_info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED,
    cmxs_output_info.encoded_video_codecs = "h264",
    cmxs_output_info.encoded_audio_codecs = "aac",
    cmxs_output_info.get_name = cmxs_output_getname;
    cmxs_output_info.get_properties = cmxs_properties_callback;

    cmxs_output_info.create = cmxs_output_create;
    cmxs_output_info.start = cmxs_output_start;

    cmxs_output_info.stop = cmxs_output_stop;
    cmxs_output_info.destroy = cmxs_output_destroy;
    cmxs_output_info.encoded_packet = cmxs_output_data;

    return cmxs_output_info;
}

