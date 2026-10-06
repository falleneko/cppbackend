#include "app.h"

#include "collision_detector.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace app {

namespace {

constexpr double PLAYER_RADIUS = 0.3;
constexpr double LOST_OBJECT_RADIUS = 0.0;
constexpr double OFFICE_RADIUS = 0.25;

std::mt19937_64::result_type GenerateSeed(std::random_device& random_device) {
    std::uniform_int_distribution<std::mt19937_64::result_type> distribution;
    return distribution(random_device);
}

geom::Point2D ToPoint(model::Position position) noexcept {
    return {position.x, position.y};
}

geom::Point2D ToPoint(model::Point point) noexcept {
    return {static_cast<double>(point.x), static_cast<double>(point.y)};
}

class SessionCollisionProvider final
    : public collision_detector::ItemGathererProvider {
public:
    SessionCollisionProvider(const std::vector<LostObject>& lost_objects,
                             const model::Map::Offices& offices,
                             const std::vector<Player*>& players) noexcept
        : lost_objects_{lost_objects}
        , offices_{offices}
        , players_{players} {
    }

    std::size_t ItemsCount() const override {
        return lost_objects_.size() + offices_.size();
    }

    collision_detector::Item GetItem(std::size_t index) const override {
        if (index < lost_objects_.size()) {
            return {ToPoint(lost_objects_.at(index).position), LOST_OBJECT_RADIUS};
        }
        return {ToPoint(offices_.at(index - lost_objects_.size()).GetPosition()),
                OFFICE_RADIUS};
    }

    std::size_t GatherersCount() const override {
        return players_.size();
    }

    collision_detector::Gatherer GetGatherer(std::size_t index) const override {
        const Player& player = *players_.at(index);
        return {ToPoint(player.GetPreviousPosition()),
                ToPoint(player.GetDog().GetPosition()), PLAYER_RADIUS};
    }

private:
    const std::vector<LostObject>& lost_objects_;
    const model::Map::Offices& offices_;
    const std::vector<Player*>& players_;
};

}  // namespace

Player::Player(Id id, std::string dog_name, model::Position dog_position,
               std::shared_ptr<const model::Map> map)
    : id_{id}
    , dog_{model::Dog::Id{*id}, std::move(dog_name), dog_position}
    , map_{std::move(map)}
    , previous_position_{dog_position} {
}

