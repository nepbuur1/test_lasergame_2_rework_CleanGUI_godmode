#include "app/MachineControl.hpp"
#include "app/Counter.hpp"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_log.h"

static const char *TAG = "MachineControl";

namespace
{
void clearQueue(QueueHandle_t queue, EventGroupHandle_t eventGroup, EventBits_t eventBitQueue)
{
  xQueueReset(queue);
  xEventGroupClearBits(eventGroup, eventBitQueue);
  // A writer may have queued and signaled a new character between reset and clear.
  if (uxQueueMessagesWaiting(queue) > 0)
  {
    xEventGroupSetBits(eventGroup, eventBitQueue);
  }
}

// Read without blocking, then synchronize the event bit with the queue contents.
// Recheck after clearing so a writer between the first check and the clear
// cannot leave a non-empty queue without its event bit.
template<typename T>
bool tryReadQueue(QueueHandle_t queue, T &item,
                  EventGroupHandle_t eventGroup, EventBits_t eventBitQueue)
{
  const bool success = (xQueueReceive(queue, &item, 0) == pdPASS);
  if (uxQueueMessagesWaiting(queue) > 0)
  {
    xEventGroupSetBits(eventGroup, eventBitQueue);
  }
  else
  {
    xEventGroupClearBits(eventGroup, eventBitQueue);
    if (uxQueueMessagesWaiting(queue) > 0)
    {
      xEventGroupSetBits(eventGroup, eventBitQueue);
    }
  }
  return success;
}
}

MachineControl::MachineControl(uint8_t ledPinR, uint8_t ledPinG, uint8_t ledPinB, Counter &counter,
                               UBaseType_t priority, BaseType_t core)
    : pinLEDR(ledPinR), pinLEDG(ledPinG), pinLEDB(ledPinB),
      state(State::StateA), executingCount(0),
      eventBitButton1Pressed(1 << 0), eventBitButton2Pressed(1 << 1), 
      eventBitKnobTurned(1 << 2), eventBitKeyTypedQueue(1 << 3),
      eventBitTimer5sec(1 << 4), eventBitTimer10sec(1 << 5),
      counter(counter), knobValue(0), character(0), telegramCharacter(0), nameLen(0)
{
  nameBuffer[nameLen] = 0;  // closing null character
  knobValueQueue = xQueueCreate(1, sizeof(uint8_t));
  configASSERT(knobValueQueue != NULL);
  keyTypedQueue = xQueueCreate(MAXNAME_SIZE + 1, sizeof(uint8_t)); // +1 for the closing '.'
  configASSERT(keyTypedQueue != NULL);
  // Same type as keyTypedQueue.
  telegramQueue = xQueueCreate(MAXNAME_SIZE + 1, sizeof(uint8_t));
  configASSERT(telegramQueue != NULL);

  // The score is a pool: a queue of length one that is overwritten.
  scoreQueue = xQueueCreate(1, sizeof(int));
  configASSERT(scoreQueue != NULL);
  const int initialScore = 0;
  xQueueOverwrite(scoreQueue, &initialScore);

  eventGroup = xEventGroupCreate();
  configASSERT(eventGroup != NULL);

  // One shot timer: it is (re)started by the state that needs it and its callback
  // sets eventBitTimer5sec, so that the timeout can be awaited in parallel with other events.
  timer5sec = xTimerCreate("Timer5sec", pdMS_TO_TICKS(5000), pdFALSE, this, timer5secCallback);
  configASSERT(timer5sec != NULL);

  // One shot timer for StateJ: (re)started at every entry of StateJ, so that it expires
  // 10 seconds after the last typed character.
  timer10sec = xTimerCreate("Timer10sec", pdMS_TO_TICKS(10000), pdFALSE, this, timer10secCallback);
  configASSERT(timer10sec != NULL);

  gpio_config_t io_conf{};
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.intr_type = GPIO_INTR_DISABLE;

  io_conf.pin_bit_mask = (1ULL << pinLEDR);
  gpio_config(&io_conf);
  gpio_set_level((gpio_num_t)pinLEDR, 0);

  io_conf.pin_bit_mask = (1ULL << pinLEDG);
  gpio_config(&io_conf);
  gpio_set_level((gpio_num_t)pinLEDG, 0);

  io_conf.pin_bit_mask = (1ULL << pinLEDB);
  gpio_config(&io_conf);
  gpio_set_level((gpio_num_t)pinLEDB, 0);

  xTaskCreatePinnedToCore(taskWrapper, "StateMachineTask", 2048, this, priority, &taskHandle, core);
}

