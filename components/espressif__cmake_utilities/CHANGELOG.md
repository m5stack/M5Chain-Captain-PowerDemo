## v0.5.3 - 2023-09-15

* fix `add_dependencies called with incorrect number of arguments` in `relinker.cmake`
* `include(cmake_utilities)` is not suggested now, to avoid cmake_utilities dependency issue

## v0.5.2 - 2023-09-15

* Support work on older ESP-IDF, eg: 4.3.x

## v0.5.1 - 2023-08-22

* Add string 1-byte align support

## v0.5.0 - 2023-08-02

* Add GCC LTO support

## V0.5.8 - 2023-05-24

* Add unit test app

### Bugfix:

* fix customer target redefinition issue

## V0.5.7 - 2023-04-21

* gen_compressed_ota: support the addition of a v2 compressed OTA header to compressed firmware.

## V0.5.6 - 2023-04-20

* relinker: add IDF v4.3.x support

## V0.5.5 - 2023-04-17

* gen_compressed_ota: remove slash use in gen_custom_ota.py so that the script can be used in the Windows cmd terminal.

## V0.5.4 - 2023-04-07

* relinker: suppressing the creation of `__pycache__`
* relinker: support same name objects in one library

## V0.5.3 - 2023-03-24

* relinker: support decoding to get IRAM excluded libraries

## V0.5.2 - 2023-03-20

### Bugfix:

* gen_compressed_ota: Fix the number of bytes reserved in the v3 compressed image header
* gen_compressed_ota: Fix definition of MD5 length in compressed image header for different versions.

## V0.5.1 - 2023-03-15

* relinker: add option to use customized configuration files
* relinker: add option to print error information instead of throwing exception when missing function
* relinker: move functions of SPI flash from IRAM to flash only when enable CONFIG_SPI_FLASH_ROM_IMPL
* relinker: move some functions of esp_timer from IRAM to flash only when disable CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD

## V0.5.0 - 2023-03-13

### Feature:

* Add command `idf.py gen_single_bin` to generate merged bin file
* Add command `idf.py flash_single_bin` to flash generated merged bin
* Add config `Color in diagnostics` to control the GCC color output

## V0.5.0 - 2023-03-10

* Add gen_compressed_ota functionality, please refer to [gen_compressed_ota.md](https://github.com/espressif/esp-iot-solution/tree/master/tools/cmake_utilities/docs/gen_compressed_ota.md)

## v0.2.1 - 2023-03-09

### Bugfix:

* package manager: Fix the compile issue when the name of the component has `-`, just like esp-xxx

## v0.2.0 - 2023-02-23

* Add relinker functionality, please refer to [relinker.md](https://github.com/espressif/esp-iot-solution/tree/master/tools/cmake_utilities/docs/relinker.md)

## v0.1.0 - 2023-01-12

### Feature:

* Add function cu_pkg_get_version
* Add macro cu_pkg_define_version
* Add cmake script to CMAKE_MODULE_PATH
