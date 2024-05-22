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
