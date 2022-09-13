#!/bin/bash

adb.exe push install/bin /usr/
adb.exe push install/etc /
# adb.exe push fota_pangu.conf /etc/fota.conf
# adb.exe push fota.conf /etc/fota.conf
echo "push kvtool"
adb.exe push kvtool /usr/bin
adb.exe shell chmod +x /usr/bin/kvtool
echo "set kv default value"
adb.exe shell kvtool set fota_cldp "cop2"
adb.exe shell kvtool set fota_tourl "flash2://misc"
adb.exe shell kvtool set otaurl "http://occ.t-head.cn/api/image/ota/pull"
adb.exe shell kvtool setint fota_retry 1
adb.exe shell kvtool setint fota_autock 0
adb.exe shell kvtool setint fota_slptm 5000
adb.exe shell kvtool setint fota_rtmout 10000
adb.exe shell kvtool setint fota_wtmout 5000
adb.exe shell kvtool setint fota_finish 0
adb.exe shell kvtool set versionA "6.0.1-20211229.1847-R-D1_Linu"
adb.exe shell kvtool set versionB "6.0.1-20211229.1847-R-D1_Linu"
adb.exe shell kvtool set changelogA "the\ init\ version"
adb.exe shell kvtool set changelogB "the\ init\ version"
adb.exe shell kvtool del cop_version
adb.exe shell kvtool del newchangelog
echo "set nvram default value"
adb.exe shell kvtool nvset device_id "265a24af0440000090e345bc86a3aacb"
adb.exe shell kvtool nvset model "D1_Linux"
adb.exe shell kvtool nvset app_version "6.0.1-20211229.1847-R-D1_Linu"
adb.exe shell kvtool nvset changelog "the\ init\ version"
adb.exe shell sync
echo "push over"