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
 * Basic AIP for load/unload
 * You can use CMake to generate makefile and make it.
 */

#ifdef _WIN32
#include <Windows.h>
#endif

#include <sys/stat.h>

#include <obs-module.h>
#include <plugin-support.h>
#include "obs.h"
#include "forms/output-settings.h"
#include "util/config-file.h"
#include <unordered_map>
extern "C" {
#include <libavutil/channel_layout.h>
}
#include <libavutil/mastering_display_metadata.h>
#include <libavformat/avformat.h>
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(59, 20, 100)
#include <libavcodec/version.h>
#endif
#include <libavcodec/avcodec.h>
#include <obs-avc.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QLibrary>
#include <QMainWindow>
#include <QAction>
#include <QMessageBox>
#include <QString>
#include <QStringList>

#include "plugin-main.h"
#include "main-output.h"
#include "Config.h"
#include "forms/output-settings.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

int s_g_cmxs_init = 0;
extern const char* s_g_host;
extern const char* s_g_deviceId;

const char *obs_module_name() {
    return "obs-cmxs";
}

const char *obs_module_description() {
    return "CMXS stream sink";
}


extern struct obs_output_info create_cmxs_output_info();
struct obs_output_info cmxs_output_info;

extern struct obs_source_info create_cmxs_source_info();
struct obs_source_info cmxs_source_info;

OutputSettings *output_settings = nullptr;

bool obs_module_load(void) {
    blog(LOG_INFO,
         "[obs-cmxs] obs_module_load: you can haz obs-cmxs (Version %s)",
         PLUGIN_VERSION);
    blog(LOG_INFO,
         "[obs-cmxs] obs_module_load: Qt Version: %s (runtime), %s (compiled)",
         qVersion(), QT_VERSION_STR);

    QMainWindow *main_window =
        reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());

    cmxs_output_info = create_cmxs_output_info();
    obs_register_output(&cmxs_output_info);
    cmxs_source_info = create_cmxs_source_info();
    obs_register_source(&cmxs_source_info);

    if (main_window) {
        Config *conf = Config::Current();
        conf->Load();
        main_output_init();
        // Ui setup
        QAction *menu_action =
            reinterpret_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
                obs_module_text(
                    "CMXSPlugin.OutputSettings")));

        obs_frontend_push_ui_translation(obs_module_get_string);
        output_settings = new OutputSettings(main_window);
        obs_frontend_pop_ui_translation();

        auto menu_cb = [] { output_settings->ToggleShowHide(); };
        menu_action->connect(menu_action, &QAction::triggered, menu_cb);

        obs_frontend_add_event_callback(
            [](enum obs_frontend_event event, void *private_data) {
                Config *conf = reinterpret_cast<Config *>(private_data);
                (void)conf;
                if (event ==
                    OBS_FRONTEND_EVENT_FINISHED_LOADING) {
                        // main_output_start();
                } else if (event == OBS_FRONTEND_EVENT_EXIT) {
                    main_output_stop();
                    main_output_deinit();
                }
            },
            reinterpret_cast<void *>(conf));
    }
    return true;
}

void obs_module_post_load(void) {
    blog(LOG_INFO, "[obs-cmxs] obs_module_post_load: ...");
}

void obs_module_unload(void) {
    blog(LOG_INFO, "[obs-cmxs] +obs_module_unload()");

    // obs_ffmpeg_unload_logging();
    CMXSSDK::uninit();
    if (s_g_host) {
        free((void*)s_g_host);  // NOLINT
        s_g_host = nullptr;
    }

    if (s_g_deviceId) {
        free((void*)s_g_deviceId);  // NOLINT
        s_g_deviceId = nullptr;
    }
    blog(LOG_INFO, "[obs-cmxs] obs_module_unload: goodbye !");
}

