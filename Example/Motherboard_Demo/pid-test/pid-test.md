# pid-test 开发问题总结

## 目标

为 pid-test 程序添加 TCP 参数热更新功能，使程序能够：
1. 通过 TCP 连接到 `192.168.1.123:1347`
2. 接收 PID 参数（kp、ki、kd、left_speed、right_speed、left_duty、right_duty）
3. 实时热更新 PID 控制参数

---

## 遇到的问题及解决方案

### 1. LoongArch 架构内存顺序问题（程序崩溃）

**问题描述**：
程序在运行时崩溃，出现在 `encoder_get_count` 调用时。

**根因**：
在 LoongArch 架构上，`encoder_get_count` 必须在 PID 对象创建之前调用，否则可能因内存顺序问题导致崩溃。

**解决方案**：
调整代码顺序，确保 `encoder_get_count` 在任何 PID 对象创建之前被调用。

---

### 2. PID 构造函数参数顺序错误

**问题描述**：
PID 输出始终为负值或零，电机无法正常转动。

**根因**：
PID 构造函数参数顺序错误：
```cpp
// 错误
PID left(100, pid_kp_func, 0, 0, 10000, -10000);

// 正确（output_min, output_max）
PID left(100, pid_kp_func, 0, 0, -10000, 10000);
```

**解决方案**：
修正 `output_min` 和 `output_max` 的顺序，确保负值在前、正值在后。

---

### 3. TCP 服务器不可达导致 connect 阻塞

**问题描述**：
程序在 `connect` 调用时阻塞，无法继续执行。

**根因**：
`192.168.1.123:1347` 服务器不可达时，阻塞模式的 `connect` 会一直等待。

**解决方案**：
使用 `O_NONBLOCK` 设置非阻塞模式：
```cpp
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
int flags = fcntl(sockfd, F_GETFL, 0);
fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
```

---

### 4. recv 阻塞导致无法处理其他任务

**问题描述**：
使用阻塞 `recv` 时，如果没有数据到达，程序会一直等待，无法执行 encoder 读取和 PID 控制。

**解决方案**：
使用 `poll` 实现带超时的轮询：
```cpp
struct pollfd pfd = {sockfd, POLLIN, 0};
if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
    ssize_t len = recv(sockfd, buf, sizeof(buf) - 1, 0);
    // 处理数据
}
```

---

### 5. 数据解析格式不匹配

**问题描述**：
服务端发送的数据格式为 `kd:394`，但解析代码使用 `%31[^=]=%f`。

**解决方案**：
修改 `sscanf` 格式字符串以匹配冒号分隔格式：
```cpp
// 从
if (sscanf(buf, "%31[^=]=%f", key, &value) == 2)
// 改为
if (sscanf(buf, "%31[^:]:%f", key, &value) == 2)
```

---

### 6. 多线程复杂度问题

**问题描述**：
最初使用 `std::thread` + `std::mutex` + `std::atomic` 实现 TCP 接收和 PID 参数更新，导致代码复杂且容易出错。

**解决方案**：
简化为单线程，使用 `poll` 轮询 TCP socket + 主循环处理 PID 控制，消除线程同步需求。

---

### 7. TCP 连接关闭后 recv 返回 0 时的处理

**问题描述**：
TCP 服务器关闭连接后，`recv` 返回 0，但程序继续循环调用 `recv`，导致 `ENOTSOCK` 错误。

**解决方案**：
正确处理 `recv` 返回 0 的情况：
```cpp
if (len > 0) {
    // 处理数据
} else if (len < 0) {
    perror("recv error");
    break;
} else {
    // len == 0 表示对方关闭连接
    printf("server closed connection\n");
    break;
}
```

---

### 8. sockfd 在循环中被意外改变（当前未解决）

**问题描述**：
`sockfd` 在两次 `recv` 调用之间从 3 变成 0，导致 `ENOTSOCK` 错误。

**调试输出**：
```
DEBUG BEFORE RECV: loop=1 sockfd=3
DEBUG AFTER RECV: loop=1 sockfd=3 len=7 errno=0
recv_count:7 buf:kd:846
set kd=846.00
DEBUG BEFORE RECV: loop=1 sockfd=0
DEBUG AFTER RECV: loop=1 sockfd=0 len=-1 errno=88
recv error at loop 1: Socket operation on non-socket (sockfd=0)
```

**分析**：
- 两次 `recv` 调用之间，`sockfd` 从 3 变成 0
- `loop_count` 保持为 1
- `errno=88` 对应 `ENOTSOCK`
- 代码中没有显式关闭或修改 `sockfd`
- 怀疑是内存覆写问题（`zf_common_headfile.h` 包含大量驱动，可能有内存越界操作）

**尝试的解决方案**：
将 `sockfd` 声明为 `static` 以防止被意外覆写：
```cpp
static int sockfd = -1;
sockfd = socket(AF_INET, SOCK_STREAM, 0);
```

---

## 开发过程中的设计决策

### 从多线程改为单线程

最初设计使用独立线程接收 TCP 数据，主线程处理 PID 控制。但发现：
- 需要复杂的 mutex 保护 PID 参数
- `zf_common_headfile.h` 中的驱动可能与多线程不兼容
- 单线程 + `poll` 轮询足以满足需求

### 数据格式

服务端发送 `key:value` 格式（如 `kd:394`），需要使用 `%31[^:]:%f` 解析。

### PID 参数热更新

参数通过全局变量 `g_kp`、`g_ki`、`g_kd` 等存储，在主循环中直接读取并更新 PID 对象。

---

## 当前状态

程序可以成功：
1. 连接到 TCP 服务器
2. 接收并解析数据（如 `kd:846`）
3. 更新 PID 参数

待解决问题：
- **sockfd 内存覆写问题**：sockfd 在循环中被意外改变，需要进一步调试

---

## 相关文件

- `Example/Motherboard_Demo/pid-test/main.cpp` - 主程序
- `Example/Motherboard_Demo/pid-test/pid.hpp` - PID 控制器
- `Example/Motherboard_Demo/pid-test/socket.hpp` - TCP socket 封装（已弃用）
- `Example/Motherboard_Demo/E07_05_tcp_helloworld_demo/main.cpp` - TCP 参考实现
