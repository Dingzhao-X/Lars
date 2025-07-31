CXX = g++
CFLAGS = -g -O2 -Wall -fPIC -lpthread 
SRC = ./src
INC = -I ./include
BUILD = ./build
TARGET = lib/libreactor.a
OBJS = $(patsubst $(SRC)/%.cpp, $(BUILD)/%.o, $(wildcard $(SRC)/*.cpp))

ALL: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p lib
	ar rcs $@ $^

$(BUILD)/%.o:$(SRC)/%.cpp
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@

.PHONY: ALL clean

clean:
	-rm -rf $(OBJS) $(TARGET)

