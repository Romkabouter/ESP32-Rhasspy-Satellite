## Get started for Matrix Voice

To get the code running I suggest you first reset the Voice if you have flashed it previously

- Follow Step 1 and 2 (or all steps) from this guide https://matrix-io.github.io/matrix-documentation/matrix-voice/esp32/
- Once your environment is set up go into the PlatformIO directory
- Copy and configure the `settings.ini.example` to `settings.ini` and configure the proper parameters. Any `$` in the `settings.ini` will be threated as an environment variable, did not find a way to escape it yet, fixes or workarounds are welcome
- Set the device_type to 1 in the settings.ini
- For the first flash, set the deployhost to the IP address of the attached pi and set method to 'matrix'
- Build and flash the project with PlatformIO, for example run `pio run --target upload` from the commandline
- After first flash you can use 'ota' as method under the OTA section in the settings.ini
- If you change code and OTA does not work anymore for some reason, you can always start over by doing the "get started" part except for the first bullet

## Known issues
- As per this issue https://github.com/Romkabouter/ESP32-Rhasspy-Satellite/issues/94 the Matrix Voice only supports buster. Read that issue for a possible work-around