#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "api_handler.h"
#include "app.h"
#include "json_loader.h"
#include "logger.h"
#include "postgres.h"
#include "request_handler.h"
#include "state_serialization.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
namespace po = boost::program_options;

namespace {

constexpr const char DB_URL_ENV_NAME[]{"GAME_DB_URL"};

std::string GetDatabaseUrl() {
    if (const char* url = std::getenv(DB_URL_ENV_NAME)) {
        return url;
    }
    throw std::runtime_error(std::string{DB_URL_ENV_NAME}
                             + " environment variable not found");
}

struct Args {
    std::string config_file;
    std::string www_root;
    std::optional<std::chrono::milliseconds> tick_period;
    std::optional<std::filesystem::path> state_file;
    std::optional<std::chrono::milliseconds> save_state_period;
    bool randomize_spawn_points = false;
};

std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    Args args;
    std::int64_t tick_period = 0;
    std::int64_t save_state_period = 0;
    std::string state_file;

    po::options_description options{"Allowed options"};
    options.add_options()
        ("help,h", "produce help message")
        ("tick-period,t",
         po::value<std::int64_t>(&tick_period)->value_name("milliseconds"),
         "set tick period")
        ("config-file,c",
         po::value<std::string>(&args.config_file)->value_name("file")->required(),
         "set config file path")
        ("www-root,w",
         po::value<std::string>(&args.www_root)->value_name("dir")->required(),
         "set static files root")
        ("state-file",
         po::value<std::string>(&state_file)->value_name("file"),
         "set game state file path")
        ("save-state-period",
         po::value<std::int64_t>(&save_state_period)->value_name("milliseconds"),
         "set automatic game state save period")
        ("randomize-spawn-points",
         po::bool_switch(&args.randomize_spawn_points),
         "spawn dogs at random positions");

    po::variables_map variables;
    po::store(po::parse_command_line(argc, argv, options), variables);
    if (variables.contains("help")) {
        std::cout << options << std::endl;
        return std::nullopt;
    }
    po::notify(variables);

    if (variables.contains("tick-period")) {
        if (tick_period <= 0) {
            throw po::validation_error(
                po::validation_error::invalid_option_value, "tick-period");
        }
        args.tick_period = std::chrono::milliseconds{tick_period};
    }
    if (variables.contains("state-file")) {
        if (state_file.empty()) {
            throw po::validation_error(
                po::validation_error::invalid_option_value, "state-file");
        }
        args.state_file = std::filesystem::path{std::move(state_file)};
    }
    if (args.state_file && variables.contains("save-state-period")) {
        if (save_state_period <= 0) {
            throw po::validation_error(
                po::validation_error::invalid_option_value,
                "save-state-period");
        }
        args.save_state_period = std::chrono::milliseconds{save_state_period};
    }
    return args;
}

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    std::optional<Args> args;
    try {
        args = ParseCommandLine(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    if (!args) {
        return EXIT_SUCCESS;
    }

    app_logging::InitBoostLog();
    try {
        // 1. Загружаем карту из файла и построить модель игры
        extra_data::MapLootTypes loot_types;
        model::Game game = json_loader::LoadGame(args->config_file, &loot_types);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals{ioc, SIGINT, SIGTERM};
        signals.async_wait([&ioc](const boost::system::error_code& ec, int) {
            if (!ec) {
                ioc.stop();
            }
        });

        // 4. Связываем HTTP-слой с моделью через фасад приложения
        postgres::Database database{pqxx::connection{GetDatabaseUrl()}};
        app::Application application{game, args->randomize_spawn_points,
                                     &database};
        std::unique_ptr<serialization::StateSerializer> state_serializer;
        std::unique_ptr<serialization::SerializingListener> serializing_listener;
        if (args->state_file) {
            state_serializer = std::make_unique<serialization::StateSerializer>(
                application, *args->state_file);
            state_serializer->Restore();
            if (args->save_state_period) {
                serializing_listener
                    = std::make_unique<serialization::SerializingListener>(
                        *state_serializer, *args->save_state_period);
                application.SetListener(serializing_listener.get());
            }
        }
        http_handler::ApiHandler api_handler{application, !args->tick_period,
                                             &loot_types};
        auto api_strand = net::make_strand(ioc);
        http_handler::RequestHandler handler{api_handler, std::move(args->www_root),
                                             api_strand};
        http_handler::LoggingRequestHandler logging_handler{handler};

        std::shared_ptr<app::Ticker> ticker;
        if (args->tick_period) {
            ticker = std::make_shared<app::Ticker>(
                api_strand, *args->tick_period,
                [&application](std::chrono::milliseconds delta) {
                    application.Tick(delta);
                });
            ticker->Start();
        }

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        app_logging::WriteLog(
            "server started"sv,
            {{"port", port}, {"address", address.to_string()}}
        );

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        if (state_serializer) {
            state_serializer->Save();
        }
        app_logging::WriteLog("server exited"sv, {{"code", 0}});
    } catch (const std::exception& ex) {
        app_logging::WriteLog(
            "server exited"sv,
            {{"code", EXIT_FAILURE}, {"exception", ex.what()}}
        );
        return EXIT_FAILURE;
    }
}
