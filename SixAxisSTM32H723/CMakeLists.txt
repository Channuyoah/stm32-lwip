cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME SixAxisSTM32H723)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Application/ht_client_task.c
    Application/ht_client_task_interrupt.c
    Application/ht_client_task_motor.c
    Application/ht_udp_server.c
    Application/xq_adc.c
    Application/xq_axis.c
    Application/xq_axis_intp.c
    Application/xq_curve.c
    Application/xq_dac.c
    Application/xq_encoder.c
    Application/xq_io.c
    Application/xq_period_task.c
    Application/xq_pwm.c
    Application/xq_sdram.c
    Application/xq_utility.c
    FreeModbus/mb.c
    FreeModbus/mbcrc.c
    FreeModbus/mbfunccoils.c
    FreeModbus/mbfuncdiag.c
    FreeModbus/mbfuncdisc.c
    FreeModbus/mbfuncholding.c
    FreeModbus/mbfuncinput.c
    FreeModbus/mbfuncother.c
    FreeModbus/mbrtu.c
    FreeModbus/mbtcp.c
    FreeModbus/mbutils.c
    FreeModbus/portevent.c
    FreeModbus/portother.c
    FreeModbus/portserial.c
    FreeModbus/porttcp.c
    FreeModbus/porttimer.c
    FreeModbus/xq_modbus.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Application
    FreeModbus
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
