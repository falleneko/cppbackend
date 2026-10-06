#include "postgres.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <pqxx/transaction>
#include <pqxx/result>
#include <pqxx/row>
#include <pqxx/zview.hxx>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace postgres {

using pqxx::operator"" _zv;

namespace {

std::int64_t ToDatabaseInteger(std::size_t value) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::out_of_range("Records query parameter is too large");
    }
    return static_cast<std::int64_t>(value);
}

}  // namespace

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work transaction{connection_};
    transaction.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    score BIGINT NOT NULL,
    play_time_ms BIGINT NOT NULL
);
)"_zv);
    transaction.commit();
}

void Database::Save(const std::vector<app::PlayerRecord>& records) {
    if (records.empty()) {
        return;
    }

    pqxx::work transaction{connection_};
    boost::uuids::random_generator generate_uuid;
    for (const app::PlayerRecord& record : records) {
        if (record.score
            > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            throw std::out_of_range("Player score is too large");
        }
        transaction.exec_params(
            R"(
INSERT INTO retired_players (id, name, score, play_time_ms)
VALUES ($1, $2, $3, $4);
)"_zv,
            boost::uuids::to_string(generate_uuid()), record.name,
            static_cast<std::int64_t>(record.score), record.play_time.count());
    }
    transaction.commit();
}

std::vector<app::PlayerRecord> Database::Get(std::size_t start,
                                             std::size_t max_items) const {
    pqxx::read_transaction transaction{connection_};
    const auto rows = transaction.exec_params(
        R"(
SELECT name, score, play_time_ms
FROM retired_players
ORDER BY score DESC, play_time_ms ASC, name ASC
OFFSET $1 LIMIT $2;
)"_zv,
        ToDatabaseInteger(start), ToDatabaseInteger(max_items));

    std::vector<app::PlayerRecord> records;
    records.reserve(rows.size());
    for (const auto row : rows) {
        records.push_back({
            row[0].as<std::string>(),
            static_cast<std::uint64_t>(row[1].as<std::int64_t>()),
            std::chrono::milliseconds{row[2].as<std::int64_t>()},
        });
    }
    return records;
}

}  // namespace postgres
