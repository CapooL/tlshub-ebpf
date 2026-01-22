# TLShub API Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
INCLUDES = -I.

# 目标文件
TEST_TARGET = tlshub_handshake_test
SOURCES = tlshub.c tlshub_handshake.c
HEADERS = tlshub.h

.PHONY: all clean test help

all: $(TEST_TARGET)

$(TEST_TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tlshub_handshake.c tlshub.c

clean:
	rm -f $(TEST_TARGET) *.o

test: $(TEST_TARGET)
	@echo "测试程序已编译: $(TEST_TARGET)"
	@echo "使用方法: sudo ./$(TEST_TARGET) <客户端IP> <服务端IP>"
	@echo "示例: sudo ./$(TEST_TARGET) 10.0.1.100 10.0.2.200"

help:
	@echo "TLShub API - Makefile"
	@echo ""
	@echo "目标："
	@echo "  all      - 编译测试程序（默认）"
	@echo "  clean    - 清理编译产物"
	@echo "  test     - 编译并显示使用说明"
	@echo "  help     - 显示此帮助信息"
	@echo ""
	@echo "使用示例："
	@echo "  make              # 编译测试程序"
	@echo "  make clean        # 清理"
	@echo "  sudo ./tlshub_handshake_test 10.0.1.100 10.0.2.200"
