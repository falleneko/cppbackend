#pragma once

#include "records.h"

#include <pqxx/connection>

namespace postgres {

class Database final : public app::PlayerRecordRepository {
public:
    explicit Database(pqxx::connection connection);

    void Save(const std::vector<app::PlayerRecord>& records) override;
    std::vector<app::PlayerRecord> Get(std::size_t start,
                                       std::size_t max_items) const override;

private:
    mutable pqxx::connection connection_;
};

}  // namespace postgres
