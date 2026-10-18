#include "FakeRtos.hpp"
#include "app/Counter.hpp"
#include "app/MachineControl.hpp"
#include "app/testHelpers/InputSimulator.hpp"
#include <algorithm>
#include <iostream>

using fake::require;
constexpr EventBits_t button1 = 1, button2 = 2, knob = 4, key = 8, timer5 = 16, timer10 = 32;

// Queues in creation order: Counter's counterQueue, then MachineControl's
// knobValueQueue, keyTypedQueue, telegramQueue and scoreQueue.
QueueHandle_t keyQueue() { return fake::runtime().queues.at(2).get(); }
QueueHandle_t telegramQueue() { return fake::runtime().queues.at(3).get(); }

struct Fixture {
    std::unique_ptr<Counter> counter;
    std::unique_ptr<MachineControl> machine;
    explicit Fixture(int count = 3) {
        fake::reset();
        counter = std::make_unique<Counter>();
        if (count) {
            int delays = 0;
            fake::runtime().onDelay = [&](TickType_t delay) {
                require(delay == 10000, "Counter must wait ten seconds per increment");
                if (++delays > count) throw fake::Stop{};
            };
            fake::runTask("CounterTask");
            fake::runtime().onDelay = {};
            require(counter->getCounterValue() == count, "Counter did not reach expected value");
        }
        machine = std::make_unique<MachineControl>(1, 2, 3, *counter);
        // StateK blocks on the empty telegram queue: by default one character lets it continue to StateA.
        fake::runtime().onBlockingReceive = [this](QueueHandle_t queue) {
            require(queue == telegramQueue() && fake::runtime().state == "StateK", "Unexpected blocking receive");
            machine->telegramCharacterReceived('t');
        };
    }
    void route(const fake::Wait &) {
        auto &state = fake::runtime().state;
        if (state == "StateA" || state == "StateC" || state == "StateF" || state == "StateG") machine->button1Pressed();
        else if (state == "StateB") { machine->button1Pressed(); machine->button2Pressed(); }
        else throw std::runtime_error("Unexpected routing state: " + state);
    }
    void toInput(const fake::Wait &wait, bool targetJ = false) {
        auto &state = fake::runtime().state;
        if (state == "StateH") machine->knobTurned(200);
        else if (targetJ && state == "StateI") machine->keyTyped('.');
        else route(wait);
    }
    void run() { fake::runTask("StateMachineTask"); }
};

bool logged(const std::string &line) {
    const auto &logs = fake::runtime().logs;
    return std::find(logs.begin(), logs.end(), line) != logs.end();
}
void checkWait(const fake::Wait &wait, EventBits_t mask, bool all = false) {
    require(wait.mask == mask && wait.all == all && wait.clear && wait.timeout == portMAX_DELAY,
            "Incorrect event wait in " + fake::runtime().state);
}
void checkNonblockingReads() {
    for (const auto &receive : fake::runtime().receives)
        if (receive.queue == keyQueue()) require(receive.timeout == 0, "Keyboard queue receive must be nonblocking");
}
void checkColor(unsigned red, unsigned green, unsigned blue) {
    const auto &pins = fake::runtime().gpio;
    require(pins.at(1) == red && pins.at(2) == green && pins.at(3) == blue, "Incorrect LED output in " + fake::runtime().state);
}

void counterTiming() {
    Fixture f(0);
    require(f.counter->getCounterValue() == 0 && fake::runtime().now == 0, "Initial Counter getter must immediately return zero");
    int delays = 0;
    fake::runtime().onDelay = [&](TickType_t delay) {
        require(delay == 10000, "Counter period differs from model");
        require(f.counter->getCounterValue() == delays, "Counter updated before its delay elapsed");
        if (++delays == 4) throw fake::Stop{};
    };
    fake::runTask("CounterTask");
    require(f.counter->getCounterValue() == 3 && fake::runtime().now == 30000, "Counter increments every ten seconds");
    for (auto timeout : fake::runtime().peekTimeouts) require(timeout == 0, "Counter getter must not block");
}

