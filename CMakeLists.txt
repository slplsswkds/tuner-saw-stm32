cmake_minimum_required(VERSION 3.22)

project(tuner-code C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
        Core/Src/main.c
        Core/Src/tuner.c
        Core/Src/uart_log.c
        Core/Src/adc_data.c
        Core/Src/stm32f4xx_it.c
        Core/Src/stm32f4xx_hal_msp.c
        Core/Src/syscalls.c
        Core/Src/sysmem.c
        Core/Src/system_stm32f4xx.c
)

target_sources(${PROJECT_NAME} PRIVATE
        Core/ssd1306_stm32_hal/src/ssd1306.c
        Core/ssd1306_stm32_hal/src/fonts.c
)

target_sources(${PROJECT_NAME} PRIVATE
        startup_stm32f411xe.s
)

target_include_directories(${PROJECT_NAME} PRIVATE
        Core/Inc
        Core/ssd1306_stm32_hal/inc
)

option(UART "Enable UART features" ON)
option(UART_LOG "Enable UART log output" ON)
option(UART_DEBUG "Enable UART debug output" OFF)
option(UART_DEBUG_ARRAYS "Enable UART debug arrays output" OFF)

if (UART)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UART)

    if (UART_LOG)
        target_compile_definitions(${PROJECT_NAME} PRIVATE UART_LOG)
    endif ()

    if (UART_DEBUG)
        if (UART_DEBUG_ARRAYS)
            target_compile_definitions(${PROJECT_NAME} PRIVATE UART_DEBUG_ARRAYS)
        endif ()
    endif ()
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")

add_subdirectory(cmake/stm32cubemx)
target_link_libraries(${PROJECT_NAME} PRIVATE stm32cubemx)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
        COMMENT "Generating ${PROJECT_NAME}.bin"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
        COMMENT "Generating ${PROJECT_NAME}.hex"
)

add_custom_target(flash
        COMMAND ${CMAKE_SOURCE_DIR}/flash.sh
        DEPENDS ${PROJECT_NAME}
)
