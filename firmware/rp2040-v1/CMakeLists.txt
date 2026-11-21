# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Code C CXX ASM)
string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--print-memory-usage")

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add lgvl subdirectory
add_subdirectory(libs/lvgl)

# Add executable. Default name is the project name, version 0.1
add_executable(Code 
	src/main.c
	src/shared_data.c
    	src/mcp3208.c
    	src/mux_hc4067.c
    	src/ili9488.c
    	src/hr2046.c
    	src/usb_descriptors.c
    	src/usb_hid.c
    	src/core1_display.c
    	src/display_pages.c
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Code 0)
pico_enable_stdio_usb(Code 1)

# Add the standard library to the build
target_link_libraries(Code
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(Code PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/inc
	${CMAKE_CURRENT_LIST_DIR}/libs/lvgl/include
	${CMAKE_CURRENT_LIST_DIR}/libs/lvgl
)

# Add any user requested libraries
target_link_libraries(Code 
        pico_stdlib
    	pico_multicore
    	hardware_spi
    	hardware_adc
    	hardware_gpio
    	hardware_irq
    	hardware_timer
    	hardware_sync
    	hardware_dma
    	pico_sync
    	tinyusb_device
    	tinyusb_board
    	lvgl
        )

pico_set_program_name(Code "FlightPanel")
pico_set_program_version(Code "1.0")

pico_add_extra_outputs(Code)

