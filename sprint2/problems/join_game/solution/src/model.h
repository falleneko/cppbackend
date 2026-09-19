#pragma once
#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {};

    struct VerticalTag {};

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept;
    Road(VerticalTag, Point start, Coord end_y) noexcept;

    bool IsHorizontal() const noexcept;
    bool IsVertical() const noexcept;
    Point GetStart() const noexcept;
    Point GetEnd() const noexcept;

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept;

    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept;

    const Id& GetId() const noexcept;
    Point GetPosition() const noexcept;
    Offset GetOffset() const noexcept;

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept;

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;

    void AddRoad(const Road& road);
    void AddBuilding(const Building& building);

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    using Id = util::Tagged<std::uint64_t, Dog>;

    Dog(Id id, std::string name);

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;

private:
    Id id_;
    std::string name_;
};

class Player {
public:
    using Id = util::Tagged<std::uint64_t, Player>;

    Player(Id id, std::string dog_name, std::shared_ptr<const Map> map);

    const Id& GetId() const noexcept;
    const Dog& GetDog() const noexcept;
    const Map& GetMap() const noexcept;

private:
    Id id_;
    Dog dog_;
    std::shared_ptr<const Map> map_;
};

class Players {
public:
    Player& Add(std::string dog_name, std::shared_ptr<const Map> map);
    const std::vector<Player*>& GetPlayersOnMap(const Map::Id& map_id) const noexcept;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;

    std::uint64_t next_player_id_ = 0;
    std::deque<Player> players_;
    std::unordered_map<Map::Id, std::vector<Player*>, MapIdHasher> players_by_map_;
};

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    PlayerTokens();
    PlayerTokens(const PlayerTokens&) = delete;
    PlayerTokens& operator=(const PlayerTokens&) = delete;
    PlayerTokens(PlayerTokens&& other) noexcept;
    PlayerTokens& operator=(PlayerTokens&&) = delete;

    Token AddPlayer(Player& player);
    Player* FindPlayerByToken(const Token& token) const noexcept;

private:
    Token GenerateToken();

    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
    std::random_device random_device_;
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;
};

class Game {
public:
    using Maps = std::vector<std::shared_ptr<const Map>>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept;
    std::shared_ptr<const Map> FindMap(const Map::Id& id) const noexcept;

    std::pair<Player&, Token> JoinPlayer(std::string dog_name, const Map::Id& map_id);
    Player* FindPlayerByToken(const Token& token) const noexcept;
    const std::vector<Player*>& GetPlayersOnMap(const Map::Id& map_id) const noexcept;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
    Players players_;
    PlayerTokens player_tokens_;
};

}  // namespace model
