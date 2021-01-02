## Get started for Matrix Voice

To get the code running I suggest you first reset the Voice if you have flashed it previously

- Follow Step 1 and 2 (or all steps) from this guide https://matrix-io.github.io/matrix-documentation/matrix-voice/esp32/
- Once your environment is set up go into the PlatformIO directory
- Copy and configure the `settings.ini.example` to `settings.ini` and configure the proper parameters. Any `$` in the `settings.ini` will be threated as an environment variable, did not find a way to escape it yet, fixes or workarounds are welcome
- Set the device_type to 1 in the settings.ini
- Build the project with PlatformIO, for example run `pio run` from the commandline
- Remotely flash the bin file using the provided script `sh deploy.sh pi@raspberry_ip_address`
- From this point one the MatrixVoice can run standalone and you should be able to update via OTA by running `pio run --target upload`.
  Use "ota" as method under the OTA section in the settings.ini
- If you change code and OTA does not work anymore for some reason, you can always start over by doing the "get started" part except for the first bullet
