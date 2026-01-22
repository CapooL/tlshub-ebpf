# TLSHub eBPF Makefile
# 用于编译 eBPF 内核程序和用户空间程序

CC := gcc
CLANG := clang
LLVM_STRIP := $(shell which llvm-strip 2>/dev/null || which llvm-strip-18 2>/dev/null || which llvm-strip-17 2>/dev/null || which llvm-strip-16 2>/dev/null || echo llvm-strip)

# 目录定义
SRC_DIR := src
USER_DIR := userspace/src
INCLUDE_DIR := include
BUILD_DIR := build
OUTPUT_DIR := bin

# 内核头文件路径
KERNEL_VERSION := $(shell uname -r)
KERNEL_HEADERS := /usr/src/linux-headers-$(KERNEL_VERSION)

# 编译选项
INCLUDES := -I$(INCLUDE_DIR) -I/usr/include
CFLAGS := -O2 -g -Wall $(INCLUDES)
BPF_CFLAGS := -target bpf -D__TARGET_ARCH_x86 -O2 -Wall $(INCLUDES)
LDFLAGS := -lbpf -lelf -lz

# 目标文件
KERN_OBJ := $(BUILD_DIR)/tlshub_kern.o
USER_BIN := $(OUTPUT_DIR)/tlshub

# 默认目标
.PHONY: all
all: $(KERN_OBJ) $(USER_BIN)

# 创建必要的目录
$(BUILD_DIR) $(OUTPUT_DIR):
	mkdir -p $@

# 编译 eBPF 内核程序
$(KERN_OBJ): $(SRC_DIR)/tlshub_kern.c $(INCLUDE_DIR)/tlshub_common.h | $(BUILD_DIR)
	@echo "编译 eBPF 内核程序..."
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@
	$(LLVM_STRIP) -g $@
	@echo "eBPF 内核程序编译完成: $@"

# 编译用户空间程序
$(USER_BIN): $(USER_DIR)/tlshub_user.c $(INCLUDE_DIR)/tlshub_common.h | $(OUTPUT_DIR)
	@echo "编译用户空间程序..."
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
	@echo "用户空间程序编译完成: $@"

# 清理
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "清理完成"

# 安装（需要 root 权限）
.PHONY: install
install: all
	@echo "安装 TLSHub eBPF..."
	install -d /usr/local/bin
	install -m 755 $(USER_BIN) /usr/local/bin/
	install -d /usr/local/lib/tlshub
	install -m 644 $(KERN_OBJ) /usr/local/lib/tlshub/
	@echo "安装完成"

# 卸载
.PHONY: uninstall
uninstall:
	@echo "卸载 TLSHub eBPF..."
	rm -f /usr/local/bin/tlshub
	rm -rf /usr/local/lib/tlshub
	@echo "卸载完成"

# 帮助信息
.PHONY: help
help:
	@echo "TLSHub eBPF 构建系统"
	@echo ""
	@echo "可用目标:"
	@echo "  make           - 编译所有组件"
	@echo "  make clean     - 清理构建文件"
	@echo "  make install   - 安装到系统 (需要 root)"
	@echo "  make uninstall - 从系统卸载 (需要 root)"
	@echo "  make help      - 显示此帮助信息"
