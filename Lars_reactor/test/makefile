CXX=g++
CFLAGS=-g -O2 -Wall -fPIC

INC=-I ../include
LIB=-L ../lib -lreactor 
OBJS = $(wildcard *.cpp)
TARGET = $(patsubst %.cpp, %, $(OBJS))

ALL:$(TARGET)

$(TARGET):%:%.cpp
	$(CXX) $(CFLAGS) $< -o $@ $(INC) $(LIB)
	
.PHONY: clean

clean:
	-rm -f $(TARGET) 
