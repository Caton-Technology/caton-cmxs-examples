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
 * This file creates a UI for configuration.
 * You can use CMake to generate makefile and make it.
 */

#ifndef OUTPUTSETTINGS_H
#define OUTPUTSETTINGS_H

#include <QDialog>
#include <unordered_map>
#include <QComboBox>


#include "ui_output-settings.h"
using WidgetPair = std::pair<QCheckBox*, QComboBox*>;

class OutputSettings : public QDialog {
    Q_OBJECT
 public:
    explicit OutputSettings(QWidget *parent = 0);
    ~OutputSettings();
    void showEvent(QShowEvent *event);
    void ToggleShowHide();
    void setStatusIndicatorColor(const QColor &color);
    #ifdef __APPLE__
    std::unordered_map<std::string, WidgetPair> mLabelWidgetMap;
    #endif
 private slots:
    void onFormAccepted();

 private:
    Ui::OutputSettings *ui;
};

#endif    // OUTPUTSETTINGS_H
