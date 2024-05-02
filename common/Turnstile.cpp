#include "Turnstile.h"

#include <array>

const std::array<std::string, 3> GATEWAYS = {"Gateway1", "Gateway2", "Gateway3"};

const char * to_string(SwingDoor::eStatus e) {
    switch (e) {
    case SwingDoor::eStatus::Closed:
        return "Closed";
    case SwingDoor::eStatus::Open:
        return "Open";
    }
    return "unknown";
}

const char * to_string(LEDController::eStatus e) {
    switch (e) {
    case LEDController::eStatus::RedCross:
        return "RedCross";
    case LEDController::eStatus::FlashRedCross:
        return "FlashRedCross";
    case LEDController::eStatus::GreenArrow:
        return "GreenArrow";
    case LEDController::eStatus::OrangeCross:
        return "OrangeCross";
    }
    return "unknown";
}

const char * to_string(eState e) {
    switch (e) {
    case eState::Locked:
        return "Locked";
    case eState::PaymentProcessing:
        return "PaymentProcessing";
    case eState::PaymentFailed:
        return "PaymentFailed";
    case eState::PaymentSuccess:
        return "PaymentSuccess";
    case eState::Unlocked:
        return "Unlocked";
    }
    return "unknown";
}

int getFare() {
    const auto now = std::time(nullptr);
    const auto calTime = *std::localtime(&now);
    const auto currentHour = calTime.tm_hour;
    constexpr int rates[] = {3, 3, 3, 3, 3, 3, 7, 7, 7, 7, 5, 5, 5, 5, 5, 7, 7, 7, 5, 5, 5, 5, 3, 3};
    return rates[currentHour % 24];
}

void * createTimer(std::function<void()> task, std::chrono::milliseconds duration) {
    return nullptr;
}

void cancelTimer(void * handle) {
}