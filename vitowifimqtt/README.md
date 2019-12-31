
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

