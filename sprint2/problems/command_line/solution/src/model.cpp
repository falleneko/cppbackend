#include "model.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

namespace model {
using namespace std::literals;

namespace DATA_CONST {
    const char* START_X = "x0";
    const char* START_Y = "y0";
    const char* END_X = "x1";
    const char* END_Y = "y1";
    const char* OFFSET_X = "offsetX";
    const char* OFFSET_Y = "offsetY";
    const char* WIDTH = "w";
    const char* HEIGHT = "h";
    const char* X = "x";
    const char* Y = "y";
    const char* MAP_BLOCK = "maps";
    const char* OFFICE_BLOCK = "offices";
    const char* ROAD_BLOCK = "roads";
    const char* BUILDING_BLOCK = "buildings";
    const char* DEF_DOG_SPEED = "defaultDogSpeed";
    const char* DOG_SPEED = "dogSpeed";
    const char* MAP_NAME = "name";
}

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;

struct Interval {
    double begin;
    double end;
};

bool Intersects(const Interval& lhs, const Interval& rhs) noexcept {
    return lhs.begin <= rhs.end && rhs.begin <= lhs.end;
}

}  // namespace

Road::Road(HorizontalTag, Point start, Coord end_x) noexcept
    : start_{start}
    , end_{end_x, start.y} {
}

Road::Road(VerticalTag, Point start, Coord end_y) noexcept
    : start_{start}
    , end_{start.x, end_y} {
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

Building::Building(Rectangle bounds) noexcept
    : bounds_{bounds} {
}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Id id, Point position, Offset offset) noexcept
    : id_{std::move(id)}
    , position_{position}
    , offset_{offset} {
}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

Point Office::GetPosition() const noexcept {
    return position_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

Map::Map(Id id, std::string name, double dog_speed) noexcept
    : id_{std::move(id)}
    , name_{std::move(name)}
    , dog_speed_{dog_speed} {
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

double Map::GetDogSpeed() const noexcept {
    return dog_speed_;
}

Position Map::GetRandomRoadPosition() const {
    if (roads_.empty()) {
        return {};
    }

    static thread_local std::mt19937_64 generator{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> road_distribution{0, roads_.size() - 1};
    const Road& road = roads_[road_distribution(generator)];
    const Point start = road.GetStart();
    const Point end = road.GetEnd();

    if (road.IsHorizontal()) {
        std::uniform_real_distribution<double> coordinate_distribution{
            static_cast<double>(std::min(start.x, end.x)),
            static_cast<double>(std::max(start.x, end.x))
        };
        return {coordinate_distribution(generator), static_cast<double>(start.y)};
    }

    std::uniform_real_distribution<double> coordinate_distribution{
        static_cast<double>(std::min(start.y, end.y)),
        static_cast<double>(std::max(start.y, end.y))
    };
    return {static_cast<double>(start.x), coordinate_distribution(generator)};
}

Position Map::GetFirstRoadPosition() const noexcept {
    if (roads_.empty()) {
        return {};
    }
    const Point start = roads_.front().GetStart();
    return {static_cast<double>(start.x), static_cast<double>(start.y)};
}

Position Map::GetBoundedPosition(Position from, Position to) const noexcept {
    if (from.x == to.x && from.y == to.y) {
        return from;
    }

    if (from.y == to.y) {
        Interval reachable{from.x, from.x};
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const Road& road : roads_) {
                const Point start = road.GetStart();
                const Point end = road.GetEnd();
                const double min_y = static_cast<double>(std::min(start.y, end.y)) -
                                     ROAD_HALF_WIDTH;
                const double max_y = static_cast<double>(std::max(start.y, end.y)) +
                                     ROAD_HALF_WIDTH;
                const Interval road_interval{
                    static_cast<double>(std::min(start.x, end.x)) - ROAD_HALF_WIDTH,
                    static_cast<double>(std::max(start.x, end.x)) + ROAD_HALF_WIDTH,
                };
                if (min_y <= from.y && from.y <= max_y &&
                    Intersects(reachable, road_interval)) {
                    const Interval joined{
                        std::min(reachable.begin, road_interval.begin),
                        std::max(reachable.end, road_interval.end),
                    };
                    if (joined.begin != reachable.begin ||
                        joined.end != reachable.end) {
                        reachable = joined;
                        expanded = true;
                    }
                }
            }
        }
        return {std::clamp(to.x, reachable.begin, reachable.end), from.y};
    }

    Interval reachable{from.y, from.y};
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const Road& road : roads_) {
            const Point start = road.GetStart();
            const Point end = road.GetEnd();
            const double min_x = static_cast<double>(std::min(start.x, end.x)) -
                                 ROAD_HALF_WIDTH;
            const double max_x = static_cast<double>(std::max(start.x, end.x)) +
                                 ROAD_HALF_WIDTH;
            const Interval road_interval{
                static_cast<double>(std::min(start.y, end.y)) - ROAD_HALF_WIDTH,
                static_cast<double>(std::max(start.y, end.y)) + ROAD_HALF_WIDTH,
            };
            if (min_x <= from.x && from.x <= max_x &&
                Intersects(reachable, road_interval)) {
                const Interval joined{
                    std::min(reachable.begin, road_interval.begin),
                    std::max(reachable.end, road_interval.end),
                };
                if (joined.begin != reachable.begin || joined.end != reachable.end) {
                    reachable = joined;
                    expanded = true;
                }
            }
        }
    }
    return {from.x, std::clamp(to.y, reachable.begin, reachable.end)};
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

Dog::Dog(Id id, std::string name, Position position)
    : id_{id}
    , name_{std::move(name)}
    , position_{position} {
}

const Dog::Id& Dog::GetId() const noexcept {
    return id_;
}

const std::string& Dog::GetName() const noexcept {
    return name_;
}

Position Dog::GetPosition() const noexcept {
    return position_;
}

Speed Dog::GetSpeed() const noexcept {
    return speed_;
}

Direction Dog::GetDirection() const noexcept {
    return direction_;
}

void Dog::SetMovement(Direction direction, double speed) noexcept {
    direction_ = direction;
    switch (direction) {
        case Direction::NORTH:
            speed_ = {0.0, -speed};
            break;
        case Direction::SOUTH:
            speed_ = {0.0, speed};
            break;
        case Direction::WEST:
            speed_ = {-speed, 0.0};
            break;
        case Direction::EAST:
            speed_ = {speed, 0.0};
            break;
    }
}

void Dog::Stop() noexcept {
    speed_ = {0.0, 0.0};
}

void Dog::Update(std::chrono::milliseconds time_delta, const Map& map) noexcept {
    const double seconds = std::chrono::duration<double>{time_delta}.count();
    const Position estimated_position{
        position_.x + speed_.x * seconds,
        position_.y + speed_.y * seconds,
    };
    const Position bounded_position = map.GetBoundedPosition(position_, estimated_position);
    position_ = bounded_position;
    if (bounded_position.x != estimated_position.x ||
        bounded_position.y != estimated_position.y) {
        Stop();
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::make_shared<Map>(std::move(map)));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

std::shared_ptr<const Map> Game::FindMap(const Map::Id& id) const noexcept {
    if (const auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return maps_.at(it->second);
    }
    return nullptr;
}

}  // namespace model
