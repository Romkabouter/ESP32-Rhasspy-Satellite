MV_ESPTOOLPY_SRC := voice_esptool

ESPPORT := /dev/ttyS0

MV_ESPTOOLPY := $(MV_ESPTOOLPY_SRC) --chip esp32

MV_ESPTOOLPY_SERIAL := $(MV_ESPTOOLPY) --port $(ESPPORT) --baud $(ESPBAUD) --before $(CONFIG_ESPTOOLPY_BEFORE) --after $(CONFIG_ESPTOOLPY_AFTER)

ESPTOOL_WRITE_FLASH := $(MV_ESPTOOLPY_SERIAL) write_flash $(if $(CONFIG_ESPTOOL_COMPRESSED),-z,-u) $(ESPTOOL_WRITE_FLASH_OPTIONS)

DEPLOY_CMD := $(shell echo $(ESPTOOL_WRITE_FLASH) $(ESPTOOL_ALL_FLASH_ARGS) | sed -e "s=$(PWD)=/tmp=g")

deploy: all_binaries
	@echo ""
	@echo "**************************************************************"	
	@echo "Programming the ESP32 in MATRIX Voice through the Raspberry PI"
	@echo "**************************************************************"	
	tar cf - build/bootloader/bootloader.bin build/*.bin | ssh pi@192.168.178.194 'tar xf - -C /tmp;sudo voice_esp32_reset;$(DEPLOY_CMD)'
