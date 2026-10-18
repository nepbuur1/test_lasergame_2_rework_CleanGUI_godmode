#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

class Counter;

/**
 * @brief MachineControl manages system state, LED feedback, and FreeRTOS task logic.
 *
 * This class encapsulates a FreeRTOS task that controls a machine state machine,
 * processes typed characters and knob values, reacts to two button events,
 * and controls RGB LED output.
 */
class MachineControl
{
public:
  /**
   * @brief High-level machine operating states.
   */
  enum class State
  {
    StateA, StateB, StateC, StateD, StateE, StateF, StateG, StateH, StateI, StateJ, StateK
  };

private:
  uint8_t pinLEDR; ///< GPIO pin for red LED channel
  uint8_t pinLEDG; ///< GPIO pin for green LED channel
  uint8_t pinLEDB; ///< GPIO pin for blue LED channel

  State state; ///< Current machine state

  /**
   * @brief LED output colors for visual system feedback.
   */
  enum class Color
  {
    Red,
    Green,
    Blue,
    Cyan,
    Magenta,
    Yellow
  };
  
  uint32_t executingCount; ///< Counts entries into StateF since the last D->F transition
  TaskHandle_t taskHandle;   ///< FreeRTOS task handle for control task
  TimerHandle_t timer5sec; ///< One shot software timer, used to time out waits of 5 seconds
  TimerHandle_t timer10sec; ///< One shot software timer, used to time out the wait for a key in StateJ (10 seconds since the last key)

  EventGroupHandle_t eventGroup;         ///< Event group for system signaling
  EventBits_t eventBitButton1Pressed; ///< Event bit for button 1 press event
  EventBits_t eventBitButton2Pressed; ///< Event bit for button 2 press event
  EventBits_t eventBitKnobTurned; ///< Event bit for for knob turned event
  EventBits_t eventBitKeyTypedQueue; ///< Event bit indicating queued typed characters
  EventBits_t eventBitTimer5sec; ///< Event bit for expiration of the 5 second timer
  EventBits_t eventBitTimer10sec; ///< Event bit for expiration of the 10 second timer
  QueueHandle_t knobValueQueue; ///< Queue for buffering knob values from other tasks
  QueueHandle_t keyTypedQueue; ///< Queue for buffering keyboard key values from other tasks
  QueueHandle_t telegramQueue; ///< Queue for buffering telegram characters from other tasks.
                               ///< StateK waits on this queue only, so it needs no event bit.
  QueueHandle_t scoreQueue; ///< Queue of length one that holds the score, such that other
                            ///< tasks can safely read it via getScore()

  Counter &counter; ///< Reference to the Counter instance

  EventBits_t uxBits;  ///< Variable to hold event bits returned from waits
  uint8_t knobValue;   ///< Variable to hold the knob value read from knobValueQueue
  uint8_t character;   ///< Variable to hold the key value read from keyTypedQueue
  uint8_t telegramCharacter; ///< Variable to hold the character read from telegramQueue

  static constexpr int MAXNAME_SIZE = 10;
  char nameBuffer[MAXNAME_SIZE+1]; // +1 for the closing null character.
  int nameLen;

  /**
   * @brief Main FreeRTOS task execution loop.
   *
   * Handles machine state transitions, queue processing, and LED feedback.
   */
  void runTask();

  /**
   * @brief Static wrapper to connect FreeRTOS task entry to C++ instance method.
   *
   * @param arg Pointer to MachineControl instance.
   */
  static void taskWrapper(void *arg);

  /**
   * @brief Callback of the 5 second software timer.
   *
   * Sets eventBitTimer5sec, such that a state can wait for this timeout
   * simultaneously with other events.
   *
   * @param xTimer Handle of the expired timer; its timer ID points to the MachineControl instance.
   */
  static void timer5secCallback(TimerHandle_t xTimer);

  /**
   * @brief Callback of the 10 second software timer.
   *
   * Sets eventBitTimer10sec, such that StateJ can wait for this timeout
   * simultaneously with the key typed event.
   *
   * @param xTimer Handle of the expired timer; its timer ID points to the MachineControl instance.
   */
  static void timer10secCallback(TimerHandle_t xTimer);

  /**
   * @brief Sets RGB LED output color.
   *
   * @param c Color enum indicating desired LED color.
   */

  void setLEDColor(Color c);

public:
  /**
   * @brief Constructs MachineControl and initializes task, GPIO, queue, and events.
   *
   * @param ledPinR GPIO pin number for red LED.
   * @param ledPinG GPIO pin number for green LED.
   * @param ledPinB GPIO pin number for blue LED.
   * @param counter Counter instance queried in StateG.
   * @param priority FreeRTOS task priority (default = 2).
   * @param core CPU core assignment (default = 0).
   */
  MachineControl(uint8_t ledPinR,
                 uint8_t ledPinG,
                 uint8_t ledPinB,
                 Counter &counter,
                 UBaseType_t priority = 2,
                 BaseType_t core = 0); // Constructor initializes system and starts task

  /**
   * @brief Signals that the first button has been pressed.
   *
   * Typically sets an event bit to trigger system startup behavior.
   */
  void button1Pressed();

  /**
   * @brief Signals that the second button has been pressed.
   *
   * Typically sets an event bit to trigger system startup behavior.
   */
  void button2Pressed();

  /**
   * @brief Signals that a knob has been turned to a new value.
   *
   * Typically writes the value in a QUEUE. 
   * Additionally sets an event bit if the task needs to wait for other events in parallel.
   */
  void knobTurned(uint8_t knobValue);

  /**
   * @brief Queues a typed character and signals that keyboard input is available.
   *
   * @param character Character value, as specified by the class diagram.
   */
  void keyTyped(uint8_t character);

  /**
   * @brief Queues a received telegram character.
   *
   * StateK waits directly on the telegram queue (it waits for nothing else),
   * so no additional event bit is needed.
   *
   * @param character Received telegram character.
   */
  void telegramCharacterReceived(uint8_t character);

  /**
   * @brief Exposes the internal score to other tasks.
   *
   * The score is kept in a queue of length one, such that it can be read safely
   * from another task. It is incremented on the transition from StateF to StateG.
   *
   * @return Current score, initially zero.
   */
  int getScore();
};
