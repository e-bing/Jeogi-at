obj-m := motor_driver.o sht20_driver.o
KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)/src

SRC_DIR      := src
INCLUDES_DIR := includes
BUILD_DIR    := build

APP_SRCS := $(SRC_DIR)/main.cpp \
            $(SRC_DIR)/motor.cpp \
            $(SRC_DIR)/sensor.cpp \
            $(SRC_DIR)/sht20.cpp \
            $(SRC_DIR)/system_monitor.cpp \
            $(SRC_DIR)/communication.cpp \
            $(SRC_DIR)/audio.cpp \
            $(SRC_DIR)/display.cpp

.PHONY: all module app clean

all: module app

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

app:
	mkdir -p $(BUILD_DIR)
	g++ -std=c++17 \
		-Wall -Wextra \
		-I$(INCLUDES_DIR) \
		-o $(BUILD_DIR)/drivers \
		$(APP_SRCS) \
		-lpaho-mqttpp3 -lpaho-mqtt3as -lpthread

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(BUILD_DIR)/drivers
