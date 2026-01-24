CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -pthread
INCLUDES = -I./include
LDFLAGS = -pthread

# Target binary
TARGET = tlshub_bench

# Source files
SRCS = src/main.c src/bench_common.c src/tcp_client.c src/echo_server.c
OBJS = $(SRCS:.c=.o)

# Header files (for dependency tracking)
HEADERS = include/bench_common.h include/tcp_client.h include/echo_server.h

.PHONY: all clean install test help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "Build successful! Binary: $(TARGET)"
	@echo "Run './$(TARGET) --help' for usage information"

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
	rm -f src/*.o
	rm -f bench_metrics_*.json bench_metrics_*.csv
	@echo "Cleaned build artifacts and metrics files"

install:
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "Installed $(TARGET) to /usr/local/bin/"

test: $(TARGET)
	@echo "Running basic functionality test..."
	@echo "Starting echo server in background..."
	@./$(TARGET) --mode server --listen-port 19090 > /tmp/tlshub_bench_server.log 2>&1 &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "Running client test..."; \
	./$(TARGET) --mode client --target-ip 127.0.0.1 --target-port 19090 \
		--data-size 1024 --total-connections 5 --verbose || true; \
	echo "Stopping echo server..."; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	echo "Test completed."

help:
	@echo "TLShub Benchmark Tool - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the tlshub_bench binary (default)"
	@echo "  clean    - Remove build artifacts and metrics files"
	@echo "  install  - Install binary to /usr/local/bin (requires sudo)"
	@echo "  test     - Run basic functionality test"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make              # Build the binary"
	@echo "  make clean        # Clean build artifacts"
	@echo "  sudo make install # Install system-wide"
	@echo "  make test         # Run tests"
	@echo ""
	@echo "After building, run:"
	@echo "  ./$(TARGET) --help"
