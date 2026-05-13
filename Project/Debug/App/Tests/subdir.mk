################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../App/Tests/example_online_parser.cpp \
../App/Tests/online_parser_test.cpp 

OBJS += \
./App/Tests/example_online_parser.o \
./App/Tests/online_parser_test.o 

CPP_DEPS += \
./App/Tests/example_online_parser.d \
./App/Tests/online_parser_test.d 


# Each subdirectory must supply rules for building sources it contributes
App/Tests/%.o App/Tests/%.su App/Tests/%.cyclo: ../App/Tests/%.cpp App/Tests/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F767xx -c -I../Core/Inc -I../App/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-Tests

clean-App-2f-Tests:
	-$(RM) ./App/Tests/example_online_parser.cyclo ./App/Tests/example_online_parser.d ./App/Tests/example_online_parser.o ./App/Tests/example_online_parser.su ./App/Tests/online_parser_test.cyclo ./App/Tests/online_parser_test.d ./App/Tests/online_parser_test.o ./App/Tests/online_parser_test.su

.PHONY: clean-App-2f-Tests

