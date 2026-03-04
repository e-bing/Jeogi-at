cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER arm-none-eabi-gcc)
endif()
if(NOT DEFINED CMAKE_ASM_COMPILER)
  set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(STM32F401_EdgeDevice C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(TARGET STM32F401_EdgeDevice)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F401RETX_FLASH.ld)

set(MCU_FLAGS
  -mcpu=cortex-m4
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -mthumb
)

set(COMMON_DEFINES
  USE_HAL_DRIVER
  STM32F401xE
  $<$<CONFIG:Debug>:DEBUG>
)

set(COMMON_INCLUDES
  ${CMAKE_SOURCE_DIR}/Core/Inc
  ${CMAKE_SOURCE_DIR}/User/GUI
  ${CMAKE_SOURCE_DIR}/User/RGBMatrix
  ${CMAKE_SOURCE_DIR}/User/Fonts
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
  ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
  ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

set(C_SOURCES
  Core/Src/gpio.c
  Core/Src/main.c
  Core/Src/Matrixrun.c
  Core/Src/app_task.c
  Core/Src/stm32f4xx_hal_msp.c
  Core/Src/stm32f4xx_it.c
  Core/Src/syscalls.c
  Core/Src/sysmem.c
  Core/Src/system_stm32f4xx.c
  Core/Src/usart.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
  User/Fonts/font12CN.c
  User/Fonts/font24CN.c
  User/Fonts/font8.c
  User/GUI/Data_Manager.c
  User/GUI/Display_Screens.c
  User/GUI/GUI_Paint.c
  User/RGBMatrix/RGBMatrix_device.c
)

set(ASM_SOURCES
  Core/Startup/startup_stm32f401retx.s
)

add_executable(${TARGET}
  ${C_SOURCES}
  ${ASM_SOURCES}
)

target_include_directories(${TARGET} PRIVATE ${COMMON_INCLUDES})
target_compile_definitions(${TARGET} PRIVATE ${COMMON_DEFINES})

target_compile_options(${TARGET} PRIVATE
  ${MCU_FLAGS}
  -ffunction-sections
  -fdata-sections
  -Wall
  --specs=nano.specs
  $<$<CONFIG:Debug>:-g3>
  $<$<CONFIG:Debug>:-O0>
)

target_compile_options(${TARGET} PRIVATE
  $<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>
)

target_link_options(${TARGET} PRIVATE
  ${MCU_FLAGS}
  -T${LINKER_SCRIPT}
  --specs=nosys.specs
  --specs=nano.specs
  -Wl,-Map=${TARGET}.map
  -Wl,--gc-sections
  -static
  -Wl,--start-group
  -lc
  -lm
  -Wl,--end-group
)

set(HEX_FILE ${CMAKE_BINARY_DIR}/${TARGET}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${TARGET}.bin)

add_custom_command(TARGET ${TARGET} POST_BUILD
  COMMAND arm-none-eabi-size $<TARGET_FILE:${TARGET}>
  COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:${TARGET}> ${HEX_FILE}
  COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${TARGET}> ${BIN_FILE}
  COMMENT "Generating size, HEX and BIN artifacts"
)

