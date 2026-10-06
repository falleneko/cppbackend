#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include "../src/collision_detector.h"

namespace Catch {

template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(const collision_detector::GatheringEvent& event) {
        std::ostringstream out;
        out << "(gatherer=" << event.gatherer_id << ", item=" << event.item_id
            << ", sq_distance=" << event.sq_distance << ", time=" << event.time << ')';
        return out.str();
    }
};

}  // namespace Catch

namespace {

using collision_detector::Gatherer;
using collision_detector::GatheringEvent;
using collision_detector::Item;

constexpr double EPSILON = 1e-10;

class VectorItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    VectorItemGathererProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items))
        , gatherers_(std::move(gatherers)) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

class EqualsEventsMatcher : public Catch::Matchers::MatcherGenericBase {
public:
    explicit EqualsEventsMatcher(const std::vector<GatheringEvent>& expected)
        : expected_(expected) {
    }

    bool match(const std::vector<GatheringEvent>& actual) const {
        return actual.size() == expected_.size()
            && std::equal(actual.begin(), actual.end(), expected_.begin(), AreEqual);
    }

    std::string describe() const override {
        return "equals " + Catch::rangeToString(expected_)
            + " (floating-point fields use absolute tolerance 1e-10)";
    }

private:
    static bool AreEqual(const GatheringEvent& lhs, const GatheringEvent& rhs) {
        return lhs.item_id == rhs.item_id && lhs.gatherer_id == rhs.gatherer_id
            && std::abs(lhs.sq_distance - rhs.sq_distance) <= EPSILON
            && std::abs(lhs.time - rhs.time) <= EPSILON;
    }

    const std::vector<GatheringEvent>& expected_;
};

EqualsEventsMatcher EqualsEvents(const std::vector<GatheringEvent>& expected) {
    return EqualsEventsMatcher{expected};
}

}  // namespace

TEST_CASE("No events are produced without both items and gatherers") {
    SECTION("there are no items") {
        const VectorItemGathererProvider provider{
            {},
            {{{0.0, 0.0}, {10.0, 0.0}, 1.0}},
        };

        CHECK(collision_detector::FindGatherEvents(provider).empty());
    }

    SECTION("there are no gatherers") {
        const VectorItemGathererProvider provider{
            {{{5.0, 0.0}, 1.0}},
            {},
        };

        CHECK(collision_detector::FindGatherEvents(provider).empty());
    }
}

TEST_CASE("Gatherers that did not move do not collect anything") {
    const VectorItemGathererProvider provider{
        {
            {{0.0, 0.0}, 100.0},
            {{5.0, 5.0}, 100.0},
        },
        {
            {{0.0, 0.0}, {0.0, 0.0}, 100.0},
            {{5.0, 5.0}, {5.0, 5.0}, 100.0},
        },
    };

    CHECK(collision_detector::FindGatherEvents(provider).empty());
}

TEST_CASE("An arbitrarily small non-zero movement is still processed") {
    const VectorItemGathererProvider provider{
        {
            {{0.5e-12, 0.0}, 0.0},
        },
        {
            {{0.0, 0.0}, {1e-12, 0.0}, 0.0},
        },
    };

    const std::vector<GatheringEvent> expected{
        {.item_id = 0, .gatherer_id = 0, .sq_distance = 0.0, .time = 0.5},
    };

    CHECK_THAT(collision_detector::FindGatherEvents(provider), EqualsEvents(expected));
}

TEST_CASE("Collision events contain exact data and are sorted by collision time") {
    const VectorItemGathererProvider provider{
        {
            {{8.0, 8.0}, 0.1},
            {{2.0, 3.0}, 0.4},
            {{1.0, 1.0}, 0.1},
            {{-1.0, -1.0}, 100.0},
            {{5.0, 6.0}, 0.3},
            {{11.0, 11.0}, 100.0},
        },
        {
            {{0.0, 0.0}, {10.0, 10.0}, 0.4},
        },
    };

    const std::vector<GatheringEvent> expected{
        {.item_id = 2, .gatherer_id = 0, .sq_distance = 0.0, .time = 0.1},
        {.item_id = 1, .gatherer_id = 0, .sq_distance = 0.5, .time = 0.25},
        {.item_id = 0, .gatherer_id = 0, .sq_distance = 0.0, .time = 0.8},
    };

    CHECK_THAT(collision_detector::FindGatherEvents(provider), EqualsEvents(expected));
}

TEST_CASE("Every moving gatherer produces its own event for the same item") {
    const VectorItemGathererProvider provider{
        {
            {{0.0, 0.0}, 0.2},
        },
        {
            {{-5.0, 0.0}, {5.0, 0.0}, 0.3},
            {{0.0, 3.0}, {0.0, -1.0}, 0.3},
            {{-8.0, -8.0}, {2.0, 2.0}, 0.3},
        },
    };

    const std::vector<GatheringEvent> expected{
        {.item_id = 0, .gatherer_id = 0, .sq_distance = 0.0, .time = 0.5},
        {.item_id = 0, .gatherer_id = 1, .sq_distance = 0.0, .time = 0.75},
        {.item_id = 0, .gatherer_id = 2, .sq_distance = 0.0, .time = 0.8},
    };

    CHECK_THAT(collision_detector::FindGatherEvents(provider), EqualsEvents(expected));
}

TEST_CASE("Contact at the path endpoints and at the radius boundary is a collision") {
    const VectorItemGathererProvider provider{
        {
            {{0.0, 0.5}, 0.2},
            {{4.0, -0.5}, 0.2},
        },
        {
            {{0.0, 0.0}, {4.0, 0.0}, 0.3},
        },
    };

    const std::vector<GatheringEvent> expected{
        {.item_id = 0, .gatherer_id = 0, .sq_distance = 0.25, .time = 0.0},
        {.item_id = 1, .gatherer_id = 0, .sq_distance = 0.25, .time = 1.0},
    };

    CHECK_THAT(collision_detector::FindGatherEvents(provider), EqualsEvents(expected));
}
