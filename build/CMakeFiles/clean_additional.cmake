# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "756351052.mp3.S"
  "bootloader\\bootloader.bin"
  "bootloader\\bootloader.elf"
  "bootloader\\bootloader.map"
  "config\\sdkconfig.cmake"
  "config\\sdkconfig.h"
  "esp-idf\\esptool_py\\flasher_args.json.in"
  "esp-idf\\mbedtls\\x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "music-16b-2c-22050hz.mp3.S"
  "music-16b-2c-44100hz.mp3.S"
  "music-16b-2c-8000hz.mp3.S"
  "play_mp3_control.bin"
  "play_mp3_control.map"
  "project_elf_src_esp32s3.c"
  "x509_crt_bundle.S"
  )
endif()
