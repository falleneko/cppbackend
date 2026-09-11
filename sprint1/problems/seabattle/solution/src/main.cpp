#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        PrintFields();

        while (!IsGameEnded()) {
            if (my_initiative) {
                std::optional<std::pair<int, int>> move;
                std::string input;
                while (!move) {
                    if (!(std::cin >> input)) {
                        return;
                    }
                    move = ParseMove(input);
                }

                if (!WriteExact(socket, MoveToString(*move))) {
                    return;
                }

                const auto response = ReadExact<1>(socket);
                if (!response) {
                    return;
                }

                const auto result = static_cast<SeabattleField::ShotResult>((*response)[0]);
                const auto [y, x] = *move;
                switch (result) {
                    case SeabattleField::ShotResult::MISS:
                        other_field_.MarkMiss(x, y);
                        my_initiative = false;
                        break;
                    case SeabattleField::ShotResult::HIT:
                        other_field_.MarkHit(x, y);
                        break;
                    case SeabattleField::ShotResult::KILL:
                        other_field_.MarkKill(x, y);
                        break;
                    default:
                        return;
                }
            } else {
                const auto request = ReadExact<2>(socket);
                if (!request) {
                    return;
                }

                const auto move = ParseMove(*request);
                if (!move) {
                    return;
                }

                const auto [y, x] = *move;
                const auto result = my_field_.Shoot(x, y);
                const char response = static_cast<char>(result);
                if (!WriteExact(socket, std::string_view(&response, 1))) {
                    return;
                }
                my_initiative = result == SeabattleField::ShotResult::MISS;
            }

            PrintFields();
        }
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 > 8) return std::nullopt;
        if (p2 < 0 || p2 > 8) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first) + 'A', static_cast<char>(move.second) + '1'};
        return {buff, 2};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    // TODO: добавьте методы по вашему желанию

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);

    net::io_context io_context;
    boost::system::error_code ec;

    tcp::acceptor acceptor{io_context, tcp::endpoint(tcp::v4(), port)};
    tcp::socket socket{io_context};
    acceptor.accept(socket, ec);
    if (ec) {
        throw std::runtime_error("Can't accept connection: " + ec.message());
    }

    agent.StartGame(socket, false);
};

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);

    boost::system::error_code ec;
    const auto address = net::ip::make_address(ip_str, ec);
    if (ec) {
        throw std::runtime_error("Unknown IP: " + ec.message());
    }

    net::io_context io_context;
    tcp::socket socket{io_context};
    socket.connect(tcp::endpoint{address, port}, ec);
    if (ec) {
        throw std::runtime_error("Can't connect to server: " + ec.message());
    }

    agent.StartGame(socket, true);
};

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
}
