# Reactor_XDZ 项目整体构建脚本
# 现代C++版本的LARS系统

# 子模块列表
SUBDIRS = lars_reactor lars_dns lars_lb_agent lars_reporter

# 默认目标
.PHONY: all
all: build_reactor build_modules

# 构建lars_reactor库
.PHONY: build_reactor  
build_reactor:
	@echo "=== Building lars_reactor library ==="
	$(MAKE) -C lars_reactor
	@echo "lars_reactor library built successfully!"
	@echo ""

# 构建所有模块
.PHONY: build_modules
build_modules: build_reactor
	@echo "=== Building all LARS modules ==="
	@list='lars_dns lars_lb_agent lars_reporter'; for subdir in $$list; do \
		echo "Building $$subdir..."; \
		$(MAKE) -C $$subdir; \
		echo "$$subdir built successfully!"; \
		echo ""; \
	done
	@echo "All modules built successfully!"

# 单独构建目标
.PHONY: dns
dns: build_reactor
	@echo "=== Building lars_dns only ==="
	$(MAKE) -C lars_dns

.PHONY: agent
agent: build_reactor  
	@echo "=== Building lars_lb_agent only ==="
	$(MAKE) -C lars_lb_agent

.PHONY: reporter
reporter: build_reactor
	@echo "=== Building lars_reporter only ==="
	$(MAKE) -C lars_reporter

# 清理所有
.PHONY: clean
clean:
	@echo "=== Cleaning all modules ==="
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "Cleaning $$subdir..."; \
		$(MAKE) -C $$subdir clean; \
	done
	@echo "All modules cleaned!"

# 安装所有模块
.PHONY: install
install: all
	@echo "=== Installing all modules ==="
	@list='lars_dns lars_lb_agent lars_reporter'; for subdir in $$list; do \
		$(MAKE) -C $$subdir install; \
	done
	@echo "All modules installed!"

# 显示构建信息
.PHONY: info
info:
	@echo "Reactor_XDZ Project - Modern C++ LARS System"
	@echo "============================================="
	@echo "Available targets:"
	@echo "  all        - Build everything"
	@echo "  dns        - Build lars_dns only"
	@echo "  agent      - Build lars_lb_agent only" 
	@echo "  reporter   - Build lars_reporter only"
	@echo "  clean      - Clean all modules"
	@echo "  install    - Install all modules"
	@echo "  info       - Show this information"
	@echo ""
	@echo "Modules:"
	@echo "  lars_reactor   - Network I/O framework (completed)"
	@echo "  lars_dns       - DNS routing service"
	@echo "  lars_lb_agent  - Load balance agent"
	@echo "  lars_reporter  - Status reporting service"
	@echo ""
	@echo "C++ Features Used:"
	@echo "  - Smart pointers (unique_ptr, shared_ptr)"
	@echo "  - STL containers (vector, unordered_map, unordered_set)"
	@echo "  - Modern threading (std::thread, std::mutex, std::condition_variable)"
	@echo "  - RAII and exception safety"
	@echo "  - Lambda expressions and std::function"

# 帮助信息
.PHONY: help
help: info
