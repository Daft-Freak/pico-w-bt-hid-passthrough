cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(bt-hid-passthrough C CXX)

# Initialise the Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(bt-hid-passthrough
    main.cpp
    usb_descriptors.c
)

pico_set_program_name(bt-hid-passthrough "bt-hid-passthrough")
pico_set_program_version(bt-hid-passthrough "0.1")

pico_enable_stdio_uart(bt-hid-passthrough 1)
pico_enable_stdio_usb(bt-hid-passthrough 0)

target_link_libraries(bt-hid-passthrough
    pico_stdlib

    pico_btstack_ble
    pico_btstack_classic
    pico_btstack_cyw43
    pico_cyw43_arch_threadsafe_background

    tinyusb_device)

# for tusb_config and btstack_config
target_include_directories(bt-hid-passthrough PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# disable lwip
target_compile_definitions(bt-hid-passthrough PRIVATE CYW43_LWIP=0)

pico_add_extra_outputs(bt-hid-passthrough)

