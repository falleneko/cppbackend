#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

namespace net = boost::asio;

class Logger {
    using TimePoint = std::chrono::system_clock::time_point;
    using Strand = net::strand<net::thread_pool::executor_type>;

    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }

        return std::chrono::system_clock::now();
    }

    static std::tm GetLocalTime(TimePoint now) {
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm local_time{};
        localtime_r(&t_c, &local_time);
        return local_time;
    }

    static std::string GetTimeStamp(TimePoint now) {
        const auto local_time = GetLocalTime(now);
        std::ostringstream timestamp;
        timestamp << std::put_time(&local_time, "%F %T");
        return timestamp.str();
    }

    static std::string GetFileTimeStamp(TimePoint now) {
        const auto local_time = GetLocalTime(now);
        std::ostringstream filename;
        filename << "/var/log/sample_log_"
                 << std::put_time(&local_time, "%Y_%m_%d") << ".log";
        return filename.str();
    }

    void Write(const std::string& message) {
        const auto now = GetTime();
        const auto filename = GetFileTimeStamp(now);

        if (!file_.is_open() || filename != current_filename_) {
            if (file_.is_open()) {
                file_.close();
            }
            file_.clear();
            file_.open(filename, std::ios::app);
            current_filename_ = filename;
        }

        file_ << GetTimeStamp(now) << ": " << message << std::endl;
    }

    Logger()
        : task_strand_(net::make_strand(task_pool_)) {
    }

    ~Logger() {
        task_pool_.join();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::ostringstream message;
        (message << ... << args);

        net::post(
            task_strand_,
            [this, text = std::move(message).str()] {
                Write(text);
            }
        );
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        net::post(task_strand_, [this, ts] {
            manual_ts_ = ts;
        });
    }

private:
    net::thread_pool task_pool_{1};
    Strand task_strand_;

    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::ofstream file_;
    std::string current_filename_;
};