Player::Player(PlayerState state, std::shared_ptr<const model::Map> map)
    : id_{state.id}
    , dog_{model::Dog::Id{state.id}, std::move(state.name), state.position,
           state.speed, state.direction}
    , map_{std::move(map)}
    , previous_position_{state.previous_position}
    , bag_{std::move(state.bag)}
    , score_{state.score}
    , play_time_{state.play_time_ms}
    , idle_time_{state.idle_time_ms} {
    if (play_time_ < std::chrono::milliseconds::zero()
        || idle_time_ < std::chrono::milliseconds::zero()
        || idle_time_ > play_time_) {
        throw std::invalid_argument("Invalid player timing state");
    }
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

model::Position Player::GetPreviousPosition() const noexcept {
    return previous_position_;
}

const std::vector<LostObject>& Player::GetBag() const noexcept {
    return bag_;
}

Score Player::GetScore() const noexcept {
    return score_;
}

std::chrono::milliseconds Player::GetPlayTime() const noexcept {
    return play_time_;
}

std::chrono::milliseconds Player::GetIdleTime() const noexcept {
    return idle_time_;
}

PlayerState Player::GetState() const {
    return {
        *id_,
        dog_.GetName(),
        dog_.GetPosition(),
        dog_.GetSpeed(),
        dog_.GetDirection(),
        *map_->GetId(),
        previous_position_,
        bag_,
        score_,
        play_time_.count(),
        idle_time_.count(),
    };
}

void Player::Move(model::Direction direction) noexcept {
    dog_.SetMovement(direction, map_->GetDogSpeed());
    idle_time_ = std::chrono::milliseconds::zero();
}

void Player::Stop() noexcept {
    dog_.Stop();
}

void Player::Tick(std::chrono::milliseconds time_delta) noexcept {
    previous_position_ = dog_.GetPosition();
    const model::Speed speed = dog_.GetSpeed();
    const bool was_stationary = speed.x == 0.0 && speed.y == 0.0;
    dog_.Update(time_delta, *map_);
    play_time_ += time_delta;
    if (was_stationary) {
        idle_time_ += time_delta;
    }
}

bool Player::TryAddToBag(const LostObject& object) {
    if (bag_.size() >= map_->GetBagCapacity()) {
        return false;
    }
    bag_.push_back(object);
    return true;
}

void Player::ReturnLoot() noexcept {
    for (const LostObject& object : bag_) {
        score_ += map_->GetLootValue(object.type);
    }
    bag_.clear();
}

Player& Players::Add(std::string dog_name,
                     std::shared_ptr<const model::Map> map,
                     bool randomize_spawn_points) {
    const Player::Id id{next_player_id_};
    const model::Map::Id map_id = map->GetId();
    const model::Position dog_position = randomize_spawn_points
        ? map->GetRandomRoadPosition()
        : map->GetFirstRoadPosition();
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

Player& Players::Restore(PlayerState state,
                         std::shared_ptr<const model::Map> map) {
    if (!map) {
        throw std::invalid_argument("Cannot restore a player without a map");
    }
    const Player::Id id{state.id};
    if (FindById(id)) {
        throw std::invalid_argument("Duplicate player id in saved state");
    }
    const model::Map::Id map_id = map->GetId();
    Player& player = players_.emplace_back(std::move(state), std::move(map));
    try {
        players_by_map_[map_id].push_back(&player);
    } catch (...) {
        players_.pop_back();
        throw;
    }
    return player;
}

void Players::Tick(std::chrono::milliseconds time_delta) noexcept {
    for (Player& player : players_) {
        player.Tick(time_delta);
    }
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

Player* Players::FindById(Player::Id id) noexcept {
    for (Player& player : players_) {
        if (player.GetId() == id) {
            return &player;
        }
    }
    return nullptr;
}

std::vector<Player*> Players::FindRetired(
    std::chrono::milliseconds retirement_time) noexcept {
    std::vector<Player*> retired;
    for (Player& player : players_) {
        if (player.GetIdleTime() >= retirement_time) {
            retired.push_back(&player);
        }
    }
    return retired;
}

void Players::Remove(Player& player) {
    const model::Map::Id map_id = player.GetMap().GetId();
    if (auto map_it = players_by_map_.find(map_id);
        map_it != players_by_map_.end()) {
        auto& map_players = map_it->second;
        map_players.erase(
            std::remove(map_players.begin(), map_players.end(), &player),
            map_players.end());
        if (map_players.empty()) {
            players_by_map_.erase(map_it);
        }
    }

    const auto player_it = std::find_if(
        players_.begin(), players_.end(),
        [&player](const Player& candidate) { return &candidate == &player; });
    if (player_it != players_.end()) {
        players_.erase(player_it);
    }
}

std::vector<PlayerState> Players::GetState() const {
    std::vector<PlayerState> state;
    state.reserve(players_.size());
    for (const Player& player : players_) {
        state.push_back(player.GetState());
    }
    return state;
}

std::uint64_t Players::GetNextPlayerId() const noexcept {
    return next_player_id_;
}

void Players::SetNextPlayerId(std::uint64_t next_player_id) {
    for (const Player& player : players_) {
        if (*player.GetId() >= next_player_id) {
            throw std::invalid_argument("Invalid next player id in saved state");
        }
    }
    next_player_id_ = next_player_id;
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

void PlayerTokens::AddPlayer(Token token, Player& player) {
    if (!token_to_player_.emplace(std::move(token), &player).second) {
        throw std::invalid_argument("Duplicate player token in saved state");
    }
}

Player* PlayerTokens::FindPlayerByToken(const Token& token) const noexcept {
    if (const auto it = token_to_player_.find(token);
        it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

void PlayerTokens::RemovePlayer(const Player& player) noexcept {
    for (auto it = token_to_player_.begin(); it != token_to_player_.end();) {
        if (it->second == &player) {
            it = token_to_player_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<TokenState> PlayerTokens::GetState() const {
    std::vector<TokenState> state;
    state.reserve(token_to_player_.size());
    for (const auto& [token, player] : token_to_player_) {
        state.push_back({*token, *player->GetId()});
    }
    return state;
}

GameSession::GameSession(std::shared_ptr<const model::Map> map,
                         model::Game::LootGeneratorConfig config)
    : map_{std::move(map)}
    , loot_generator_{config.period, config.probability} {
}

GameSession::GameSession(GameSessionState state,
                         std::shared_ptr<const model::Map> map,
                         model::Game::LootGeneratorConfig config)
    : map_{std::move(map)}
    , loot_generator_{config.period, config.probability}
    , lost_objects_{std::move(state.lost_objects)}
    , next_object_id_{state.next_object_id} {
    loot_generator_.SetTimeWithoutLoot(
        std::chrono::milliseconds{state.time_without_loot_ms});
    for (const LostObject& object : lost_objects_) {
        if (object.id >= next_object_id_) {
            throw std::invalid_argument("Invalid next lost object id in saved state");
        }
    }
}

void GameSession::Tick(std::chrono::milliseconds time_delta,
                       const std::vector<Player*>& players) {
    const std::size_t lost_object_count = lost_objects_.size();
    const SessionCollisionProvider collision_provider{
        lost_objects_, map_->GetOffices(), players};
    const auto events = collision_detector::FindGatherEvents(collision_provider);
    std::vector<bool> collected(lost_object_count, false);

    for (const auto& event : events) {
        Player& player = *players.at(event.gatherer_id);
        if (event.item_id < lost_object_count) {
            if (!collected[event.item_id]
                && player.TryAddToBag(lost_objects_.at(event.item_id))) {
                collected[event.item_id] = true;
            }
        } else {
            player.ReturnLoot();
        }
    }

    if (lost_object_count != 0) {
        std::vector<LostObject> remaining_objects;
        remaining_objects.reserve(lost_object_count);
        for (std::size_t index = 0; index < lost_object_count; ++index) {
            if (!collected[index]) {
                remaining_objects.push_back(std::move(lost_objects_[index]));
            }
        }
        lost_objects_ = std::move(remaining_objects);
    }

    const unsigned count = loot_generator_.Generate(
        time_delta, static_cast<unsigned>(lost_objects_.size()),
        static_cast<unsigned>(players.size()));
    if (count == 0 || map_->GetRoads().empty() || map_->GetLootTypeCount() == 0) {
        return;
    }
    std::uniform_int_distribution<std::size_t> type_distribution{
        0, map_->GetLootTypeCount() - 1};
    for (unsigned i = 0; i < count; ++i) {
        lost_objects_.push_back({next_object_id_++, type_distribution(random_generator_),
                                 map_->GetRandomRoadPosition()});
    }
}

const std::vector<LostObject>& GameSession::GetLostObjects() const noexcept {
    return lost_objects_;
}

GameSessionState GameSession::GetState() const {
    return {
        *map_->GetId(),
        lost_objects_,
        next_object_id_,
        loot_generator_.GetTimeWithoutLoot().count(),
    };
}

Application::Application(model::Game& game, bool randomize_spawn_points,
                         PlayerRecordRepository* records) noexcept
    : game_{game}
    , randomize_spawn_points_{randomize_spawn_points}
    , records_{records} {
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
    Player& player = players_.Add(std::move(dog_name), std::move(map),
                                  randomize_spawn_points_);
    if (!sessions_.contains(map_id)) {
        sessions_.emplace(map_id, std::make_unique<GameSession>(
            game_.FindMap(map_id), game_.GetLootGeneratorConfig()));
    }
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

const std::vector<LostObject>& Application::GetLostObjectsOnMap(
    const model::Map::Id& map_id) const noexcept {
    static const std::vector<LostObject> no_objects;
    const auto it = sessions_.find(map_id);
    return it == sessions_.end() ? no_objects : it->second->GetLostObjects();
}

void Application::Tick(std::chrono::milliseconds time_delta) {
    players_.Tick(time_delta);
    for (auto& [map_id, session] : sessions_) {
        session->Tick(time_delta, players_.GetPlayersOnMap(map_id));
    }

    const std::vector<Player*> retired
        = players_.FindRetired(game_.GetDogRetirementTime());
    if (!retired.empty()) {
        std::vector<PlayerRecord> records;
        records.reserve(retired.size());
        for (const Player* player : retired) {
            records.push_back({player->GetDog().GetName(), player->GetScore(),
                               player->GetPlayTime()});
        }
        if (records_) {
            records_->Save(records);
        }
        for (Player* player : retired) {
            player_tokens_.RemovePlayer(*player);
            players_.Remove(*player);
        }
    }

    if (listener_) {
        listener_->OnTick(time_delta);
    }
}

std::vector<PlayerRecord> Application::GetRecords(std::size_t start,
                                                   std::size_t max_items) const {
    return records_ ? records_->Get(start, max_items)
                    : std::vector<PlayerRecord>{};
}

ApplicationState Application::GetState() const {
    ApplicationState state;
    state.next_player_id = players_.GetNextPlayerId();
    state.players = players_.GetState();
    state.tokens = player_tokens_.GetState();
    state.sessions.reserve(sessions_.size());
    for (const auto& [map_id, session] : sessions_) {
        state.sessions.push_back(session->GetState());
    }
    return state;
}

void Application::RestoreState(ApplicationState state) {
    if (!players_.GetState().empty() || !player_tokens_.GetState().empty()
        || !sessions_.empty()) {
        throw std::logic_error("Application state can only be restored at startup");
    }

    for (PlayerState& player_state : state.players) {
        const auto map = game_.FindMap(model::Map::Id{player_state.map_id});
        if (!map) {
            throw std::invalid_argument("Unknown map in saved player state");
        }
        players_.Restore(std::move(player_state), map);
    }
    players_.SetNextPlayerId(state.next_player_id);

    for (TokenState& token_state : state.tokens) {
        Player* player = players_.FindById(Player::Id{token_state.player_id});
        if (!player) {
            throw std::invalid_argument("Unknown player in saved token state");
        }
        player_tokens_.AddPlayer(Token{std::move(token_state.token)}, *player);
    }

    for (GameSessionState& session_state : state.sessions) {
        const model::Map::Id map_id{session_state.map_id};
        const auto map = game_.FindMap(map_id);
        if (!map) {
            throw std::invalid_argument("Unknown map in saved session state");
        }
        auto session = std::make_unique<GameSession>(
            std::move(session_state), map, game_.GetLootGeneratorConfig());
        if (!sessions_.emplace(map_id, std::move(session)).second) {
            throw std::invalid_argument("Duplicate map session in saved state");
        }
    }

    for (const PlayerState& player_state : players_.GetState()) {
        if (!sessions_.contains(model::Map::Id{player_state.map_id})) {
            throw std::invalid_argument("Player session is missing in saved state");
        }
    }
}

void Application::SetListener(ApplicationListener* listener) noexcept {
    listener_ = listener;
}

}  // namespace app
