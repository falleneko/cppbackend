#include "app.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace app {

namespace {

std::mt19937_64::result_type GenerateSeed(std::random_device& random_device) {
    std::uniform_int_distribution<std::mt19937_64::result_type> distribution;
    return distribution(random_device);
}

}  // namespace

Player::Player(Id id, std::string dog_name, model::Position dog_position,
               std::shared_ptr<const model::Map> map)
    : id_{id}
    , dog_{model::Dog::Id{*id}, std::move(dog_name), dog_position}
    , map_{std::move(map)} {
}

const Player::Id& Player::GetId() const noexcept {
    return id_;
}

const model::Dog& Player::GetDog() const noexcept {
    return dog_;
}

const model::Map& Player::GetMap() const noexcept {
    return *map_;
}

void Player::Move(model::Direction direction) noexcept {
    dog_.SetMovement(direction, map_->GetDogSpeed());
}

void Player::Stop() noexcept {
    dog_.Stop();
}

Player& Players::Add(std::string dog_name,
                     std::shared_ptr<const model::Map> map) {
    const Player::Id id{next_player_id_};
    const model::Map::Id map_id = map->GetId();
    const model::Position dog_position = map->GetRandomRoadPosition();
    Player& player = players_.emplace_back(id, std::move(dog_name), dog_position,
                                           std::move(map));
    try {
        players_by_map_[map_id].push_back(&player);
    } catch (...) {
        players_.pop_back();
        throw;
    }
    ++next_player_id_;
    return player;
}

const std::vector<Player*>& Players::GetPlayersOnMap(
    const model::Map::Id& map_id) const noexcept {
    static const std::vector<Player*> no_players;
    if (const auto it = players_by_map_.find(map_id);
        it != players_by_map_.end()) {
        return it->second;
    }
    return no_players;
}

PlayerTokens::PlayerTokens()
    : generator1_{GenerateSeed(random_device_)}
    , generator2_{GenerateSeed(random_device_)} {
}

Token PlayerTokens::GenerateToken() {
    std::ostringstream token;
    token << std::hex << std::setfill('0') << std::setw(16) << generator1_()
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
    if (const auto it = token_to_player_.find(token);
        it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

Application::Application(model::Game& game) noexcept
    : game_{game} {
}

const model::Game::Maps& Application::GetMaps() const noexcept {
    return game_.GetMaps();
}

std::shared_ptr<const model::Map> Application::FindMap(const model::Map::Id& id) const noexcept {
    return game_.FindMap(id);
}

std::pair<Player&, Token> Application::JoinGame(
    std::string dog_name,
    const model::Map::Id& map_id
) {
    auto map = game_.FindMap(map_id);
    if (!map) {
        throw std::invalid_argument("Map not found");
    }
    Player& player = players_.Add(std::move(dog_name), std::move(map));
    return {player, player_tokens_.AddPlayer(player)};
}

Player* Application::FindPlayerByToken(const Token& token) const noexcept {
    return player_tokens_.FindPlayerByToken(token);
}

const std::vector<Player*>& Application::GetPlayersOnMap(
    const model::Map::Id& map_id
) const noexcept {
    return players_.GetPlayersOnMap(map_id);
}

}  // namespace app
