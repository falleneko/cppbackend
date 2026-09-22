#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "../src/tv.h"

class TVByDefault : public testing::Test {
protected:
    TV tv_;
};
TEST_F(TVByDefault, IsOff) {
    EXPECT_FALSE(tv_.IsTurnedOn());
}
TEST_F(TVByDefault, DoesntShowAChannelWhenItIsOff) {
    EXPECT_FALSE(tv_.GetChannel().has_value());
}
TEST_F(TVByDefault, CantSelectAnyChannel) {
    EXPECT_THROW(tv_.SelectChannel(10), std::logic_error);
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
    tv_.TurnOn();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}
TEST_F(TVByDefault, CantSelectPreviousChannel) {
    EXPECT_THROW(tv_.SelectLastViewedChannel(), std::logic_error);
    tv_.TurnOn();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}

TEST_F(TVByDefault, TurningOffAgainKeepsTVOff) {
    tv_.TurnOff();
    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

// Тестовый стенд "Включенный телевизор"
class TurnedOnTV : public TVByDefault {
protected:
    void SetUp() override {
        tv_.TurnOn();
    }
};
TEST_F(TurnedOnTV, ShowsChannel1) {
    EXPECT_TRUE(tv_.IsTurnedOn());
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}
TEST_F(TurnedOnTV, AfterTurningOffTurnsOffAndDoesntShowAnyChannel) {
    tv_.TurnOff();
    EXPECT_FALSE(tv_.IsTurnedOn());
    // Сравнение с nullopt в GoogleTest выполняется так:
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}
TEST_F(TurnedOnTV, CanSelectChannelFrom1To99) {
    for (int channel = TV::MIN_CHANNEL; channel <= TV::MAX_CHANNEL; ++channel) {
        EXPECT_NO_THROW(tv_.SelectChannel(channel));
        EXPECT_THAT(tv_.GetChannel(), testing::Optional(channel));
    }
}

TEST_F(TurnedOnTV, RejectsChannelsOutsideRangeWithoutChangingState) {
    tv_.SelectChannel(42);
    EXPECT_THROW(tv_.SelectChannel(TV::MIN_CHANNEL - 1), std::out_of_range);
    EXPECT_THROW(tv_.SelectChannel(TV::MAX_CHANNEL + 1), std::out_of_range);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(42));
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}

TEST_F(TurnedOnTV, PreviousChannelSwitchesBetweenLastTwoDifferentChannels) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(12);
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(8));
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(12));
}

TEST_F(TurnedOnTV, SelectingCurrentChannelKeepsPreviousChannel) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(12);
    tv_.SelectChannel(12);
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(8));
}

TEST_F(TurnedOnTV, FirstPreviousChannelIsChannel1) {
    tv_.SelectChannel(8);
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}

TEST_F(TurnedOnTV, TurningOnAgainDoesNotResetChannel) {
    tv_.SelectChannel(42);
    tv_.TurnOn();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(42));
}

TEST_F(TurnedOnTV, TurningOffAndOnRestoresCurrentAndPreviousChannels) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(12);
    tv_.TurnOff();
    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
    EXPECT_THROW(tv_.SelectChannel(5), std::logic_error);
    EXPECT_THROW(tv_.SelectLastViewedChannel(), std::logic_error);
    tv_.TurnOn();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(12));
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(8));
}
