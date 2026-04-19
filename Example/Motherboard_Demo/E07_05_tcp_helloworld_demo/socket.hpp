#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

class TcpSocket {
public:
  // 构造函数: 创建 socket
  TcpSocket() : fd_(-1) {
    // AF_INET: IPv4
    // SOCK_STREAM: TCP
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "socket() failed");
    }
    std::cout << "[TcpSocket] socket created, fd=" << fd_ << std::endl;
  }

  // 析构函数: 确保 socket 被关闭
  ~TcpSocket() {
    if (fd_ >= 0) {
      std::cout << "[TcpSocket] closing fd=" << fd_ << std::endl;
      close(fd_);
    }
  }

  // 删除拷贝构造函数和拷贝赋值运算符 (socket 不能共享)
  TcpSocket(const TcpSocket &) = delete;
  TcpSocket &operator=(const TcpSocket &) = delete;

  // 支持移动语义
  TcpSocket(TcpSocket &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  TcpSocket &operator=(TcpSocket &&other) noexcept {
    if (this != &other) {
      if (fd_ >= 0) {
        close(fd_);
      }
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  // 连接到服务器
  // ip: 服务器 IP 地址 (如 "192.168.1.123")
  // port: 服务器端口 (如 1347)
  void connect_to_server(const char *ip, uint16_t port) {
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;            // IPv4
    server_addr.sin_port = htons(port);          // 端口号 (网络字节序)
    server_addr.sin_addr.s_addr = inet_addr(ip); // IP 地址

    // connect() 用于 TCP 客户端连接到服务器
    if (::connect(fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
      throw std::system_error(errno, std::generic_category(),
                              "connect() failed");
    }

    std::cout << "[TcpSocket] connected to " << ip << ":" << port << std::endl;
  }

  // 发送数据
  // data: 要发送的数据缓冲区
  // len: 要发送的数据长度
  // 返回值: 实际发送的字节数
  ssize_t send_data(const void *data, size_t len) {
    ssize_t sent = send(fd_, data, len, 0);
    if (sent < 0) {
      throw std::system_error(errno, std::generic_category(), "send() failed");
    }
    std::cout << "[TcpSocket] sent " << sent << " bytes" << std::endl;
    return sent;
  }

  // 接收数据 (非阻塞)
  // data: 接收缓冲区
  // len: 缓冲区大小
  // 返回值: 实际接收的字节数, 0 表示连接关闭, -1 表示暂无数据
  ssize_t recv_data(void *data, size_t len) {
    ssize_t received = recv(fd_, data, len, 0);
    if (received < 0) {
      // EAGAIN / EWOULDBLOCK 表示非阻塞模式下暂无数据
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
      }
      throw std::system_error(errno, std::generic_category(), "recv() failed");
    }
    std::cout << "[TcpSocket] received " << received << " bytes" << std::endl;
    return received;
  }

  // 获取 socket 文件描述符
  int fd() const { return fd_; }

private:
  int fd_; // socket 文件描述符, -1 表示无效
};