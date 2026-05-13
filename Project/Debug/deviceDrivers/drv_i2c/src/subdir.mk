################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/yunus/Desktop/Embbeded\ Systems/Project/deviceDrivers/drv_i2c/src/drv_i2c.c 

C_DEPS += \
./deviceDrivers/drv_i2c/src/drv_i2c.d 

OBJS += \
./deviceDrivers/drv_i2c/src/drv_i2c.o 


# Each subdirectory must supply rules for building sources it contributes
deviceDrivers/drv_i2c/src/drv_i2c.o: C:/Users/yunus/Desktop/Embbeded\ Systems/Project/deviceDrivers/drv_i2c/src/drv_i2c.c deviceDrivers/drv_i2c/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F767xx -c -I../Core/Inc -I../App/Inc -I../Utils/Inc -I../Utils/Types -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I"C:/Users/yunus/Desktop/Embbeded Systems/Project/deviceDrivers" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-deviceDrivers-2f-drv_i2c-2f-src

clean-deviceDrivers-2f-drv_i2c-2f-src:
	-$(RM) ./deviceDrivers/drv_i2c/src/drv_i2c.cyclo ./deviceDrivers/drv_i2c/src/drv_i2c.d ./deviceDrivers/drv_i2c/src/drv_i2c.o ./deviceDrivers/drv_i2c/src/drv_i2c.su

.PHONY: clean-deviceDrivers-2f-drv_i2c-2f-src

