#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

namespace DATA_CONST {
extern const char* START_X;
extern const char* START_Y;
extern const char* END_X;
extern const char* END_Y;
extern const char* OFFSET_X;
extern const char* OFFSET_Y;
extern const char* WIDTH;
extern const char* HEIGHT;
extern const char* X;
extern const char* Y;
extern const char* MAP_BLOCK;
extern const char* OFFICE_BLOCK;
extern const char* ROAD_BLOCK;
extern const char* BUILDING_BLOCK;
extern const char* DEF_DOG_SPEED;
extern const char* DOG_SPEED;
extern const char* MAP_NAME;
} // namespace DATA_CONST

struct Point {
    Coord x, y;
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double x = 0.0;
    double y = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST,
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

    Map(Id id, std::string name, double dog_speed = 1.0) noexcept;

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;
    double GetDogSpeed() const noexcept;
    Position GetRandomRoadPosition() const;
    Position GetFirstRoadPosition() const noexcept;
    Position GetBoundedPosition(Position from, Position to) const noexcept;

    void AddRoad(const Road& road);
    void AddBuilding(const Building& building);

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double dog_speed_ = 1.0;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    using Id = util::Tagged<std::uint64_t, Dog>;

    Dog(Id id, std::string name, Position position);

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    Position GetPosition() const noexcept;
    Speed GetSpeed() const noexcept;
    Direction GetDirection() const noexcept;

    void SetMovement(Direction direction, double speed) noexcept;
    void Stop() noexcept;
    void Update(std::chrono::milliseconds time_delta, const Map& map) noexcept;

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_;
    Direction direction_ = Direction::NORTH;
};

class Game {
public:
    using Maps = std::vector<std::shared_ptr<const Map>>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept;
    std::shared_ptr<const Map> FindMap(const Map::Id& id) const noexcept;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
};

}  // namespace model
