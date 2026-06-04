################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../App/Src/Application.cpp \
../App/Src/CommunicationApplication.cpp \
../App/Src/GpioApplication.cpp \
../App/Src/ImuReferenceController.cpp \
../App/Src/ImuSensorApplication.cpp \
../App/Src/MotorApplication.cpp \
../App/Src/PwmApplication.cpp \
../App/Src/UartApplication.cpp 

OBJS += \
./App/Src/Application.o \
./App/Src/CommunicationApplication.o \
./App/Src/GpioApplication.o \
./App/Src/ImuReferenceController.o \
./App/Src/ImuSensorApplication.o \
./App/Src/MotorApplication.o \
./App/Src/PwmApplication.o \
./App/Src/UartApplication.o 

CPP_DEPS += \
./App/Src/Application.d \
./App/Src/CommunicationApplication.d \
./App/Src/GpioApplication.d \
./App/Src/ImuReferenceController.d \
./App/Src/ImuSensorApplication.d \
./App/Src/MotorApplication.d \
./App/Src/PwmApplication.d \
./App/Src/UartApplication.d 


# Each subdirectory must supply rules for building sources it contributes
App/Src/%.o App/Src/%.su App/Src/%.cyclo: ../App/Src/%.cpp App/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F767xx -c -I../Core/Inc -I../App/Inc -I../Utils/Inc -I../Utils/Types -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I"C:/Users/yunus/Desktop/Embbeded Systems/Project/deviceDrivers" -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-Src

clean-App-2f-Src:
	-$(RM) ./App/Src/Application.cyclo ./App/Src/Application.d ./App/Src/Application.o ./App/Src/Application.su ./App/Src/CommunicationApplication.cyclo ./App/Src/CommunicationApplication.d ./App/Src/CommunicationApplication.o ./App/Src/CommunicationApplication.su ./App/Src/GpioApplication.cyclo ./App/Src/GpioApplication.d ./App/Src/GpioApplication.o ./App/Src/GpioApplication.su ./App/Src/ImuReferenceController.cyclo ./App/Src/ImuReferenceController.d ./App/Src/ImuReferenceController.o ./App/Src/ImuReferenceController.su ./App/Src/ImuSensorApplication.cyclo ./App/Src/ImuSensorApplication.d ./App/Src/ImuSensorApplication.o ./App/Src/ImuSensorApplication.su ./App/Src/MotorApplication.cyclo ./App/Src/MotorApplication.d ./App/Src/MotorApplication.o ./App/Src/MotorApplication.su ./App/Src/PwmApplication.cyclo ./App/Src/PwmApplication.d ./App/Src/PwmApplication.o ./App/Src/PwmApplication.su ./App/Src/UartApplication.cyclo ./App/Src/UartApplication.d ./App/Src/UartApplication.o ./App/Src/UartApplication.su

.PHONY: clean-App-2f-Src

