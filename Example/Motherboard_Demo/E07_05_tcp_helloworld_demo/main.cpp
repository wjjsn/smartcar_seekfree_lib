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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "socket.hpp"
// ============================================
// RAII 封装 TCP Socket
// ============================================



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
