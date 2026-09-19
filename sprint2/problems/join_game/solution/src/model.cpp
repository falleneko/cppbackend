#include "model.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace model {
using namespace std::literals;

namespace {

std::mt19937_64::result_type GenerateSeed(std::random_device& random_device) {
    std::uniform_int_distribution<std::mt19937_64::result_type> dist;
    return dist(random_device);
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

Map::Map(Id id, std::string name) noexcept
    : id_{std::move(id)}
    , name_{std::move(name)} {
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

Dog::Dog(Id id, std::string name)
    : id_{id}
    , name_{std::move(name)} {
}

const Dog::Id& Dog::GetId() const noexcept {
    return id_;
}

const std::string& Dog::GetName() const noexcept {
    return name_;
}

Player::Player(Id id, std::string dog_name, std::shared_ptr<const Map> map)
    : id_{id}
    , dog_{Dog::Id{*id}, std::move(dog_name)}
    , map_{std::move(map)} {
}

const Player::Id& Player::GetId() const noexcept {
    return id_;
}

const Dog& Player::GetDog() const noexcept {
    return dog_;
}

const Map& Player::GetMap() const noexcept {
    return *map_;
}

Player& Players::Add(std::string dog_name, std::shared_ptr<const Map> map) {
    const Player::Id id{next_player_id_};
    const Map::Id map_id = map->GetId();
    Player& player = players_.emplace_back(id, std::move(dog_name), std::move(map));
    try {
        players_by_map_[map_id].push_back(&player);
    } catch (...) {
        players_.pop_back();
        throw;
    }
    ++next_player_id_;
    return player;
}

const std::vector<Player*>& Players::GetPlayersOnMap(const Map::Id& map_id) const noexcept {
    static const std::vector<Player*> no_players;
    if (const auto it = players_by_map_.find(map_id); it != players_by_map_.end()) {
        return it->second;
    }
    return no_players;
}

PlayerTokens::PlayerTokens()
    : generator1_{GenerateSeed(random_device_)}
    , generator2_{GenerateSeed(random_device_)} {
}

PlayerTokens::PlayerTokens(PlayerTokens&& other) noexcept
    : token_to_player_{std::move(other.token_to_player_)}
    , generator1_{std::move(other.generator1_)}
    , generator2_{std::move(other.generator2_)} {
}

Token PlayerTokens::GenerateToken() {
    std::ostringstream token;
    token << std::hex << std::setfill('0')
          << std::setw(16) << generator1_()
          << std::setw(16) << generator2_();
    return Token{std::move(token).str()};
}

Token PlayerTokens::AddPlayer(Player& player) {
    while (true) {
        Token token = GenerateToken();
        if (token_to_player_.emplace(token, &player).second) {
            return token;
        }
    }
}

Player* PlayerTokens::FindPlayerByToken(const Token& token) const noexcept {
    if (const auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
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

std::pair<Player&, Token> Game::JoinPlayer(std::string dog_name, const Map::Id& map_id) {
    auto map = FindMap(map_id);
    if (!map) {
        throw std::invalid_argument("Map not found");
    }
    Player& player = players_.Add(std::move(dog_name), std::move(map));
    return {player, player_tokens_.AddPlayer(player)};
}

Player* Game::FindPlayerByToken(const Token& token) const noexcept {
    return player_tokens_.FindPlayerByToken(token);
}

const std::vector<Player*>& Game::GetPlayersOnMap(const Map::Id& map_id) const noexcept {
    return players_.GetPlayersOnMap(map_id);
}

}  // namespace model
