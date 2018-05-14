UNAME := $(shell uname)

XTENSA_TOOLCHAIN := "xtensa-esp32-elf-linux64-1.22.0-61-gab8375a-5.2.0.tar.gz"

ifeq ($(UNAME), Darwin)
XTENSA_TOOLCHAIN := "xtensa-esp32-elf-osx-1.22.0-61-gab8375a-5.2.0.tar.gz"
endif

ESP_IDF_VERSION := "8bca703467be0d5e43e2a3dce2ca7727ca826474" # 2.1.1

fmt:
	clang-format -i ./*.c ./*.h -style="{BasedOnStyle: Google, ColumnLimit: 120}"
	clang-format -i ./test/main/*.c -style="{BasedOnStyle: Google, ColumnLimit: 120}"

test/xtensa-esp32-elf:
	wget https://dl.espressif.com/dl/$(XTENSA_TOOLCHAIN)
	cd test; tar -xzf ../$(XTENSA_TOOLCHAIN)
	rm *.tar.gz

test/esp-idf:
	git clone --recursive  https://github.com/espressif/esp-idf.git test/esp-idf
	cd test/esp-idf; git fetch; git checkout $(ESP_IDF_VERSION)
	cd test/esp-idf/; git submodule update --recursive

update:
	cd test/esp-idf; git fetch; git checkout $(ESP_IDF_VERSION)
	cd test/esp-idf/; git submodule update --recursive

defconfig: test/xtensa-esp32-elf test/esp-idf
	export PATH=$(shell pwd)/test/xtensa-esp32-elf/bin:$$PATH; cd ./test; make defconfig

erase: test/xtensa-esp32-elf test/esp-idf
	export PATH=$(shell pwd)/test/xtensa-esp32-elf/bin:$$PATH; cd ./test; make erase_flash

clean: test/xtensa-esp32-elf test/esp-idf
	export PATH=$(shell pwd)/test/xtensa-esp32-elf/bin:$$PATH; cd ./test; make clean

build: test/xtensa-esp32-elf test/esp-idf
	export PATH=$(shell pwd)/test/xtensa-esp32-elf/bin:$$PATH; cd ./test; make

flash: test/xtensa-esp32-elf test/esp-idf
	export PATH=$(shell pwd)/test/xtensa-esp32-elf/bin:$$PATH; cd ./test; make flash

monitor: test/xtensa-esp32-elf test/esp-idf
	@clear
	miniterm.py /dev/cu.SLAB_USBtoUART 115200 --rts 0 --dtr 0 --raw --exit-char 99

run: erase build flash monitor
