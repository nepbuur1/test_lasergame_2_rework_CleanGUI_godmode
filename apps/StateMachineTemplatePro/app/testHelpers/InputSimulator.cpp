#include "InputSimulator.hpp"
#include "app/MachineControl.hpp"
#include "esp_log.h"

static const char *TAG = "InputSimulator";

/// StateG only continues when Counter::getCounterValue() has reached 3. The Counter
/// increments once every 10 seconds, so that happens about 30 seconds after startup.
static constexpr uint32_t counterPeriodMs = 10000;
static constexpr uint32_t counterValueNeeded = 3;
static constexpr uint32_t counterReadyMs = counterPeriodMs * counterValueNeeded;

/// The name that is typed in StateI. The closing '.' makes StateI continue to StateJ.
static const char *nameToType = "Marius.";

/// The name that is typed in StateJ. It has no closing '.', so StateJ only continues (via StateK) to StateA
/// when its timeout expires: 10 seconds after the last typed character.
static const char *nameToTypeInStateJ = "Bart";
static constexpr uint32_t stateJTimeoutMs = 10000;

/// The telegram that is sent while MachineControl is in StateE. It waits in the telegramQueue,
/// so StateK reads it character by character and re-enters itself until the queue is empty.
static const char *telegramInStateE = "HELLO";

/// The telegram character that is sent while StateK waits on an empty telegramQueue.
static constexpr uint8_t telegramCharacterInStateK = '!';

/**
 * @brief Milliseconds since startup, used to predict the value of the Counter.
 */
static uint32_t msSinceBoot()
{
  return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

InputSimulator::InputSimulator(const char *name, MachineControl &mc,
               UBaseType_t priority, BaseType_t core)
    : machineControl(mc)
{
  xTaskCreatePinnedToCore(taskWrapper, name, 2560, this, priority, &taskHandle, core);
}

void InputSimulator::taskWrapper(void *arg)
{
  InputSimulator *instance = static_cast<InputSimulator *>(arg);
  instance->runTask();
}

void InputSimulator::runTask()
{
  vTaskDelay(pdMS_TO_TICKS(2000)); // Give the state machine time to reach StateA

  while (true)
  {
    // ---------- First round: StateA -> StateB -> StateC -> StateE -> StateK -> StateA ----------
    ESP_LOGI(TAG, "StateA: pressing button1 -> StateB");
    machineControl.button1Pressed();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "StateB: pressing button1 AND button2 -> StateC");
    machineControl.button1Pressed();
    machineControl.button2Pressed();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "StateC: pressing button2 -> StateE");
    machineControl.button2Pressed();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // StateE continues to StateK after 5 seconds. StateK finds the telegram waiting in its queue.
    ESP_LOGI(TAG, "StateE: sending telegram \"%s\"", telegramInStateE);
    for (const char *pCharacter = telegramInStateE; *pCharacter != 0; pCharacter++)
    {
      machineControl.telegramCharacterReceived(*pCharacter);
    }
    vTaskDelay(pdMS_TO_TICKS(5000)); // StateK returns to StateA when the telegramQueue is empty

    // ---------- Second round: StateA -> StateB -> StateC -> StateD -> StateF ----------
    ESP_LOGI(TAG, "StateA: pressing button1 -> StateB");
    machineControl.button1Pressed();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "StateB: pressing button1 AND button2 -> StateC");
    machineControl.button1Pressed();
    machineControl.button2Pressed();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "StateC: pressing button1 -> StateD");
    machineControl.button1Pressed();
    vTaskDelay(pdMS_TO_TICKS(2000)); // StateD continues to StateF after 1 second

    // D->F resets executingCount, so every tour needs three StateF entries.
    for (int pressCount = 1; pressCount <= 3; pressCount++)
    {
      ESP_LOGI(TAG, "StateF: pressing button1 (%d of 3)", pressCount);
      machineControl.button1Pressed();
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // ---------- StateG: press until the Counter has reached 3 ----------
    // Every press before that moment only makes StateG wait for button1 again.
    while (msSinceBoot() < counterReadyMs)
    {
      ESP_LOGI(TAG, "StateG: pressing button1 (counterValue has not reached 3 yet)");
      machineControl.button1Pressed();
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI(TAG, "StateG: pressing button1 -> StateH");
    machineControl.button1Pressed();

    // ---------- StateH: waits for a knob value above 127 and 5 seconds in parallel ----------
    ESP_LOGI(TAG, "StateH: turning knob to 100 (too low, so StateH keeps waiting)");
    machineControl.knobTurned(100);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "StateH: turning knob to 200 -> StateI");
    machineControl.knobTurned(200);

    // ---------- StateI: type the name, the closing '.' continues to StateJ ----------
    vTaskDelay(pdMS_TO_TICKS(500));
    for (const char *pCharacter = nameToType; *pCharacter != 0; pCharacter++)
    {
      ESP_LOGI(TAG, "StateI: typing '%c'", *pCharacter);
      machineControl.keyTyped(*pCharacter);
      vTaskDelay(pdMS_TO_TICKS(300));
    }

    // ---------- StateJ: type a name without '.', then stay silent until the timeout ----------
    // Every typed character restarts the 10 second timer of StateJ, so StateJ returns to
    // StateK 10 seconds after the last character. Wait 1 second longer than that.
    for (const char *pCharacter = nameToTypeInStateJ; *pCharacter != 0; pCharacter++)
    {
      ESP_LOGI(TAG, "StateJ: typing '%c'", *pCharacter);
      machineControl.keyTyped(*pCharacter);
      vTaskDelay(pdMS_TO_TICKS(300));
    }
    ESP_LOGI(TAG, "StateJ: typing nothing for %d seconds -> timeout -> StateK", (int)(stateJTimeoutMs / 1000 + 1));
    vTaskDelay(pdMS_TO_TICKS(stateJTimeoutMs + 1000));

    // ---------- StateK: waits on the (now empty) telegramQueue ----------
    ESP_LOGI(TAG, "StateK: sending telegram character '%c' -> StateA", telegramCharacterInStateK);
    machineControl.telegramCharacterReceived(telegramCharacterInStateK);

    ESP_LOGI(TAG, "Score is %d", machineControl.getScore());
    ESP_LOGI(TAG, "Full tour completed, restarting in StateA");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}
