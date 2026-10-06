#include <catch2/catch_test_macros.hpp>

#include "../src/app.h"

#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

namespace {

class MemoryRecords final : public app::PlayerRecordRepository {
public:
    void Save(const std::vector<app::PlayerRecord>& new_records) override {
        records.insert(records.end(), new_records.begin(), new_records.end());
    }

    std::vector<app::PlayerRecord> Get(std::size_t start,
                                       std::size_t max_items) const override {
        if (start >= records.size()) {
            return {};
        }
        const auto first = records.begin() + static_cast<std::ptrdiff_t>(start);
        const auto count = std::min(max_items,
                                    static_cast<std::size_t>(records.end() - first));
        return {first, first + static_cast<std::ptrdiff_t>(count)};
    }

    std::vector<app::PlayerRecord> records;
};

model::Map MakeMap(std::string id, std::size_t loot_type_count) {
    model::Map map{model::Map::Id{std::move(id)}, "Test map"};
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 2}, 10});
    map.AddRoad(model::Road{model::Road::VERTICAL, {5, -4}, 4});
    map.SetLootTypeCount(loot_type_count);
    return map;
}

bool IsOnRoad(model::Position position) {
    return (position.y == 2.0 && position.x >= 0.0 && position.x <= 10.0) ||
           (position.x == 5.0 && position.y >= -4.0 && position.y <= 4.0);
}

}  // namespace

TEST_CASE("Lost objects are generated on roads with valid types") {
    model::Game game;
    game.SetLootGeneratorConfig({1s, 1.0});
    game.AddMap(MakeMap("first", 3));
    app::Application application{game};

    const model::Map::Id map_id{"first"};
    application.JoinGame("dog one", map_id);
    application.JoinGame("dog two", map_id);
    application.JoinGame("dog three", map_id);

    application.Tick(1s);
    const auto& objects = application.GetLostObjectsOnMap(map_id);
    REQUIRE(objects.size() == 3);
    for (std::size_t index = 0; index < objects.size(); ++index) {
        CHECK(objects[index].id == index);
        CHECK(objects[index].type < 3);
        CHECK(IsOnRoad(objects[index].position));
    }

    application.Tick(1s);
    CHECK(application.GetLostObjectsOnMap(map_id).size() == 3);
}

TEST_CASE("Lost objects belong only to their map") {
    model::Game game;
    game.SetLootGeneratorConfig({1s, 1.0});
    game.AddMap(MakeMap("first", 1));
    game.AddMap(MakeMap("second", 2));
    app::Application application{game};

    const model::Map::Id first{"first"};
    const model::Map::Id second{"second"};
    application.JoinGame("first dog", first);
    application.Tick(1s);
    CHECK(application.GetLostObjectsOnMap(first).size() == 1);
    CHECK(application.GetLostObjectsOnMap(second).empty());

    application.JoinGame("second dog", second);
    application.Tick(1s);
    REQUIRE(application.GetLostObjectsOnMap(second).size() == 1);
    CHECK(application.GetLostObjectsOnMap(second).front().id == 0);
    CHECK(application.GetLostObjectsOnMap(second).front().type < 2);
    CHECK(application.GetLostObjectsOnMap(first).size() == 1);
}

TEST_CASE("No lost objects appear without elapsed time") {
    model::Game game;
    game.SetLootGeneratorConfig({1s, 1.0});
    game.AddMap(MakeMap("first", 1));
    app::Application application{game};
    const model::Map::Id map_id{"first"};
    application.JoinGame("dog", map_id);
    application.Tick(0ms);
    CHECK(application.GetLostObjectsOnMap(map_id).empty());
}

TEST_CASE("A moving player collects a lost object") {
    model::Game game;
    game.SetLootGeneratorConfig({1s, 1.0});
    game.AddMap(MakeMap("first", 1));
    app::Application application{game};
    const model::Map::Id map_id{"first"};
    auto [player, token] = application.JoinGame("dog", map_id);

    application.Tick(1s);
    REQUIRE(application.GetLostObjectsOnMap(map_id).size() == 1);

    player.Move(model::Direction::EAST);
    application.Tick(10s);

    REQUIRE(player.GetBag().size() == 1);
    CHECK(player.GetBag().front().id == 0);
    CHECK(player.GetBag().front().type == 0);
    CHECK(application.GetLostObjectsOnMap(map_id).front().id != 0);
}

TEST_CASE("A player returns collected objects at an office") {
    model::Game game;
    game.SetLootGeneratorConfig({1s, 1.0});
    model::Map map{model::Map::Id{"first"}, "Test map", 1.0, 1};
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 10});
    map.AddOffice(model::Office{model::Office::Id{"office"}, {10, 0}, {0, 0}});
    map.SetLootTypeCount(1);
    game.AddMap(std::move(map));

    app::Application application{game};
    const model::Map::Id map_id{"first"};
    auto [player, token] = application.JoinGame("dog", map_id);
    application.Tick(1s);

    player.Move(model::Direction::EAST);
    application.Tick(10s);

    CHECK(player.GetBag().empty());
    REQUIRE(application.GetLostObjectsOnMap(map_id).size() == 1);
    CHECK(application.GetLostObjectsOnMap(map_id).front().id == 1);
}

TEST_CASE("A bag preserves collection order and respects map capacity") {
    auto map = std::make_shared<model::Map>(
        model::Map::Id{"first"}, "Test map", 1.0, 2);
    map->SetLootValues({10, 20, 30});
    app::Player player{app::Player::Id{0}, "dog", {0.0, 0.0}, map};

    CHECK(player.GetScore() == 0);
    CHECK(player.TryAddToBag({7, 1, {1.0, 0.0}}));
    CHECK(player.TryAddToBag({3, 2, {2.0, 0.0}}));
    CHECK_FALSE(player.TryAddToBag({9, 0, {3.0, 0.0}}));
    REQUIRE(player.GetBag().size() == 2);
    CHECK(player.GetBag()[0].id == 7);
    CHECK(player.GetBag()[1].id == 3);

    player.ReturnLoot();
    CHECK(player.GetBag().empty());
    CHECK(player.GetScore() == 50);

    CHECK(player.TryAddToBag({11, 0, {3.0, 0.0}}));
    player.ReturnLoot();
    CHECK(player.GetScore() == 60);
}

TEST_CASE("An idle player retires and its token is invalidated") {
    model::Game game;
    game.SetLootGeneratorConfig({1s, 0.0});
    game.SetDogRetirementTime(1500ms);
    game.AddMap(MakeMap("first", 1));
    MemoryRecords records;
    app::Application application{game, false, &records};

    const auto [player, token] = application.JoinGame(
        "retiring dog", model::Map::Id{"first"});
    application.Tick(1499ms);
    REQUIRE(application.FindPlayerByToken(token) == &player);
    CHECK(records.records.empty());

    application.Tick(1ms);
    CHECK(application.FindPlayerByToken(token) == nullptr);
    REQUIRE(records.records.size() == 1);
    CHECK(records.records.front().name == "retiring dog");
    CHECK(records.records.front().score == 0);
    CHECK(records.records.front().play_time == 1500ms);
}
