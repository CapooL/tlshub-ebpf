# libbpf 版本兼容性问题及解决方案

## 问题描述

在编译 capture 模块时，如果使用旧版本的 libbpf (例如 Ubuntu 22.04 LTS 默认的 libbpf 0.5.0)，会出现以下编译错误：

```
src/main.c:307:10: error: too many arguments to function 'perf_buffer__new'
  307 |     pb = perf_buffer__new(events_fd, 8, handle_tcp_event, NULL, NULL, NULL);
      |          ^~~~~~~~~~~~~~~~
In file included from src/main.c:7:
/usr/include/bpf/libbpf.h:682:1: note: declared here
  682 | perf_buffer__new(int map_fd, size_t page_cnt,
      | ^~~~~~~~~~~~~~~~
make: *** [Makefile:24: src/main.o] Error 1
```

## 根本原因分析

这是由于 **libbpf 库版本变化导致的 API 不兼容问题**，而不是代码编写错误。

### libbpf 0.x 版本 (旧 API)

在 libbpf 0.x 版本中，`perf_buffer__new` 函数的签名为：

```c
struct perf_buffer *
perf_buffer__new(int map_fd, size_t page_cnt,
                 const struct perf_buffer_opts *opts);
```

其中回调函数和上下文通过 `perf_buffer_opts` 结构体传递：

```c
struct perf_buffer_opts {
    perf_buffer_sample_fn sample_cb;  /* 数据回调函数 */
    perf_buffer_lost_fn lost_cb;      /* 丢失数据回调函数 */
    void *ctx;                         /* 用户上下文 */
};
```

**使用示例：**

```c
struct perf_buffer_opts pb_opts = {
    .sample_cb = handle_tcp_event,
    .lost_cb = NULL,
    .ctx = NULL,
};
pb = perf_buffer__new(events_fd, 8, &pb_opts);
```

### libbpf 1.0+ 版本 (新 API)

从 libbpf 1.0 开始，`perf_buffer__new` 函数的签名变更为：

```c
struct perf_buffer *
perf_buffer__new(int map_fd, size_t page_cnt,
                 perf_buffer_sample_fn sample_cb,
                 perf_buffer_lost_fn lost_cb,
                 void *ctx,
                 const struct perf_buffer_opts *opts);
```

回调函数和上下文现在作为独立参数直接传递，`perf_buffer_opts` 结构体仅用于传递额外的高级选项。

**使用示例：**

```c
pb = perf_buffer__new(events_fd, 8, handle_tcp_event, NULL, NULL, NULL);
```

## 解决方案

我们在代码中实现了 **编译时版本检测机制**，使用条件编译来支持两种不同的 API。

### 实现细节

#### 1. 包含版本头文件

```c
/* Try to include libbpf_version.h for version detection */
#ifdef __has_include
  #if __has_include(<bpf/libbpf_version.h>)
    #include <bpf/libbpf_version.h>
  #endif
#endif
```

- `libbpf_version.h` 头文件在 libbpf 1.0+ 中引入，包含 `LIBBPF_MAJOR_VERSION` 和 `LIBBPF_MINOR_VERSION` 宏
- 使用 `__has_include` 检查头文件是否存在，避免在旧版本中编译失败

#### 2. 条件编译调用正确的 API

```c
/* 根据 libbpf 版本使用不同的 API */
#if defined(LIBBPF_MAJOR_VERSION) && LIBBPF_MAJOR_VERSION >= 1
    /* libbpf 1.0+ API: 回调函数作为独立参数传递 */
    pb = perf_buffer__new(events_fd, 8, handle_tcp_event, NULL, NULL, NULL);
#else
    /* libbpf 0.x API: 回调函数通过 opts 结构体传递 */
    struct perf_buffer_opts pb_opts = {
        .sample_cb = handle_tcp_event,
        .lost_cb = NULL,
        .ctx = NULL,
    };
    pb = perf_buffer__new(events_fd, 8, &pb_opts);
#endif
```

### 兼容性

此解决方案支持：

- ✅ **libbpf 0.5.0** (Ubuntu 22.04 LTS 默认版本)
- ✅ **libbpf 1.0+** (包括 Ubuntu 24.04 及更高版本)
- ✅ **自行编译的 libbpf** (任何版本)

## 验证

### 在 Ubuntu 22.04 LTS (libbpf 0.5.0) 上编译

```bash
cd capture
make clean
make
```

应成功编译，不会出现 "too many arguments" 错误。

### 在 Ubuntu 24.04 LTS (libbpf 1.3.0+) 上编译

```bash
cd capture
make clean
make
```

应成功编译，使用新 API。

## 相关信息

### 查看当前 libbpf 版本

```bash
# Debian/Ubuntu 系统
dpkg -l | grep libbpf

# 或者检查头文件中的版本宏
cat /usr/include/bpf/libbpf_version.h
```

### libbpf 版本历史

- **libbpf 0.x**: 传统 API，回调通过结构体传递
- **libbpf 1.0** (2022): API 重构，回调作为函数参数
- **libbpf 1.1+**: 持续改进和新特性

### 参考链接

- [libbpf GitHub 仓库](https://github.com/libbpf/libbpf)
- [libbpf API 文档](https://libbpf.readthedocs.io/)
- [libbpf v1.0 更新日志](https://github.com/libbpf/libbpf/releases/tag/v1.0.0)

## 总结

这是一个典型的库版本演进导致的 API 不兼容问题。通过使用条件编译，我们的代码能够在编译时自动适配不同版本的 libbpf，无需用户手动修改代码或配置，确保了在不同 Linux 发行版上的良好兼容性。
