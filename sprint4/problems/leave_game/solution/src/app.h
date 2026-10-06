#pragma once

#include "model.h"
#include "loot_generator.h"
#include "records.h"

#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace app {

using Score = std::uint64_t;

struct LostObject {
    std::uint64_t id;
    std::size_t type;
    model::Position position;
};

struct PlayerState {
    std::uint64_t id = 0;
    std::string name;
    model::Position position;
    model::Speed speed;
    model::Direction direction = model::Direction::NORTH;
    std::string map_id;
    model::Position previous_position;
    std::vector<LostObject> bag;
    Score score = 0;
    std::int64_t play_time_ms = 0;
    std::int64_t idle_time_ms = 0;
};

struct TokenState {
    std::string token;
    std::uint64_t player_id = 0;
};

struct GameSessionState {
    std::string map_id;
    std::vector<LostObject> lost_objects;
    std::uint64_t next_object_id = 0;
    std::int64_t time_without_loot_ms = 0;
};

struct ApplicationState {
    std::uint64_t next_player_id = 0;
    std::vector<PlayerState> players;
    std::vector<TokenState> tokens;
    std::vector<GameSessionState> sessions;
};

class ApplicationListener {
public:
    virtual void OnTick(std::chrono::milliseconds time_delta) = 0;
    virtual ~ApplicationListener() = default;
};

class Player {
public:
    using Id = util::Tagged<std::uint64_t, Player>;

    Player(Id id, std::string dog_name, model::Position dog_position,
           std::shared_ptr<const model::Map> map);
    Player(PlayerState state, std::shared_ptr<const model::Map> map);

    const Id& GetId() const noexcept;
    const model::Dog& GetDog() const noexcept;
    const model::Map& GetMap() const noexcept;
    model::Position GetPreviousPosition() const noexcept;
    const std::vector<LostObject>& GetBag() const noexcept;
    Score GetScore() const noexcept;
    std::chrono::milliseconds GetPlayTime() const noexcept;
    std::chrono::milliseconds GetIdleTime() const noexcept;
    PlayerState GetState() const;

    void Move(model::Direction direction) noexcept;
    void Stop() noexcept;
    void Tick(std::chrono::milliseconds time_delta) noexcept;
    bool TryAddToBag(const LostObject& object);
    void ReturnLoot() noexcept;

private:
    Id id_;
    model::Dog dog_;
    std::shared_ptr<const model::Map> map_;
    model::Position previous_position_;
    std::vector<LostObject> bag_;
    Score score_ = 0;
    std::chrono::milliseconds play_time_{0};
    std::chrono::milliseconds idle_time_{0};
};

class Players {
public:
    Player& Add(std::string dog_name, std::shared_ptr<const model::Map> map,
                bool randomize_spawn_points);
    Player& Restore(PlayerState state, std::shared_ptr<const model::Map> map);
    void Tick(std::chrono::milliseconds time_delta) noexcept;
    const std::vector<Player*>& GetPlayersOnMap(
        const model::Map::Id& map_id) const noexcept;
    Player* FindById(Player::Id id) noexcept;
    std::vector<Player*> FindRetired(
        std::chrono::milliseconds retirement_time) noexcept;
    void Remove(Player& player);
    std::vector<PlayerState> GetState() const;
    std::uint64_t GetNextPlayerId() const noexcept;
    void SetNextPlayerId(std::uint64_t next_player_id);

private:
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;

    std::uint64_t next_player_id_ = 0;
    std::list<Player> players_;
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
    void AddPlayer(Token token, Player& player);
    Player* FindPlayerByToken(const Token& token) const noexcept;
    void RemovePlayer(const Player& player) noexcept;
    std::vector<TokenState> GetState() const;

private:
    Token GenerateToken();

    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
    std::random_device random_device_;
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;
};

class GameSession {
public:
    GameSession(std::shared_ptr<const model::Map> map,
                model::Game::LootGeneratorConfig config);
    GameSession(GameSessionState state, std::shared_ptr<const model::Map> map,
                model::Game::LootGeneratorConfig config);

    void Tick(std::chrono::milliseconds time_delta,
              const std::vector<Player*>& players);
    const std::vector<LostObject>& GetLostObjects() const noexcept;
    GameSessionState GetState() const;

private:
    std::shared_ptr<const model::Map> map_;
    loot_gen::LootGenerator loot_generator_;
    std::vector<LostObject> lost_objects_;
    std::uint64_t next_object_id_ = 0;
    std::mt19937_64 random_generator_{std::random_device{}()};
};

class Application {
public:
    explicit Application(model::Game& game,
                         bool randomize_spawn_points = false,
                         PlayerRecordRepository* records = nullptr) noexcept;

    const model::Game::Maps& GetMaps() const noexcept;
    std::shared_ptr<const model::Map> FindMap(const model::Map::Id& id) const noexcept;
    std::pair<Player&, Token> JoinGame(std::string dog_name,
                                      const model::Map::Id& map_id);
    Player* FindPlayerByToken(const Token& token) const noexcept;
    const std::vector<Player*>& GetPlayersOnMap(
        const model::Map::Id& map_id) const noexcept;
    const std::vector<LostObject>& GetLostObjectsOnMap(
        const model::Map::Id& map_id) const noexcept;
    void Tick(std::chrono::milliseconds time_delta);
    std::vector<PlayerRecord> GetRecords(std::size_t start,
                                         std::size_t max_items) const;
    ApplicationState GetState() const;
    void RestoreState(ApplicationState state);
    void SetListener(ApplicationListener* listener) noexcept;

private:
    model::Game& game_;
    bool randomize_spawn_points_ = false;
    PlayerRecordRepository* records_ = nullptr;
    Players players_;
    PlayerTokens player_tokens_;
    std::unordered_map<model::Map::Id, std::unique_ptr<GameSession>,
                       util::TaggedHasher<model::Map::Id>> sessions_;
    ApplicationListener* listener_ = nullptr;
};

}  // namespace app