void buttonsAndBranchE() {
    Fixture f;
    int a = 0, b = 0;
    fake::runtime().onDelay = [&](TickType_t delay) {
        require(fake::runtime().state == "StateE" && delay == 5000, "StateE must return after five seconds");
        checkColor(0, 1, 0);
    };
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        auto &state = fake::runtime().state;
        if (state == "StateA") {
            checkWait(wait, button1 | button2); checkColor(1, 0, 1);
            if (++a == 2) throw fake::Stop{};
            f.machine->button2Pressed();
        } else if (state == "StateB") {
            checkWait(wait, button1 | button2, true); checkColor(1, 1, 0);
            if (++b == 1) f.machine->button1Pressed();
            else { require(b == 2, "StateB failed to keep waiting for both buttons"); f.machine->button2Pressed(); }
        } else if (state == "StateC") {
            checkWait(wait, button1 | button2); checkColor(0, 1, 1); f.machine->button2Pressed();
        } else throw std::runtime_error("Unexpected state in branch E: " + state);
    };
    f.run();
    require(a == 2 && b == 2 && !fake::runtime().blocked.empty(), "A any/B all/C to E route failed");
}

void guardsAndBranchD() {
    Fixture f(2);
    int fWaits = 0, gWaits = 0;
    fake::runtime().onDelay = [&](TickType_t delay) {
        require(fake::runtime().state == "StateD" && delay == 1000, "StateD must wait one second");
        checkColor(1, 0, 0);
    };
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateF") {
            checkWait(wait, button1); checkColor(0, 0, 1); ++fWaits;
            require(logged("Incrementing executingCount to " + std::to_string(fWaits)), "StateF must increment on every entry");
        } else if (state == "StateG") {
            checkWait(wait, button1); checkColor(0, 1, 0);
            require(fWaits == 3, "StateF must require three first-tour button events");
            if (++gWaits == 2) throw fake::Stop{};
        }
        f.route(wait);
    };
    f.run();
    require(gWaits == 2, "Counter below three must keep StateG waiting");
}

void lowKnobRestartsTimeout() {
    Fixture f;
    int h = 0;
    TickType_t firstDeadline = 0;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        auto &rt = fake::runtime();
        if (rt.state == "StateH") {
            checkWait(wait, knob | timer5); checkColor(0, 0, 1);
            auto &timer = fake::timer("Timer5sec");
            if (++h == 1) {
                require(timer.period == 5000 && timer.starts == 1, "StateH must start a five-second timer");
                firstDeadline = timer.due; fake::advance(4000); f.machine->knobTurned(127);
            } else {
                require(h == 2 && timer.starts == 2 && timer.due > firstDeadline, "Low knob self-transition must restart StateH timeout");
                fake::advance(4000);
                require((wait.group->bits & timer5) == 0, "Old timeout incorrectly expired after StateH re-entry");
                fake::advance(1000);
            }
        } else if (rt.state == "StateI") {
            require(h == 2 && !fake::timer("Timer5sec").active, "Restarted timeout must transition to StateI and stop timer");
            checkColor(0, 1, 1); throw fake::Stop{};
        } else f.route(wait);
    };
    f.run();
}

void hTimeoutAndHighKnob() {
    for (bool timeout : {true, false}) {
        Fixture f;
        fake::runtime().onWait = [&](const fake::Wait &wait) {
            const auto &state = fake::runtime().state;
            if (state == "StateH") {
                checkWait(wait, knob | timer5);
                if (timeout) fake::advance(5000); else f.machine->knobTurned(128);
            } else if (state == "StateI") {
                require(!fake::timer("Timer5sec").active, "StateH exit must stop timer");
                checkColor(0, 1, 1); throw fake::Stop{};
            } else f.route(wait);
        };
        f.run();
    }
}

void inputQueueEvents() {
    Fixture f;
    int i = 0;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateI") {
            checkWait(wait, key);
            auto queue = keyQueue();
            switch (++i) {
            case 1: xEventGroupSetBits(wait.group, key); break; // Delayed producer bit after queue reset.
            case 2:
                require(!logged("Name is now \"a\""), "An empty event must not append a character");
                f.machine->keyTyped('a'); f.machine->keyTyped('b'); break;
            case 3:
                require(uxQueueMessagesWaiting(queue) == 1 && (wait.group->bits & key), "Pending queue item must reassert event bit"); break;
            case 4:
                require(uxQueueMessagesWaiting(queue) == 0 && !(wait.group->bits & key), "Empty queue must clear event bit");
                f.machine->keyTyped('.'); break;
            default: throw std::runtime_error("StateI failed to finish on dot");
            }
        } else if (state == "StateJ") {
            checkColor(1, 0, 0);
            require(logged("The input was: \"ab.\""), "StateI must preserve queued character order");
            require(!(wait.group->bits & key), "Transition to StateJ must discard keyboard event");
            throw fake::Stop{};
        } else f.toInput(wait);
    };
    f.run(); checkNonblockingReads();
}

