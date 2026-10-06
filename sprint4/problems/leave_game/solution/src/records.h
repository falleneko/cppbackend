#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace app {

struct PlayerRecord {
    std::string name;
    std::uint64_t score = 0;
    std::chrono::milliseconds play_time{0};
};

class PlayerRecordRepository {
public:
    virtual void Save(const std::vector<PlayerRecord>& records) = 0;
    virtual std::vector<PlayerRecord> Get(std::size_t start,
                                          std::size_t max_items) const = 0;

    virtual ~PlayerRecordRepository() = default;
};

}  // namespace app
