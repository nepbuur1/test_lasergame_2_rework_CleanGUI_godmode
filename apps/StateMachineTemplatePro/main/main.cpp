#include "app/MachineControl.hpp"
#include "app/testHelpers/InputSimulator.hpp"
#include "app/Counter.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Keuzes voor CYD
constexpr uint8_t kPinBtn1 = 35;
constexpr uint8_t kPinBtn2 = 22;
constexpr uint8_t kPinLedR = 4;
constexpr uint8_t kPinLedG = 16;
constexpr uint8_t kPinLedB = 17;

extern "C" void app_main()
{

  Counter counter;

  vTaskDelay(pdMS_TO_TICKS(100));

  // Create state machine task (internally creates EventGroup and initializes GPIO)
  MachineControl machineControl(kPinLedR, kPinLedG, kPinLedB, counter);

  // Create the input simulation task and link it to the state machine
  InputSimulator inputSimulator("InputSimulator", machineControl);

  // main does nothing
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
