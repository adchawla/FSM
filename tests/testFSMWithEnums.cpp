// Example program
#include <iostream>
#include <string>

// enums
enum class eArmStatus { Locked, Unlocked };
enum class eDisplayStatus { RedCross, FlashRedCross, GreenArrow, OrangeDash };
enum class eCardReaderStatus { TouchCard, Processing, ApprovedWithInfo, Approved, Declined };
struct CardReaderFields { eCardReaderStatus status; int fare; int balance; };

// actions
void setArmStatus(eArmStatus status);
void setDisplayStatus(eDisplayStatus status);
void setCardReaderMessage(std::string msg);
void connectToServer();

// events
struct CardPresented { std::string cardNumber; };
struct TransactionDeclined { std::string reason; };
struct TransactionSuccess { int fare; int balance; };
struct PersonPassed {};
struct Timeout {};

// states
enum class eStates { Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked };

class FSMWithEnums {
    public:
        void process(CardPresented event);
        void process(TransactionDeclined event);
        void process(TransactionSuccess event);
        void process(PersonPassed event);
        void process(Timeout event);
        
    private:
        eStates _state{eStates::Locked};
        int retryCounts{0};
        std::string cardNumber;

};