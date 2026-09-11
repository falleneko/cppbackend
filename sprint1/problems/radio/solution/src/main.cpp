#include "audio.h"
#include <iostream>
#include <boost/asio.hpp>
#include <array>
#include <string>

namespace net = boost::asio;

using net::ip::udp;
using namespace std::literals;

constexpr std::chrono::duration<long double> voice_duration = 1.5s;
static const int port = 3333;
static const size_t max_buffer_size = 65000;

void server_mode() {
    try {
        boost::asio::io_context io_context;

        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

        // Запускаем сервер в цикле, чтобы можно было работать со многими клиентами
        for (;;) {
            // Создаём буфер достаточного размера, чтобы вместить датаграмму.
            std::array<char, max_buffer_size> recv_buf;
            udp::endpoint remote_endpoint;

            // Получаем не только данные, но и endpoint клиента
            auto size = socket.receive_from(boost::asio::buffer(recv_buf), remote_endpoint);

            Player player(ma_format_u8, 1);
            player.PlayBuffer(recv_buf.data(), size / player.GetFrameSize(), voice_duration);
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void client_mode(const std::string& ip) {
    try {
        Recorder recorder(ma_format_u8, 1);
        auto rec_result = recorder.Record(65000, voice_duration);

        net::io_context io_context;
        udp::socket socket(io_context, udp::v4());

        auto endpoint = udp::endpoint(net::ip::make_address(ip), port);
        const auto recorded_size = rec_result.frames * recorder.GetFrameSize();
        socket.send_to(net::buffer(rec_result.data.data(), recorded_size), endpoint);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc == 1) {
        server_mode();
    } else if (argc == 2) {
        client_mode(argv[1]);
    } else {
        std::cerr << "Usage: " << argv[0] << " [server_ip]" << std::endl;
        return 1;
    }

    return 0;
}