void MachineControl::taskWrapper(void *arg)
{
  static_cast<MachineControl *>(arg)->runTask();
}

void MachineControl::timer5secCallback(TimerHandle_t xTimer)
{
  MachineControl *instance = static_cast<MachineControl *>(pvTimerGetTimerID(xTimer));
  xEventGroupSetBits(instance->eventGroup, instance->eventBitTimer5sec);
}

void MachineControl::timer10secCallback(TimerHandle_t xTimer)
{
  MachineControl *instance = static_cast<MachineControl *>(pvTimerGetTimerID(xTimer));
  xEventGroupSetBits(instance->eventGroup, instance->eventBitTimer10sec);
}

void MachineControl::button1Pressed()
{
  xEventGroupSetBits(eventGroup, eventBitButton1Pressed);
}

void MachineControl::button2Pressed()
{
  xEventGroupSetBits(eventGroup, eventBitButton2Pressed);
}

void MachineControl::knobTurned(uint8_t knobValue)
{
  xQueueOverwrite(knobValueQueue, &knobValue);
  xEventGroupSetBits(eventGroup, eventBitKnobTurned);
}

void MachineControl::keyTyped(uint8_t character)
{
  xQueueSend(keyTypedQueue, &character, portMAX_DELAY);
  xEventGroupSetBits(eventGroup, eventBitKeyTypedQueue);
}

void MachineControl::telegramCharacterReceived(uint8_t character)
{
  // StateK waits directly on telegramQueue, so no event bit is needed.
  xQueueSend(telegramQueue, &character, portMAX_DELAY);
}

int MachineControl::getScore()
{
  int score = 0;
  xQueuePeek(scoreQueue, &score, 0);
  return score;
}

void MachineControl::setLEDColor(Color c)
{
  gpio_set_level((gpio_num_t)pinLEDR, 1);
  gpio_set_level((gpio_num_t)pinLEDG, 1);
  gpio_set_level((gpio_num_t)pinLEDB, 1);

  switch (c)
  {
  case Color::Red:
    gpio_set_level((gpio_num_t)pinLEDR, 0);
    break;
  case Color::Green:
    gpio_set_level((gpio_num_t)pinLEDG, 0);
    break;
  case Color::Blue:
    gpio_set_level((gpio_num_t)pinLEDB, 0);
    break;
  case Color::Cyan:
    gpio_set_level((gpio_num_t)pinLEDG, 0);
    gpio_set_level((gpio_num_t)pinLEDB, 0);
    break;
  case Color::Magenta:
    gpio_set_level((gpio_num_t)pinLEDR, 0);
    gpio_set_level((gpio_num_t)pinLEDB, 0);
    break;
  case Color::Yellow:
    gpio_set_level((gpio_num_t)pinLEDR, 0);
    gpio_set_level((gpio_num_t)pinLEDG, 0);
    break;
  }
}

