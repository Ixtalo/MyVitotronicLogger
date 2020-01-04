
# VitoWifiMqtt

Read Vissmann boiler parameters with a ESP8266 and a DIY opto-coupler interface.  


## Prerequisites

- VisualStudio Code (VSCode)
- PlatformIO


## How-To Build
1. Generate private key for signing (`private.key`). See https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#signed-binary-prerequisites

    > openssl genrsa -out private.key 2048
    > openssl rsa -in private.key -outform PEM -pubout -out public.key

    Adjust option `signing_private_key` in `platformio.ini` to point to you `private.key` file.

    > signing_private_key = $SMARTHOME_WORKSPACE/private.key 

2. Open folder in VSCode
3. Open PlatformIO
4. Build
5. The signed binary is `vitowifimqtt/.pio/build/d1_mini/firmware.bin.signed`




## Debug Output


### NoWiFi
```
SDK:2.2.2-dev(38a443e)/Core:2.6.3=20603000/lwIP:STABLE-2_1_2_RELEASE/glue:1.2-16-ge23a07e/BearSSL:89454af
Boot mode: 1
Reset reason: Deep-Sleep Wake
Connecting WiFi ...
scandone
no Tintagel.dev found, reconnect after 1s
reconnect
scandone
no Tintagel.dev found, reconnect after 1s
reconnect
scandone
no Tintagel.dev found, reconnect after 1s
WiFi wifi_res: 1
DeepSleep! (30.0 sec)
del if0
usl
enter deep sleep
```


### WiFi, No DHCP
```
DK:2.2.2-dev(38a443e)/Core:2.6.3=20603000/lwIP:STABLE-2_1_2_RELEASE/glue:1.2-16-ge23a07e/BearSSL:89454af
Boot mode: 1
Reset reason: Deep-Sleep Wake
tstart: 85
Connecting WiFi ...
scandone
scandone
state: 0 -> 2 (b0)
state: 2 -> 3 (0)
state: 3 -> 5 (10)
add 0
aid 1
cnt 

connected with Tintagel.dev, channel 1
dhcp client start...
WiFi wifi_res: -1
DeepSleep! (30.0 sec)
state: 5 -> 0 (0)
rm 0
del if0
usl
enter deep sleep
```


### WiFi, DHCP, No MQTT
```
SDK:2.2.2-dev(38a443e)/Core:2.6.3=20603000/lwIP:STABLE-2_1_2_RELEASE/glue:1.2-16-ge23a07e/BearSSL:89454af
Boot mode: 1
Reset reason: Deep-Sleep Wake
tstart: 83
Connecting WiFi ...
scandone
state: 0 -> 2 (b0)
state: 2 -> 3 (0)
state: 3 -> 5 (10)
add 0
aid 1
cnt 

connected with Tintagel.dev, channel 1
dhcp client start...
ip:192.168.99.128,mask:255.255.255.0,gw:192.168.99.1
WiFi connect result time: 4125
WiFi wifi_res: 3
OTA update check ...
[OTA]  HTTP update fatal error code -1
HTTP_UPDATE_FAILD Error (-1): HTTP error: connection refused
OTA update check END.
Attempting MQTT connection...failed, rc=-2
failed, rc=-2
pm open,type:2 0
failed, rc=-2
WiFi disconnect ...
state: 5 -> 0 (0)
rm 0
pm close 7
DeepSleep! (30.0 sec)
del if0
usl
enter deep sleep
```

### OTA HTTP request
```
Connection from 192.168.99.128 52336 received!
GET /firmware.bin HTTP/1.0
Host: 192.168.99.1:8266
User-Agent: ESP8266-http-Update
Connection: close
x-ESP8266-Chip-ID: 2713345
x-ESP8266-STA-MAC: xxx
x-ESP8266-AP-MAC: xxx
x-ESP8266-free-space: 2797568
x-ESP8266-sketch-size: 346400
x-ESP8266-sketch-md5: e4fa3ca3d5b1ca2a9892513ec5f4b3ee
x-ESP8266-chip-size: 4194304
x-ESP8266-sdk-version: 2.2.2-dev(38a443e)
x-ESP8266-mode: sketch
Content-Length: 0
```