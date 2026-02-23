obj-m := motor_driver.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: module app

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

app:
	g++ -std=c++17 -o motor_app motor_app.cpp \
		-lpaho-mqttpp3 -lpaho-mqtt3as -lpthread

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f motor_app
