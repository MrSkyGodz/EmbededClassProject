CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
INCLUDES := -I../../Core/Inc -I../Inc -I../Types
TEST_SRCS := ../Src/ComLib.cpp ../Src/OnlineParser.cpp ./example_online_parser.cpp online_parser_test.cpp
TEST_TARGET := online_parser_test.exe
SENDER_SRCS := usart_sender.cpp
SENDER_TARGET := usart_sender.exe
PORT ?=
PWM ?= 128

ifeq ($(OS),Windows_NT)
RUN_TEST_CMD := $(TEST_TARGET)
RUN_SENDER_CMD := $(SENDER_TARGET)
CLEAN_CMD := if exist $(TEST_TARGET) del /Q $(TEST_TARGET) & if exist $(SENDER_TARGET) del /Q $(SENDER_TARGET)
else
RUN_TEST_CMD := ./$(TEST_TARGET)
RUN_SENDER_CMD := ./$(SENDER_TARGET)
CLEAN_CMD := rm -f $(TEST_TARGET) $(SENDER_TARGET)
endif

.PHONY: all run send clean

all: $(TEST_TARGET) $(SENDER_TARGET)

$(TEST_TARGET): $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_SRCS) -o $(TEST_TARGET)

$(SENDER_TARGET): $(SENDER_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SENDER_SRCS) -o $(SENDER_TARGET)

run: $(TEST_TARGET)
	$(RUN_TEST_CMD)

send: $(SENDER_TARGET)
ifeq ($(PORT),)
	@echo "Usage: make send PORT=COM5 [PWM=128]"
	@exit 1
else
	$(RUN_SENDER_CMD) $(PORT) $(PWM)
endif

clean:
	$(CLEAN_CMD)
