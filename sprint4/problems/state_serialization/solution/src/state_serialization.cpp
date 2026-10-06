#include "state_serialization.h"

#include "model_serialization.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace serialization {

StateSerializer::StateSerializer(app::Application& application,
                                 std::filesystem::path state_file)
    : application_{application}
    , state_file_{std::move(state_file)} {
    if (state_file_.empty()) {
        throw std::invalid_argument("State file path must not be empty");
    }
}

bool StateSerializer::Restore() const {
    std::error_code error;
    const bool exists = std::filesystem::exists(state_file_, error);
    if (error) {
        throw std::filesystem::filesystem_error(
            "Failed to inspect state file", state_file_, error);
    }
    if (!exists) {
        return false;
    }

    std::ifstream input{state_file_, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Failed to open state file for reading: "
                                 + state_file_.string());
    }

    app::ApplicationState state;
    boost::archive::binary_iarchive archive{input};
    archive >> state;
    application_.RestoreState(std::move(state));
    return true;
}

void StateSerializer::Save() const {
    std::filesystem::path temporary_file = state_file_;
    temporary_file += ".tmp";

    try {
        {
            std::ofstream output{temporary_file,
                                 std::ios::binary | std::ios::trunc};
            if (!output) {
                throw std::runtime_error("Failed to open temporary state file: "
                                         + temporary_file.string());
            }
            boost::archive::binary_oarchive archive{output};
            archive << application_.GetState();
            output.flush();
            if (!output) {
                throw std::runtime_error("Failed to write temporary state file: "
                                         + temporary_file.string());
            }
        }
        std::filesystem::rename(temporary_file, state_file_);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary_file, ignored);
        throw;
    }
}

SerializingListener::SerializingListener(
    const StateSerializer& serializer,
    std::chrono::milliseconds save_period)
    : serializer_{serializer}
    , save_period_{save_period} {
    if (save_period_ <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Save state period must be positive");
    }
}

void SerializingListener::OnTick(std::chrono::milliseconds time_delta) {
    if (time_delta < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Tick duration must not be negative");
    }
    time_since_save_ += time_delta;
    if (time_since_save_ >= save_period_) {
        serializer_.Save();
        time_since_save_ = std::chrono::milliseconds::zero();
    }
}

}  // namespace serialization
