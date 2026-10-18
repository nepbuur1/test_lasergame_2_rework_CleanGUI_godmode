#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class MachineControl;

/**
 * @brief InputSimulator feeds MachineControl with simulated user input.
 *
 * It SIMULATES button presses, knob turns, typed keys and received telegram characters
 * for demonstration purposes. The generated sequence navigates MachineControl through
 * all of its states (StateA .. StateK). It types the name "Marius." in StateI and the
 * name "Bart" (without closing '.') in StateJ, after which it stays silent until the
 * 10 second timeout of StateJ expires.
 *
 * During StateE it sends the telegram "HELLO", which StateK finds waiting in its queue
 * and reads character by character. After the timeout of StateJ, StateK finds an empty
 * queue and waits until InputSimulator sends the single telegram character '!'.
 * At the end of every tour it prints the score of MachineControl.
 */

class InputSimulator
{
private:
  TaskHandle_t taskHandle; ///< FreeRTOS task handle for button task
  MachineControl &machineControl; ///< Reference to system controller

  /**
   * @brief Main FreeRTOS task loop that generates the simulated input.
   *
   * Repeatedly walks MachineControl through all states of its state diagram.
   */
  void runTask();

  /**
   * @brief Static wrapper to link FreeRTOS task entry to C++ instance method.
   *
   * @param arg Pointer to InputSimulator instance.
   */
  static void taskWrapper(void* arg);

public:
  /**
   * @brief Constructs InputSimulator and starts the FreeRTOS simulation task.
   *
   * @param name Human-readable task name.
   * @param mc Reference to MachineControl for event signaling.
   * @param priority FreeRTOS task priority (default = 1).
   * @param core CPU core assignment (default = 0).
   */
  InputSimulator(const char* name,
         MachineControl &mc,
         UBaseType_t priority = 1,
         BaseType_t core = 0);
};
