#pragma once

#include "model.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace app {

class Player {
public:
    using Id = util::Tagged<std::uint64_t, Player>;

    Player(Id id, std::string dog_name, model::Position dog_position,
           std::shared_ptr<const model::Map> map);

    const Id& GetId() const noexcept;
    const model::Dog& GetDog() const noexcept;
    const model::Map& GetMap() const noexcept;

    void Move(model::Direction direction) noexcept;
    void Stop() noexcept;
    void Tick(std::chrono::milliseconds time_delta) noexcept;

private:
    Id id_;
    model::Dog dog_;
    std::shared_ptr<const model::Map> map_;
};

class Players {
public:
    Player& Add(std::string dog_name, std::shared_ptr<const model::Map> map,
                bool randomize_spawn_points);
    void Tick(std::chrono::milliseconds time_delta) noexcept;
    const std::vector<Player*>& GetPlayersOnMap(
        const model::Map::Id& map_id) const noexcept;

private:
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;

    std::uint64_t next_player_id_ = 0;
    std::deque<Player> players_;
    std::unordered_map<model::Map::Id, std::vector<Player*>, MapIdHasher> players_by_map_;
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

    Token AddPlayer(Player& player);
    Player* FindPlayerByToken(const Token& token) const noexcept;

private:
    Token GenerateToken();

    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
    std::random_device random_device_;
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;
};

class Application {
public:
    explicit Application(model::Game& game,
                         bool randomize_spawn_points = false) noexcept;

    const model::Game::Maps& GetMaps() const noexcept;
    std::shared_ptr<const model::Map> FindMap(const model::Map::Id& id) const noexcept;
    std::pair<Player&, Token> JoinGame(std::string dog_name,
                                      const model::Map::Id& map_id);
    Player* FindPlayerByToken(const Token& token) const noexcept;
    const std::vector<Player*>& GetPlayersOnMap(
        const model::Map::Id& map_id) const noexcept;
    void Tick(std::chrono::milliseconds time_delta) noexcept;

private:
    model::Game& game_;
    bool randomize_spawn_points_ = false;
    Players players_;
    PlayerTokens player_tokens_;
};

}  // namespace app