void queueWriterDuringClear() {
    Fixture f;
    int i = 0;
    bool race = false, injected = false;
    fake::runtime().afterClear = [&](EventGroupHandle_t, EventBits_t bits) {
        if (race && bits == key) {
            race = false; injected = true;
            const uint8_t item = 'b';
            // The producer enqueues between the helper's clear and final queue check.
            xQueueSend(keyQueue(), &item, 0);
        }
    };
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateI") {
            if (++i == 1) { f.machine->keyTyped('a'); race = true; }
            else if (i == 2) require(injected && (wait.group->bits & key), "Writer during clear must not lose queue notification");
            else { require(logged("Name is now \"ab\""), "Racing queued character must be processed"); throw fake::Stop{}; }
        } else f.toInput(wait);
    };
    f.run(); checkNonblockingReads();
}

void jSpuriousEventKeepsDeadline() {
    Fixture f;
    int j = 0;
    TickType_t deadline = 0;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateJ") {
            checkWait(wait, key | timer10);
            auto &timer = fake::timer("Timer10sec");
            if (++j == 1) { deadline = timer.due; fake::advance(9000); xEventGroupSetBits(wait.group, key); }
            else {
                require(j == 2 && timer.starts == 1 && timer.due == deadline, "Empty queue event must not restart StateJ timeout");
                fake::advance(1000);
            }
        } else if (state == "StateA" && j) {
            require(logged("No key typed for 10 seconds. The input was: \"\""), "StateJ must finish at original deadline");
            throw fake::Stop{};
        } else f.toInput(wait, true);
    };
    f.run(); checkNonblockingReads();
}

void queueWriterBetweenResetAndClear() {
    Fixture f;
    bool injectBeforeClear = false;
    int resets = 0, i = 0, j = 0;
    fake::runtime().afterReset = [&](QueueHandle_t queue) {
        if (queue == keyQueue()) { ++resets; injectBeforeClear = true; }
    };
    fake::runtime().beforeClear = [&](EventGroupHandle_t, EventBits_t bits) {
        if (injectBeforeClear && bits == key) {
            injectBeforeClear = false;
            // The complete producer call lands after queue reset, before event clear.
            f.machine->keyTyped(resets == 1 ? 'i' : 'j');
        }
    };
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateI") {
            if (++i == 1) require((wait.group->bits & key) != 0, "H->I reset must preserve notification of racing producer");
            else { require(i == 2 && logged("Name is now \"i\""), "H->I racing input was not processed"); f.machine->keyTyped('.'); }
        } else if (state == "StateJ") {
            if (++j == 1) require((wait.group->bits & key) != 0, "I->J reset must preserve notification of racing producer");
            else { require(j == 2 && logged("Name is now \"j\""), "I->J racing input was not processed"); f.machine->keyTyped('.'); }
        } else if (state == "StateA" && j) {
            require(resets == 2, "Expected both keyboard queue reset transitions"); throw fake::Stop{};
        } else f.toInput(wait);
    };
    f.run(); checkNonblockingReads();
}

void jTimerWinsSimultaneousKey() {
    Fixture f;
    bool reachedJ = false;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateJ") { reachedJ = true; f.machine->keyTyped('x'); fake::advance(10000); }
        else if (state == "StateA" && reachedJ) {
            require(!logged("Name is now \"x\""), "StateJ timer must take precedence over simultaneous queued key");
            throw fake::Stop{};
        } else f.toInput(wait, true);
    };
    f.run(); checkNonblockingReads();
}

void jTypedCharactersResetDeadline() {
    Fixture f;
    int j = 0;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateJ") {
            auto &timer = fake::timer("Timer10sec"); ++j;
            require(timer.starts == static_cast<unsigned>(j) && timer.due == fake::runtime().now + 10000,
                    "Each typed character must restart ten-second idle timeout");
            if (j < 3) { fake::advance(9000); f.machine->keyTyped(j == 1 ? 'a' : 'b'); }
            else { require(j == 3, "Unexpected StateJ re-entry"); fake::advance(10000); }
        } else if (state == "StateA" && j) {
            require(logged("No key typed for 10 seconds. The input was: \"ab\""), "StateJ timeout must preserve typed input");
            throw fake::Stop{};
        } else f.toInput(wait, true);
    };
    f.run(); checkNonblockingReads();
}

