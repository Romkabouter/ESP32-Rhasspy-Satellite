Import ("env")
import os.path
import configparser
import hashlib
from string import Template

settings = "settings.ini"
sectionGeneral = "General"
sectionWifi = "Wifi"
sectionOta = "OTA"
sectionMqtt = "MQTT"

if os.path.isfile(settings):
    config = configparser.RawConfigParser()
    config.read(settings)

    otaPasswordHash = hashlib.md5(config[sectionOta]['password'].encode()).hexdigest()
 
    env.Append(CPPDEFINES=[
        ("WIFI_SSID", "\\\"" + config[sectionWifi]["ssid"] + "\\\""),
        ("WIFI_PASS", "\\\"" + config[sectionWifi]["password"] + "\\\""),
        ("OTA_PASS_HASH", "\\\"" + otaPasswordHash + "\\\""),
        ("SITEID", "\\\"" + config[sectionGeneral]["siteId"] + "\\\""),
        ("HOSTNAME", "\\\"" + config[sectionGeneral]["hostname"] + "\\\""),
        ("MQTT_IP", "\\\"" + config[sectionMqtt]["ip"] + "\\\""),
        ("MQTT_PORT", config[sectionMqtt]["port"]),
        ("MQTT_USER", "\\\"" + config[sectionMqtt]["username"] + "\\\""),
        ("MQTT_PASS", "\\\"" + config[sectionMqtt]["password"] + "\\\""),
        ("MQTT_MAX_PACKET_SIZE", 2000),
        ("CONFIG_ASYNC_TCP_RUNNING_CORE", 0),
        ("DEVICE_TYPE", config[sectionGeneral]["device_type"])
    ])

    if (config[sectionOta]["method"] == "upload") :
        env.Replace(
            TARGETS="upload",
        )

    if (config[sectionOta]["method"] == "matrix") :
        env.Replace(
            UPLOAD_PROTOCOL="custom",
            UPLOADCMD="sh deploy.sh " + config[sectionGeneral]["deployhost"]
        )

    if (config[sectionOta]["method"] == "ota") :
        env.Replace(
            UPLOAD_PROTOCOL="espota",
            UPLOAD_PORT=config[sectionGeneral]["hostname"],
            UPLOAD_FLAGS=[
                "--port=" + config[sectionOta]["port"],
                "--auth=" + config[sectionOta]["password"],
                "--timeout=30",
                "--f=.pio/build/esp32dev/firmware.bin"
                ],
        )
else:
    print()
    print("Please copy 'settings.ini.example' to 'settings.ini' and set the correct values before building")
    exit(1)
