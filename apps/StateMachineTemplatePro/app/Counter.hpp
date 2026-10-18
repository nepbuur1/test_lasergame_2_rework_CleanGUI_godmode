#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * @brief Counter exposes a count that increments every 10 seconds.
 *
 * This class encapsulates a FreeRTOS task that runs a small state machine.
 * It periodically increments a counter value. It can be read out by other 
 * objects, in this case, it's a MachineControl instance.
 */
class Counter
{
public:
    /**
     * @brief Initializes the counter to zero and starts the FreeRTOS task.
     */
    Counter(); // Constructor starts periodic task

    /**
     * @brief Retrieves the current counter value.
     *
     * @return Current count, initially zero.
     */
    int getCounterValue();

private:
    TaskHandle_t taskHandle;    ///< FreeRTOS task handle for counter task
    QueueHandle_t counterQueue; ///< Queue for buffering counter values, such that other 
                                ///< tasks can safely read them via getCounterValue();

    /**
     * @brief Main FreeRTOS task execution loop.
     *
     * Runs the counter state machine.
     */
    void runTask();

    /**
     * @brief Static wrapper to connect FreeRTOS task entry to C++ instance method.
     *
     * @param arg Pointer to Counter instance.
     */
    static void taskWrapper(void *arg); // FreeRTOS task wrapper
};