void inputDotAndLengthLimits() {
    for (bool targetJ : {false, true}) for (const std::string &text : {std::string("."), std::string("0123456789")}) {
        Fixture f;
        bool injected = false;
        fake::runtime().onWait = [&](const fake::Wait &wait) {
            const auto &state = fake::runtime().state;
            const auto target = targetJ ? "StateJ" : "StateI";
            const auto finish = targetJ ? "StateA" : "StateJ";
            if (injected && state == finish) {
                require(logged("The input was: \"" + text + "\""), "Dot/ten-character termination lost input");
                if (targetJ) require(!fake::timer("Timer10sec").active, "Finishing StateJ must stop timer");
                throw fake::Stop{};
            }
            if (state == target) {
                checkWait(wait, targetJ ? key | timer10 : key);
                if (!injected) { injected = true; for (char ch : text) f.machine->keyTyped(ch); }
            } else f.toInput(wait, targetJ);
        };
        f.run(); checkNonblockingReads();
    }
}

void executingCountResetsAcrossTours() {
    Fixture f;
    int visitsJ = 0, firstF = 0, secondF = 0;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateF") {
            const int expected = visitsJ ? ++secondF : ++firstF;
            require(fake::runtime().logs.back() == "Incrementing executingCount to " + std::to_string(expected),
                    "D->F must reset executingCount before the first StateF entry of each tour");
        }
        if (state == "StateG" && visitsJ) {
            require(firstF == 3 && secondF == 3 && !logged("Incrementing executingCount to 4"),
                    "Each tour must need three StateF entries after the D->F reset");
            throw fake::Stop{};
        }
        if (state == "StateJ") { ++visitsJ; fake::advance(10000); }
        else f.toInput(wait, true);
    };
    f.run();
}

void simulatorTiming() {
    Fixture f(0);
    InputSimulator simulator("SimulatorTest", *f.machine);
    int completed = 0;
    unsigned firstF = 0, secondF = 0;
    TickType_t secondFirstFTime = 0;
    fake::runtime().onLog = [&](const std::string &message) {
        if (message.rfind("StateF: pressing button1", 0) == 0) {
            if (!completed) ++firstF;
            else { if (secondF == 0) secondFirstFTime = fake::runtime().now; ++secondF; }
        }
        if (message == "Full tour completed, restarting in StateA") {
            if (++completed == 2) throw fake::Stop{};
            require(fake::runtime().now == 46800, "First simulator tour differs from architecture timing");
            // Spy consumer discards the first tour's keys and telegram characters.
            xQueueReset(keyQueue()); xQueueReset(telegramQueue());
        }
    };
    fake::runTask("SimulatorTest");
    const auto &rt = fake::runtime();
    require(rt.signals.front().time == 2000 && rt.signals.front().bits == button1, "Simulator must initially wait two seconds");
    require(firstF == 3 && secondF == 3 && secondFirstFTime == 61800 && rt.now == 80600,
            "Simulator must send three StateF presses every tour after the D->F counter reset");
    std::string keys, telegrams;
    std::vector<fake::SentItem> keySent, telegramSent;
    for (const auto &sent : rt.sent) {
        if (sent.queue == keyQueue()) { keys += static_cast<char>(sent.value); keySent.push_back(sent); }
        else if (sent.queue == telegramQueue()) { telegrams += static_cast<char>(sent.value); telegramSent.push_back(sent); }
    }
    require(keys == "Marius.BartMarius.Bart", "Simulator typed names differ from models");
    require(keySent[0].time == 32500 && keySent[7].time == 34600 && keySent[10].time == 35500,
            "Simulator's half-second StateI entry and 300ms character cadence differ from models");
    require(telegrams == "HELLO!HELLO!", "Simulator telegrams differ from models");
    require(telegramSent[0].time == 5000 && telegramSent[4].time == 5000 && telegramSent[5].time == 46800 &&
            telegramSent[6].time == 52800 && telegramSent[11].time == 80600,
            "Simulator must send the telegram one second into StateE and '!' after the StateJ timeout");
    require(logged("Score is 0"), "Simulator must print the score at the end of every tour");
    std::vector<fake::SentItem> knobWrites;
    for (const auto &write : rt.overwritten) if (write.queue->itemSize == 1) knobWrites.push_back(write);
    require(knobWrites.size() == 4 && knobWrites[0].time == 31000 && knobWrites[0].value == 100 &&
            knobWrites[1].time == 32000 && knobWrites[1].value == 200,
            "Simulator must send low/high knob values one second apart after StateG");
}

