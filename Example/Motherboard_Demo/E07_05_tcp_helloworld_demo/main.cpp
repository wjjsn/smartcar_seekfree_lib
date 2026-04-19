/*
 * TCP 客户端示例 - 发送 Hello World
 *
 * 本例程演示如何使用 RAII 风格的 TCP 客户端连接到服务器并发送数据。
 * 使用智能指针和 RAII 封装确保socket资源正确释放。
 *
 * 服务器地址: 192.168.1.123
 * 端口: 1347
 */

#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <system_error>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// ============================================
// RAII 封装 TCP Socket
// ============================================

class TcpSocket {
public:
    // 构造函数: 创建 socket
    TcpSocket() : fd_(-1) {
        // AF_INET: IPv4
        // SOCK_STREAM: TCP
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            throw std::system_error(errno, std::generic_category(), "socket() failed");
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
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // 支持移动语义
    TcpSocket(TcpSocket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    TcpSocket& operator=(TcpSocket&& other) noexcept {
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
    void connect_to_server(const char* ip, uint16_t port) {
        sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin_family = AF_INET;              // IPv4
        server_addr.sin_port = htons(port);             // 端口号 (网络字节序)
        server_addr.sin_addr.s_addr = inet_addr(ip);    // IP 地址

        // connect() 用于 TCP 客户端连接到服务器
        if (::connect(fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::system_error(errno, std::generic_category(), "connect() failed");
        }

        std::cout << "[TcpSocket] connected to " << ip << ":" << port << std::endl;
    }

    // 发送数据
    // data: 要发送的数据缓冲区
    // len: 要发送的数据长度
    // 返回值: 实际发送的字节数
    ssize_t send_data(const void* data, size_t len) {
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
    ssize_t recv_data(void* data, size_t len) {
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
    int fd_;  // socket 文件描述符, -1 表示无效
};

// ============================================
// 主函数
// ============================================

int main() {
    try {
        // -------------------------------------------
        // 第一步: 创建 TCP Socket (RAII 自动管理资源)
        // -------------------------------------------
        // TcpSocket 对象在构造时自动创建 socket
        // 当 TcpSocket 对象离开作用域时, 析构函数会自动关闭 socket
        // 即使发生异常, RAII 也能保证资源被正确释放
        auto tcp_socket = std::make_unique<TcpSocket>();

        // -------------------------------------------
        // 第二步: 连接到服务器
        // -------------------------------------------
        // 服务器地址: 192.168.1.123
        // 端口: 1347
        const char* server_ip = "192.168.1.123";
        uint16_t server_port = 1347;

        tcp_socket->connect_to_server(server_ip, server_port);

        // -------------------------------------------
        // 第三步: 发送 Hello World
        // -------------------------------------------
        std::string message = "Hello World";
        tcp_socket->send_data(message.c_str(), message.size());

        // -------------------------------------------
        // 第四步: 可选 - 接收服务器响应
        // -------------------------------------------
        // 如果服务器有数据返回, 可以在这里接收
        // 本例程只是简单发送后退出
        char recv_buffer[256] = {0};
        ssize_t recv_len = tcp_socket->recv_data(recv_buffer, sizeof(recv_buffer) - 1);
        if (recv_len > 0) {
            std::cout << "[Main] server response: " << recv_buffer << std::endl;
        }

        // -------------------------------------------
        // 第五步: 程序结束
        // -------------------------------------------
        // TcpSocket 的 unique_ptr 离开作用域
        // 析构函数自动关闭 socket, 无需手动调用 close()
        std::cout << "[Main] done, connection will be closed automatically" << std::endl;

    } catch (const std::exception& e) {
        // 捕获并打印错误
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
