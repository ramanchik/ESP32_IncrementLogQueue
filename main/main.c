#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"


typedef struct {
	uint32_t value;
	TickType_t ticks;
} IncrementMessage;


static const char *TAG = "LOG";

QueueHandle_t queue;


void task_incrementer(void *pvParameter)
{
	uint32_t value = 0;
	TickType_t previousTicksValue = xTaskGetTickCount();

	while(1){
		value++;

		TickType_t currentTicks = xTaskGetTickCount();
		TickType_t ticksDelta = currentTicks - previousTicksValue;
		previousTicksValue = currentTicks;

		IncrementMessage message = {value, ticksDelta};
		xQueueSend(queue, &message, 0);

		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}


void task_logger(void *pvParameter)
{
	IncrementMessage message;

	while(1) {
		xQueueReceive(queue, &message, portMAX_DELAY);
		ESP_LOGI(TAG, "incremented value: %lu;  sending period: %lu ticks;", message.value, message.ticks);
	}
}


void app_main(void)
{
	queue = xQueueCreate(10, sizeof(IncrementMessage));
	xTaskCreate(&task_logger, "task_logger", 2048, NULL, 5, NULL);
    xTaskCreate(&task_incrementer, "task_incrementer", 2048, NULL, 5, NULL);

}
