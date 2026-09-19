#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    using TimePoint = std::chrono::system_clock::time_point;

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
        std::ofstream file(filename, std::ios::app);
        file << GetTimeStamp(now) << ": " << message << std::endl;
    }

    Logger() = default;

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

        std::lock_guard lock(mutex_);
        Write(std::move(message).str());
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::mutex mutex_;
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
};
