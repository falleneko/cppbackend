#include "model.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

namespace model {
using namespace std::literals;

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
