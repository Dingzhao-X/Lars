TARGET = bin/lars_lb_agent
CXX = g++
CFLAGS = -g -O2 -Wall -std=c++17 -Wno-deprecated

# 项目路径配置
REACTOR_XDZ = ..
LARS_REACTOR = $(REACTOR_XDZ)/lars_reactor
COMMON = $(REACTOR_XDZ)/common
ORIGINAL_BASE = ../../base

# 头文件路径
LARS_REACTOR_H = $(LARS_REACTOR)/include
COMMON_H = $(COMMON)/include
MYSQL_H = $(ORIGINAL_BASE)/mysql-connector-c/include

# 库文件路径
LARS_REACTOR_LIB = $(LARS_REACTOR)/lib
MYSQL_LIB = $(ORIGINAL_BASE)/mysql-connector-c/lib/libmysqlclient.a

# 其他库
OTHER_LIB = -lpthread -ldl -lprotobuf

# 源文件目录
SRC = ./src

# 包含路径
INC = -I./include -I$(LARS_REACTOR_H) -I$(COMMON_H) -I$(MYSQL_H)

# 链接库  
LIB = $(MYSQL_LIB) -L$(LARS_REACTOR_LIB) -lreactor $(OTHER_LIB)

# 获取所有源文件对象
OBJS = $(addsuffix .o, $(basename $(wildcard $(SRC)/*.cpp)))
OBJS += $(COMMON)/include/lars.pb.o

# 主目标
$(TARGET): $(OBJS)
	mkdir -p bin
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(INC) $(LIB)

# 编译规则
%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

# 清理
.PHONY: clean
clean:
	-rm -f src/*.o $(TARGET) $(COMMON)/include/*.o

# 安装(可选)
.PHONY: install
install: $(TARGET)
	@echo "Load Balance Agent built successfully!"
	@echo "Run: cd bin && ./lars_lb_agent"
