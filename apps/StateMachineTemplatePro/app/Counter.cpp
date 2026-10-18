#include "app/Counter.hpp"
#include "esp_log.h"

static const char *TAG = "Counter";

Counter::Counter() 
{
    counterQueue = xQueueCreate(1, sizeof(int));
    configASSERT(counterQueue != NULL);
    const int initialCounter = 0;
    xQueueOverwrite(counterQueue, &initialCounter);
    // Start the FreeRTOS task for periodic sending
    xTaskCreatePinnedToCore(taskWrapper, "CounterTask", 2048, this, 1, &taskHandle, 0);
}

void Counter::taskWrapper(void *arg)
{
    Counter *instance = static_cast<Counter *>(arg);
    instance->runTask();
}

int Counter::getCounterValue()
{
   int counter = 0;
   xQueuePeek(counterQueue, &counter, 0);
   return counter;
}

void Counter::runTask()
{
    int counter = 0;
    while (true)
    {
        // Waiting for next count.
        vTaskDelay(pdMS_TO_TICKS(10000)); // Delay for 10 seconds
        counter++; 
        xQueueOverwrite(counterQueue, &counter);
        ESP_LOGI(TAG, "Counter value updated to %d", counter);
    }
}
