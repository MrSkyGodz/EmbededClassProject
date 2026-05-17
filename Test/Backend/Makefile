CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -pthread
INCLUDES := -Iinclude -I../../Project/Utils/Inc
PORT ?= 8081
TARGET_BASE := usart_test_backend
POSIX_TARGET := $(TARGET_BASE)
WINDOWS_TARGET := $(TARGET_BASE).exe
TARGET := $(POSIX_TARGET)

SRCS := \
	src/main.cpp \
	src/api/ApiServer.cpp \
	src/protocol/Frame.cpp \
	src/protocol/IcdOnlineParser.cpp \
	src/protocol/MessageRegistry.cpp \
	src/recorder/ParsedMessageRecorder.cpp \
	src/recorder/ReceiveRecorder.cpp \
	src/tests/TestRunner.cpp \
	src/transport/MockTransport.cpp \
	src/transport/SerialTransport.cpp \
	../../Project/Utils/Src/OnlineParser.cpp

ifeq ($(OS),Windows_NT)
TARGET := $(WINDOWS_TARGET)
OTHER_TARGET := $(POSIX_TARGET)
LDLIBS := -lws2_32
CLEAN_CMD := if exist $(POSIX_TARGET) del /Q $(POSIX_TARGET) & if exist $(WINDOWS_TARGET) del /Q $(WINDOWS_TARGET)
CLEAN_OTHER_CMD := if exist $(OTHER_TARGET) del /Q $(OTHER_TARGET) || exit /B 0
RUN_CMD := $(TARGET)
else
OTHER_TARGET := $(WINDOWS_TARGET)
LDLIBS :=
CLEAN_CMD := rm -f $(POSIX_TARGET) $(WINDOWS_TARGET)
CLEAN_OTHER_CMD := rm -f $(OTHER_TARGET) || true
RUN_CMD := ./$(TARGET)
endif

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CLEAN_OTHER_CMD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	$(RUN_CMD) $(PORT)

clean:
	$(CLEAN_CMD)
