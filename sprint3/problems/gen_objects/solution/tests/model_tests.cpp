#include <catch2/catch_test_macros.hpp>

#include "../src/app.h"

#include <chrono>

using namespace std::chrono_literals;

namespace {

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
