cmake_minimum_required(VERSION 3.16)

set(PICO_BOARD pico)

# Pull in SDK (must be before project)
include(pico-sdk/external/pico_sdk_import.cmake)

project(jiggle C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.5.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

##add_subdirectory(adc)
include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

# remember: needs to come before add_executable!
add_compile_options(
        -Wall
        -falign-functions=16
        #-Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        #-Wno-unused-function # we have some for the docs that aren't called
        #-Wno-maybe-uninitialized
)

add_executable(jiggle
        src/main.cpp
)

target_compile_definitions(jiggle PRIVATE
        PARAM_ASSERTIONS_ENABLE_ALL=1
        # default pins
        PICO_DEFAULT_UART=1 PICO_DEFAULT_UART_TX_PIN=20 PICO_DEFAULT_UART_RX_PIN=21 PICO_UART_ENABLE_CRLF_SUPPORT=0
        PICO_DEFAULT_I2C=1 PICO_DEFAULT_I2C_SDA_PIN=26 PICO_DEFAULT_I2C_SCL_PIN=27
        # compat
        PICO_PRINTF_ALWAYS_INCLUDED=1
        PICO_RP2040_B0_SUPPORTED=0 PICO_RP2040_B1_SUPPORTED=0 PICO_RP2040_B2_SUPPORTED=1
        # debug & perf
        PICO_STDOUT_MUTEX=0
        PICO_USE_STACK_GUARDS=1
)

# pull in common dependencies
target_link_libraries(jiggle
        pico_stdlib pico_runtime pico_multicore
        hardware_rtc
        tinyusb_device tinyusb_board
)

# create map/bin/hex file etc.
pico_add_extra_outputs(jiggle)
