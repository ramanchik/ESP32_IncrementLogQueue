#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"


//#define LOG_SHOW_SENDING_TIME
#define LOG_SHOW_RECEIVING_TIME



typedef struct {
	uint32_t value;
	TickType_t ticks;
} IncrementMessage;


static const char *TAG = "LOG";

QueueHandle_t queue;


TickType_t update_ticks_period(TickType_t *previousTicksValue) {
	TickType_t currentTicks = xTaskGetTickCount();
	TickType_t ticksDelta = currentTicks - *previousTicksValue;
	*previousTicksValue = currentTicks;
	return ticksDelta;
}


void task_incrementer(void *pvParameter)
{
	uint32_t value = 0;
	TickType_t previousTicksValue = xTaskGetTickCount();

	while(1){
		value++;

		TickType_t ticksDelta = update_ticks_period(&previousTicksValue);

		IncrementMessage message = {value, ticksDelta};
		xQueueSend(queue, &message, 0);

		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}


void task_logger(void *pvParameter)
{
	IncrementMessage message;
	TickType_t previousTicksValue = xTaskGetTickCount();

	while(1) {
		xQueueReceive(queue, &message, portMAX_DELAY);

		TickType_t ticksDelta = update_ticks_period(&previousTicksValue);

		char sendingTimeString[40] = "";
		char receivingTimeString[40] = "";

#ifdef LOG_SHOW_SENDING_TIME
		snprintf(sendingTimeString, sizeof(sendingTimeString), "  sending time: %lu ticks;",  message.ticks);
#endif

#ifdef LOG_SHOW_RECEIVING_TIME
		snprintf(receivingTimeString, sizeof(receivingTimeString), "  receiving time: %lu ticks;", ticksDelta);
#endif

		ESP_LOGI(TAG, "incremented value: %lu;%s%s", message.value, sendingTimeString, receivingTimeString);
	}
}


void app_main(void)
{
	queue = xQueueCreate(10, sizeof(IncrementMessage));
	xTaskCreate(&task_logger, "task_logger", 2048, NULL, 5, NULL);
    xTaskCreate(&task_incrementer, "task_incrementer", 2048, NULL, 5, NULL);

}
