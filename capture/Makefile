CC = gcc
CLANG = clang
CFLAGS = -Wall -Wextra -O2 -g
INCLUDES = -I./include -I/usr/include
LDFLAGS = -lbpf -lssl -lcrypto

# 目标文件
TARGET = capture
BPF_OBJ = capture.bpf.o
SRCS = src/main.c src/pod_mapping.c src/tlshub_client.c src/ktls_config.c src/key_provider.c src/performance_metrics.c
OBJS = $(SRCS:.c=.o)

# eBPF 编译选项
BPF_CFLAGS = -target bpf -D__TARGET_ARCH_x86_64 -O2 -g -Wall -I/usr/include/x86_64-linux-gnu

.PHONY: all clean install

all: $(TARGET) $(BPF_OBJ)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BPF_OBJ): src/capture.bpf.c
	$(CLANG) $(BPF_CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(TARGET) $(BPF_OBJ) $(OBJS)
	rm -f src/*.o

install:
	install -d /etc/tlshub
	install -m 644 config/capture.conf /etc/tlshub/
	install -m 644 config/pod_node_mapping.conf /etc/tlshub/
	install -m 755 $(TARGET) /usr/local/bin/
	install -m 644 $(BPF_OBJ) /usr/local/lib/

help:
	@echo "TLShub Traffic Capture Module - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the capture module and eBPF program"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install binaries and configuration files"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - libbpf"
	@echo "  - OpenSSL/BoringSSL"
	@echo "  - Kernel with eBPF and KTLS support"
