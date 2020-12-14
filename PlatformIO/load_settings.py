Import ("env")
import os.path
import configparser
import hashlib
from string import Template

settings = "settings.ini"
sectionMatrix = "Matrix"
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
        ("SITEID", "\\\"" + config[sectionMatrix]["siteId"] + "\\\""),
        ("HOSTNAME", "\\\"" + config[sectionMatrix]["hostname"] + "\\\""),
        ("MQTT_IP", "IPAddress\(" + config[sectionMqtt]["ip"].replace(".", ",") + "\)"),
        ("MQTT_HOST", "\\\"" + config[sectionMqtt]["hostname"] + "\\\""),
        ("MQTT_PORT", config[sectionMqtt]["port"]),
        ("MQTT_USER", "\\\"" + config[sectionMqtt]["username"] + "\\\""),
        ("MQTT_PASS", "\\\"" + config[sectionMqtt]["password"] + "\\\""),
        ("MQTT_MAX_PACKET_SIZE", config[sectionMqtt]["maxPacketSize"])
    ])

    if (config[sectionOta]["method"] == "upload") :
        env.Replace(
            TARGETS="upload",
        )
    
    if (config[sectionOta]["method"] == "ota") :
        env.Replace(
            UPLOAD_PROTOCOL="espota",
            UPLOAD_PORT=config[sectionMatrix]["hostname"],
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
