################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Flash-W25N04KV/flash.c 

OBJS += \
./Flash-W25N04KV/flash.o 

C_DEPS += \
./Flash-W25N04KV/flash.d 


# Each subdirectory must supply rules for building sources it contributes
Flash-W25N04KV/%.o Flash-W25N04KV/%.su Flash-W25N04KV/%.cyclo: ../Flash-W25N04KV/%.c Flash-W25N04KV/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I"/home/niyaz/STM32CubeIDE/workspace_1.17.0/nucleo-f746zg-flashmem/Flash-W25N04KV" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Flash-2d-W25N04KV

clean-Flash-2d-W25N04KV:
	-$(RM) ./Flash-W25N04KV/flash.cyclo ./Flash-W25N04KV/flash.d ./Flash-W25N04KV/flash.o ./Flash-W25N04KV/flash.su

.PHONY: clean-Flash-2d-W25N04KV