void telegramQueueDrainedInStateK() {
    Fixture f;
    int a = 0;
    size_t signalsBefore = 0, signalsAfter = 0;
    fake::runtime().onBlockingReceive = [](QueueHandle_t) { throw std::runtime_error("StateK must not block on a filled queue"); };
    fake::runtime().onDelay = [&](TickType_t delay) {
        require(fake::runtime().state == "StateE" && delay == 5000, "StateE must continue after five seconds");
        signalsBefore = fake::runtime().signals.size();
        for (char ch : std::string("abc")) f.machine->telegramCharacterReceived(ch);
        signalsAfter = fake::runtime().signals.size();
    };
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateA") { if (++a == 2) throw fake::Stop{}; f.machine->button1Pressed(); }
        else if (state == "StateC") f.machine->button2Pressed();
        else f.route(wait);
    };
    f.run();
    const auto &logs = fake::runtime().logs;
    require(signalsAfter == signalsBefore, "telegramCharacterReceived must not set an event bit");
    require(std::count(logs.begin(), logs.end(), "State is StateK") == 3, "StateK must re-enter itself until the queue is empty");
    const auto first = std::find(logs.begin(), logs.end(), "Telegram character 'a' received");
    require(std::distance(first, logs.end()) > 6 &&
            *std::next(first, 3) == "Telegram character 'b' received" &&
            *std::next(first, 6) == "Telegram character 'c' received", "StateK must read the telegram in order");
    require(uxQueueMessagesWaiting(telegramQueue()) == 0, "StateK must leave an empty telegram queue");
    for (const auto &receive : fake::runtime().receives)
        if (receive.queue == telegramQueue()) require(receive.timeout == portMAX_DELAY, "StateK must wait directly on the telegram queue");
}

void telegramWaitBlocksInStateK() {
    Fixture f;
    int blocked = 0;
    bool reachedK = false;
    fake::runtime().onBlockingReceive = [&](QueueHandle_t queue) {
        require(queue == telegramQueue() && fake::runtime().state == "StateK", "Only StateK may block on the telegram queue");
        ++blocked; f.machine->telegramCharacterReceived('x');
    };
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateJ") { reachedK = true; fake::advance(10000); }
        else if (state == "StateA" && reachedK) {
            require(blocked == 1 && logged("Telegram character 'x' received"), "StateK must wait for a telegram character");
            throw fake::Stop{};
        } else f.toInput(wait, true);
    };
    f.run();
}

void scoreIncrementsOnFToG() {
    Fixture f;
    require(f.machine->getScore() == 0, "Score must initially be zero");
    int g = 0;
    fake::runtime().onWait = [&](const fake::Wait &wait) {
        const auto &state = fake::runtime().state;
        if (state == "StateF") require(f.machine->getScore() == g, "Score must not change in StateF");
        if (state == "StateG") {
            ++g;
            require(f.machine->getScore() == g && logged("Incrementing score to " + std::to_string(g)),
                    "Every F->G transition must increment the score");
            if (g == 2) throw fake::Stop{};
        }
        if (state == "StateJ") fake::advance(10000);
        else f.toInput(wait, true);
    };
    f.run();
    for (auto timeout : fake::runtime().peekTimeouts) require(timeout == 0, "getScore must not block");
}

int main() {
    const std::vector<std::pair<const char *, void (*)()>> cases{
        {"Counter initial value and ten-second increments", counterTiming},
        {"StateA any/StateB all/buttons and StateC -> StateE", buttonsAndBranchE},
        {"StateC -> StateD, StateF guard, Counter < 3 in StateG", guardsAndBranchD},
        {"Low knob restarts StateH five-second timeout", lowKnobRestartsTimeout},
        {"StateH timeout/high knob and Counter >= 3", hTimeoutAndHighKnob},
        {"StateI nonblocking queue/event synchronization", inputQueueEvents},
        {"Queue writer during event clear", queueWriterDuringClear},
        {"Queue writer between reset and event clear", queueWriterBetweenResetAndClear},
        {"StateJ empty event keeps deadline", jSpuriousEventKeepsDeadline},
        {"StateJ timer wins simultaneous key", jTimerWinsSimultaneousKey},
        {"StateJ typed characters restart idle timer", jTypedCharactersResetDeadline},
        {"StateI/StateJ dot and ten-character limits", inputDotAndLengthLimits},
        {"Executing count resets on D->F every tour", executingCountResetsAcrossTours},
        {"StateK reads the telegram queue until it is empty", telegramQueueDrainedInStateK},
        {"StateK waits directly on an empty telegram queue", telegramWaitBlocksInStateK},
        {"Score increments on F->G and is exposed via getScore", scoreIncrementsOnFToG},
        {"InputSimulator tour timing and repeated tour", simulatorTiming},
    };
    int failures = 0;
    for (const auto &test : cases) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception &ex) { ++failures; std::cerr << "FAIL " << test.first << ": " << ex.what() << '\n'; }
    }
    std::cout << cases.size() - failures << '/' << cases.size() << " behavior tests passed\n";
    return failures ? 1 : 0;
}
