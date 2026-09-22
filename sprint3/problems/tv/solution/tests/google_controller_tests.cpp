#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "../src/controller.h"

using namespace std::literals;

class ControllerWithTurnedOffTV : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_FALSE(tv_.IsTurnedOn());
    }

    void RunMenuCommand(std::string command) {
        input_.str(std::move(command));
        input_.clear();
        menu_.Run();
    }

    void ExpectExtraArgumentsErrorInOutput(std::string_view command) const {
        ExpectOutput(
            "Error: the "s.append(command).append(" command does not require any arguments\n"sv));
    }

    void ExpectEmptyOutput() const {
        ExpectOutput({});
    }

    void ExpectOutput(std::string_view expected) const {
        EXPECT_EQ(output_.str(), std::string{expected});
    }

    TV tv_;
    std::istringstream input_;
    std::ostringstream output_;
    Menu menu_{input_, output_};
    Controller controller_{tv_, menu_};
};

TEST_F(ControllerWithTurnedOffTV, OnInfoCommandPrintsThatTVIsOff) {
    input_.str("Info"s);
    menu_.Run();
    ExpectOutput("TV is turned off\n"sv);
    EXPECT_FALSE(tv_.IsTurnedOn());
}
TEST_F(ControllerWithTurnedOffTV, OnInfoCommandPrintsErrorMessageIfCommandHasAnyArgs) {
    RunMenuCommand("Info some extra args"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("Info"sv);
}
TEST_F(ControllerWithTurnedOffTV, OnInfoCommandWithTrailingSpacesPrintsThatTVIsOff) {
    input_.str("Info  "s);
    menu_.Run();
    ExpectOutput("TV is turned off\n"sv);
}
TEST_F(ControllerWithTurnedOffTV, OnTurnOnCommandTurnsTVOn) {
    RunMenuCommand("TurnOn"s);
    EXPECT_TRUE(tv_.IsTurnedOn());
    ExpectEmptyOutput();
}
TEST_F(ControllerWithTurnedOffTV, OnTurnOnCommandPrintsErrorMessageIfCommandHasAnyArgs) {
    RunMenuCommand("TurnOn some extra args"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("TurnOn"sv);
}
TEST_F(ControllerWithTurnedOffTV, OnTurnOffCommandKeepsTVOff) {
    RunMenuCommand("TurnOff"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectEmptyOutput();
}

TEST_F(ControllerWithTurnedOffTV, OnSelectChannelReportsThatTVIsOff) {
    RunMenuCommand("SelectChannel 8"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectOutput("TV is turned off\n"sv);
}

TEST_F(ControllerWithTurnedOffTV, OnSelectPreviousChannelReportsThatTVIsOff) {
    RunMenuCommand("SelectPreviousChannel"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectOutput("TV is turned off\n"sv);
}

TEST_F(ControllerWithTurnedOffTV, OnSelectPreviousChannelRejectsArguments) {
    RunMenuCommand("SelectPreviousChannel 8"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("SelectPreviousChannel"sv);
}

//-----------------------------------------------------------------------------------

class ControllerWithTurnedOnTV : public ControllerWithTurnedOffTV {
protected:
    void SetUp() override {
        tv_.TurnOn();
    }
};

TEST_F(ControllerWithTurnedOnTV, OnTurnOffCommandTurnsTVOff) {
    RunMenuCommand("TurnOff"s);
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectEmptyOutput();
}
TEST_F(ControllerWithTurnedOnTV, OnTurnOffCommandPrintsErrorMessageIfCommandHasAnyArgs) {
    RunMenuCommand("TurnOff some extra args"s);
    EXPECT_TRUE(tv_.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("TurnOff"sv);
}
TEST_F(ControllerWithTurnedOnTV, OnInfoPrintsCurrentChannel) {
    tv_.SelectChannel(42);
    RunMenuCommand("Info"s);
    ExpectOutput("TV is turned on\nChannel number is 42\n"sv);
}
TEST_F(ControllerWithTurnedOnTV, OnInfoWithArgumentsReportsError) {
    RunMenuCommand("Info extra"s);
    ExpectExtraArgumentsErrorInOutput("Info"sv);
}

TEST_F(ControllerWithTurnedOnTV, OnSelectChannelChangesChannel) {
    RunMenuCommand("SelectChannel 99  "s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(99));
    ExpectEmptyOutput();
}

TEST_F(ControllerWithTurnedOnTV, OnSelectChannelWithoutIntegerReportsInvalidChannel) {
    RunMenuCommand("SelectChannel"s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
    ExpectOutput("Invalid channel\n"sv);
}

TEST_F(ControllerWithTurnedOnTV, OnSelectChannelWithNonIntegerReportsInvalidChannel) {
    RunMenuCommand("SelectChannel abc"s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
    ExpectOutput("Invalid channel\n"sv);
}

TEST_F(ControllerWithTurnedOnTV, OnSelectChannelWithExtraDataReportsInvalidChannel) {
    RunMenuCommand("SelectChannel 8 extra"s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
    ExpectOutput("Invalid channel\n"sv);
}

TEST_F(ControllerWithTurnedOnTV, OnSelectChannelOutsideRangeReportsError) {
    RunMenuCommand("SelectChannel 100"s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
    ExpectOutput("Channel is out of range\n"sv);
}

TEST_F(ControllerWithTurnedOnTV, OnSelectPreviousChannelRestoresPreviousChannel) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(12);
    RunMenuCommand("SelectPreviousChannel"s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(8));
    ExpectEmptyOutput();
}

TEST_F(ControllerWithTurnedOnTV, OnSelectPreviousChannelRejectsArguments) {
    tv_.SelectChannel(8);
    RunMenuCommand("SelectPreviousChannel extra"s);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(8));
    ExpectExtraArgumentsErrorInOutput("SelectPreviousChannel"sv);
}
