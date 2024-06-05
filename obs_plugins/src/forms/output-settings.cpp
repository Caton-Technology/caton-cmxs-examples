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

#include "output-settings.h"

#include "../Config.h"
#include "../main-output.h"
#include <QCheckBox>
#include <iostream>
#ifndef _WIN32
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <algorithm>
#include <utility>
#include <iomanip>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "../obs-cmxs-tool.h"
#endif
#include <cstring>
#include <cmxssdk/cmxs_type.h>

extern int s_g_cmxs_init;
extern const char* s_g_host;
extern const char* s_g_deviceId;
OutputSettings::OutputSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::OutputSettings) {

    ui->setupUi(this);

    #ifdef __APPLE__
    QVBoxLayout* dynamicInputsLayout = ui->dynamicInputsLayout;
    std::unordered_map<std::string, std::string>   nics;
    getNetworkInterfacesInfo(nics);
    for (const auto& entry : nics) {
        bool nicIsWifi = false;
        if (!getNicType(entry.first, nicIsWifi)) {
            continue;
        }
        const int labelWidth = 200;
        const std::string labelStr = entry.first + ": " + entry.second;

        QCheckBox* checkBox = new QCheckBox();

        QLabel* label = new QLabel(QString::fromStdString(labelStr));
        label->setFixedWidth(labelWidth);
        QComboBox* comboBox = new QComboBox();
        comboBox->addItem(obs_module_text("CMXSPlugin.wifi"));
        comboBox->addItem(obs_module_text("CMXSPlugin.cable"));
        comboBox->addItem(obs_module_text("CMXSPlugin.cellular"));
        if (nicIsWifi) {
            comboBox->setCurrentIndex(comboBox->findText(obs_module_text("CMXSPlugin.wifi")));
        } else {
            comboBox->setCurrentIndex(comboBox->findText(obs_module_text("CMXSPlugin.cable")));
        }
        QHBoxLayout* horizontalLayout = new QHBoxLayout();
        horizontalLayout->setAlignment(Qt::AlignLeft);
        horizontalLayout->addWidget(checkBox);
        horizontalLayout->addWidget(label, 0, Qt::AlignLeft);
        horizontalLayout->addWidget(comboBox);
        dynamicInputsLayout->addLayout(horizontalLayout);
        mLabelWidgetMap[entry.first] = std::make_pair(checkBox, comboBox);
    }
    #endif
    connect(ui->buttonBox, SIGNAL(accepted()), this,
    SLOT(onFormAccepted()));
}


void OutputSettings::onFormAccepted() {
    blog(LOG_INFO,
         "onFormAccepted: starting CMXS main output");
    Config *conf = Config::Current();

    conf->host = ui->GlobalHost->text();

    conf->deviceId = ui->GlobalDeviceId->text();

    conf->streamKey = ui->StreamKey->text();
    #ifdef __APPLE__
    conf->mSelectedNic.clear();
    for (const auto& pair : mLabelWidgetMap) {
        std::string label = pair.first;
        QCheckBox* checkBox = pair.second.first;
        QComboBox* comboBox = pair.second.second;

        if (checkBox->isChecked()) {
            QString selectedText = comboBox->currentText();
            std::string selectedValue = selectedText.toStdString();
            if (selectedValue == "wifi") {
                conf->mSelectedNic[pair.first] = kCMXSLinkDeviceTypeWiFi;
            } else if (selectedValue == "cable") {
                conf->mSelectedNic[pair.first] = kCMXSLinkDeviceTypeCable;
            } else if (selectedValue == "cellular") {
                conf->mSelectedNic[pair.first] = kCMXSLinkDeviceTypeCellular;
            } else {
                conf->mSelectedNic[pair.first] = kCMXSLinkDeviceTypeUnknown;
            }
        }
    }
    #endif
    conf->Save();
    main_output_gbl_init();
    conf->isStart = ui->enableStreamingCheckbox->isChecked();
    blog(LOG_INFO,
        "onFormAccepted: starting CMXS main output, %s, %s, %s",
        qPrintable(conf->host),
        qPrintable(conf->deviceId),
        qPrintable(conf->streamName));
    if (conf->isStart) {
        if (main_output_is_running()) {
            main_output_stop();
        }
        main_output_start();
    } else {
        main_output_stop();
    }
}

void OutputSettings::showEvent(QShowEvent *event) {
    UNUSED_PARAMETER(event);
    Config *conf = Config::Current();
    if (s_g_cmxs_init) {
        ui->GlobalHost->setText(s_g_host);
        ui->GlobalDeviceId->setText(s_g_deviceId);
    } else {
        ui->GlobalHost->setText(conf->host);
        ui->GlobalDeviceId->setText(conf->deviceId);
    }
    #ifdef _WIN32
        ui->net_interface->setVisible(false);
    #endif
    ui->StreamKey->setText(conf->streamKey);
    ui->enableStreamingCheckbox->setChecked(conf->isStart);
    const char* copyrightInfo = "Copyright © 2023 Caton Technology. All rights reserved.";
    const char* versionString = cmxssdk_version();

    QString fullVersionInfo = QString::fromUtf8("%1<br>Powered by Caton mediaXstream %2").arg(copyrightInfo).arg(versionString);  // NOLINT

    ui->versionLabel->setText(fullVersionInfo);
    ui->versionLabel->setAlignment(Qt::AlignHCenter);

    if (conf->isConnected) {
        ui->statusIndicator->setStyleSheet("background-color: green; border-radius: 8px;");
    } else {
        ui->statusIndicator->setStyleSheet("background-color: red; border-radius: 8px;");
    }
}

void OutputSettings::ToggleShowHide() {
    setVisible(!isVisible());
}

OutputSettings::~OutputSettings() {
    #ifdef __APPLE__
    for (const auto& pair : mLabelWidgetMap) {
        delete pair.second.first;  // QCheckBox
        delete pair.second.second;  // QComboBox
    }
    #endif
    delete ui;
}
