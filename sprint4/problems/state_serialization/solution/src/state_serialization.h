#pragma once

#include "app.h"

#include <chrono>
#include <filesystem>

namespace serialization {

class StateSerializer {
public:
    StateSerializer(app::Application& application, std::filesystem::path state_file);

    bool Restore() const;
    void Save() const;

private:
    app::Application& application_;
    std::filesystem::path state_file_;
};

class SerializingListener final : public app::ApplicationListener {
public:
    SerializingListener(const StateSerializer& serializer,
                        std::chrono::milliseconds save_period);

    void OnTick(std::chrono::milliseconds time_delta) override;

private:
    const StateSerializer& serializer_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds time_since_save_{};
};

}  // namespace serialization
