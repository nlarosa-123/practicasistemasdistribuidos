# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/usuarioso/sed/esp-idf/components/bootloader/subproject"
  "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader"
  "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix"
  "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix/tmp"
  "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix/src/bootloader-stamp"
  "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix/src"
  "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/usuarioso/sed/esp-workspace/parking_mqtt/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
