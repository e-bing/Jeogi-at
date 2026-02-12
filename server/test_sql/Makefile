# Makefile for Jeogi Server

CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread
LIBS = -lmariadb -lssl -lcrypto

TARGET = main_server
SOURCES = Main_server.cpp Database.cpp Sensor.cpp Qt.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