void MachineControl::runTask()
{
  while (true)
  {
    switch (state)
    {
    case State::StateA:
      ESP_LOGI(TAG, "State is StateA");
      ESP_LOGI(TAG, "Waiting for either button1 or button2...");
      setLEDColor(Color::Green);
      xEventGroupWaitBits(eventGroup, eventBitButton1Pressed | eventBitButton2Pressed, pdTRUE, pdFALSE, portMAX_DELAY);
      state = State::StateB;
      break;

    case State::StateB:
      ESP_LOGI(TAG, "State is StateB");
      ESP_LOGI(TAG, "Waiting for button1 AND button2...");
      setLEDColor(Color::Blue);
      xEventGroupWaitBits(eventGroup, eventBitButton1Pressed | eventBitButton2Pressed, pdTRUE, pdTRUE, portMAX_DELAY);
      state = State::StateC;
      break;

    case State::StateC:
      ESP_LOGI(TAG, "State is StateC");
      ESP_LOGI(TAG, "Waiting for button1 -> jump to StateD or button2 -> jump to StateE...");
      setLEDColor(Color::Red);
      uxBits = xEventGroupWaitBits(eventGroup, eventBitButton1Pressed | eventBitButton2Pressed, pdTRUE, pdFALSE, portMAX_DELAY);
      if ((uxBits & eventBitButton1Pressed) != 0)
      {
        state = State::StateD;
      }
      else if ((uxBits & eventBitButton2Pressed) != 0)
      {
        state = State::StateE;
      }
      break;

    case State::StateD:
      ESP_LOGI(TAG, "State is StateD");
      setLEDColor(Color::Cyan);
      vTaskDelay(pdMS_TO_TICKS(1000));
      executingCount = 0;
      state = State::StateF;
      break;

    case State::StateE:
      ESP_LOGI(TAG, "State is StateE");
      setLEDColor(Color::Magenta);
      vTaskDelay(pdMS_TO_TICKS(5000));
      state = State::StateK;
      break;

    case State::StateF:
      ESP_LOGI(TAG, "State is StateF");
      setLEDColor(Color::Yellow);
      executingCount++;
      ESP_LOGI(TAG, "Incrementing executingCount to %d",(int) executingCount);
      xEventGroupWaitBits(eventGroup, eventBitButton1Pressed, pdTRUE, pdFALSE, portMAX_DELAY);
      if (executingCount < 3)
      {
        state = State::StateF;
      }
      else
      {
        // Action of the transition to StateG (StateG re-enters itself, so it cannot be placed there).
        int score;
        xQueuePeek(scoreQueue, &score, 0);
        score++;
        xQueueOverwrite(scoreQueue, &score);
        ESP_LOGI(TAG, "Incrementing score to %d", score);
        state = State::StateG;
      }
      break;

    case State::StateG:
      ESP_LOGI(TAG, "State is StateG");
      setLEDColor(Color::Magenta);
      do
      {
        xEventGroupWaitBits(eventGroup, eventBitButton1Pressed, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "Button1 pressed in StateG, waiting for counterValue >= 3");
      } while (counter.getCounterValue() < 3); // Exit loop when counter value is 3 or more
      state = State::StateH;
      break;

    case State::StateH:
      ESP_LOGI(TAG, "State is StateH");
      ESP_LOGI(TAG, "Waiting for knobTurned [knobValue > 127] or 5 seconds timeout expires, whichever comes first...");
      setLEDColor(Color::Yellow);
      // Every entry, including the self-transition for a low knob value,
      // starts a fresh five-second wait, as specified by the state diagram.
      xEventGroupClearBits(eventGroup, eventBitTimer5sec);
      xTimerStart(timer5sec, portMAX_DELAY);
      uxBits = xEventGroupWaitBits(eventGroup, eventBitTimer5sec | eventBitKnobTurned, pdTRUE, pdFALSE, portMAX_DELAY);
      if ((uxBits & eventBitKnobTurned) != 0)
      {
        xQueuePeek(knobValueQueue, &knobValue, 0);
        ESP_LOGI(TAG, "Knob turned to %d in StateH", (int)knobValue);
        if (knobValue <= 127)
        {
          // StateH -> choice5 -> StateH: re-enter and restart the timer.
          break;
        }
      }
      xTimerStop(timer5sec, portMAX_DELAY);
      // Actions of the transition to StateI (StateI re-enters itself, so they cannot be placed there).
      nameLen = 0;
      nameBuffer[nameLen] = 0;  // closing null character
      setLEDColor(Color::Red);
      clearQueue(keyTypedQueue, eventGroup, eventBitKeyTypedQueue);
      state = State::StateI;
      break;

    case State::StateI:
      ESP_LOGI(TAG, "State is StateI");
      ESP_LOGI(TAG, "Waiting for typed characters, until '.' is typed or %d characters are received...", MAXNAME_SIZE);
      
      do
      {
        xEventGroupWaitBits(eventGroup, eventBitKeyTypedQueue, pdTRUE, pdFALSE, portMAX_DELAY);
      } while (!tryReadQueue(keyTypedQueue, character, eventGroup, eventBitKeyTypedQueue));
      nameBuffer[nameLen++] = character;
      nameBuffer[nameLen] = 0;  // closing null character
      ESP_LOGI(TAG, "Name is now \"%s\"", nameBuffer);
    
      if ((nameLen == MAXNAME_SIZE) || (character == '.'))
      {  
        ESP_LOGI(TAG, "The input was: \"%s\"", nameBuffer);
        // Actions of the transition to StateJ (StateJ re-enters itself, so they cannot be placed there).
        nameLen = 0;
        nameBuffer[nameLen] = 0;  // closing null character
        setLEDColor(Color::Cyan);
        clearQueue(keyTypedQueue, eventGroup, eventBitKeyTypedQueue);
        state = State::StateJ;
      }
      // else re-enter this state.
      break;

    case State::StateJ:
      ESP_LOGI(TAG, "State is StateJ");
      ESP_LOGI(TAG, "Waiting for typed characters, until '.' is typed, %d characters are received or no key is typed for 10 seconds...", MAXNAME_SIZE);
      xEventGroupClearBits(eventGroup, eventBitTimer10sec);
      xTimerStart(timer10sec, portMAX_DELAY);
      do
      {
        uxBits = xEventGroupWaitBits(eventGroup, eventBitKeyTypedQueue | eventBitTimer10sec, pdTRUE, pdFALSE, portMAX_DELAY);
        // A writer can signal just after a transition has reset the queue.
        // An empty keyboard event must not re-enter StateJ or restart its timer.
      } while (((uxBits & eventBitTimer10sec) == 0) &&
               !tryReadQueue(keyTypedQueue, character, eventGroup, eventBitKeyTypedQueue));

      // The CleanRTOS diagram's note checks the timer first if both fired.
      if ((uxBits & eventBitTimer10sec) != 0)
      {
        xTimerStop(timer10sec, portMAX_DELAY);
        ESP_LOGI(TAG, "No key typed for 10 seconds. The input was: \"%s\"", nameBuffer);
        state = State::StateK;
      }
      else // A character was successfully read without blocking.
      {
        nameBuffer[nameLen++] = character;
        nameBuffer[nameLen] = 0;  // closing null character
        ESP_LOGI(TAG, "Name is now \"%s\"", nameBuffer);

        if ((nameLen == MAXNAME_SIZE) || (character == '.'))
        {
          xTimerStop(timer10sec, portMAX_DELAY);
          ESP_LOGI(TAG, "The input was: \"%s\"", nameBuffer);
          state = State::StateK;
        }
        // else re-enter this state, which restarts the 10 second timer.
      }
      break;

    case State::StateK:
      ESP_LOGI(TAG, "State is StateK");
      ESP_LOGI(TAG, "Waiting for a telegram character, until the telegramQueue is empty...");
      // StateK waits for nothing else than telegramQueue, so it can block on the queue itself:
      // no event bit (and no synchronization of that event bit with the queue) is needed.
      xQueueReceive(telegramQueue, &telegramCharacter, portMAX_DELAY);
      ESP_LOGI(TAG, "Telegram character '%c' received", telegramCharacter);
      if (uxQueueMessagesWaiting(telegramQueue) > 0)
      {
        state = State::StateK;
      }
      else
      {
        state = State::StateA;
      }
      break;
    }
  }
}
