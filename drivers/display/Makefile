# Makefile for display_pi_sub (Paho MQTT C++ + nlohmann/json)
# 사용:
#   make
#   make clean
#   make run

CXX      := g++
TARGET   := display_pi_sub
SRCS     := test_display.cpp
OBJS     := $(SRCS:.cpp=.o)

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS :=
LDFLAGS  :=

# Paho MQTT C++(mqtt/async_client.h) 링크 조합
# 보통: -lpaho-mqttpp3 -lpaho-mqtt3as (async, non-SSL)
LDLIBS   := -lpaho-mqttpp3 -lpaho-mqtt3as -pthread

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)