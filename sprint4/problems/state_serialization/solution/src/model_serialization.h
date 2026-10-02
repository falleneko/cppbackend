#pragma once

#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include "app.h"

namespace model {

template <typename Archive>
void serialize(Archive& archive, Position& position,
               [[maybe_unused]] const unsigned version) {
    archive& position.x;
    archive& position.y;
}

template <typename Archive>
void serialize(Archive& archive, Speed& speed,
               [[maybe_unused]] const unsigned version) {
    archive& speed.x;
    archive& speed.y;
}

}  // namespace model

namespace app {

template <typename Archive>
void serialize(Archive& archive, LostObject& object,
               [[maybe_unused]] const unsigned version) {
    archive& object.id;
    archive& object.type;
    archive& object.position;
}

template <typename Archive>
void serialize(Archive& archive, PlayerState& state,
               [[maybe_unused]] const unsigned version) {
    archive& state.id;
    archive& state.name;
    archive& state.position;
    archive& state.speed;
    archive& state.direction;
    archive& state.map_id;
    archive& state.previous_position;
    archive& state.bag;
    archive& state.score;
}

template <typename Archive>
void serialize(Archive& archive, TokenState& state,
               [[maybe_unused]] const unsigned version) {
    archive& state.token;
    archive& state.player_id;
}

template <typename Archive>
void serialize(Archive& archive, GameSessionState& state,
               [[maybe_unused]] const unsigned version) {
    archive& state.map_id;
    archive& state.lost_objects;
    archive& state.next_object_id;
    archive& state.time_without_loot_ms;
}

template <typename Archive>
void serialize(Archive& archive, ApplicationState& state,
               [[maybe_unused]] const unsigned version) {
    archive& state.next_player_id;
    archive& state.players;
    archive& state.tokens;
    archive& state.sessions;
}

}  // namespace app
