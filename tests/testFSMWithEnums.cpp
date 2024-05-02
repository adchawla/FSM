#include "FSMWithEnums.h"

#include <gtest/gtest.h>

using with_enums::FSM;

TEST(FSMWithEnums, TestInitialState) {
    FSM fsm;
    logFSM(fsm);

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestPaymentProcessing) {
    FSM fsm;
    fsm.process(CardPresented{"A"});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLED().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", getFare()));
}

TEST(FSMWithEnums, TestPaymentFailed) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentFailed);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::FlashRedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Declined", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Insufficient Funds", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", getFare()));
}

TEST(FSMWithEnums, TestTimeoutOnPaymentProcessing) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLED().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway2", "A", getFare()));
}

TEST(FSMWithEnums, TestLockedFromPaymentFailed) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"}).process(Timeout{});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestPaymentSuccessful) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(eState::PaymentSuccess, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLED().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Fare: 5", fsm.getPOS().getSecondRow());
    EXPECT_EQ("Balance: 25", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestUnlocked) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25}).process(Timeout{});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(eState::Unlocked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLED().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestLockedFromUnlocked) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{}).process(Timeout{}).process(PersonPassed{});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestLockedFromPaymentSuccessful) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{}).process(PersonPassed{});
    logFSM(fsm);

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestBug) {
    FSM fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{}).process(Timeout{}).process(Timeout{}).process(Timeout{});
    EXPECT_EQ(eState::Locked, fsm.getState());
    fsm.process(CardPresented{"A"}).process(Timeout{});
    EXPECT_EQ(eState::PaymentProcessing, fsm.getState());
}