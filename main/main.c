#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"


#define LOG_SHOW_SENDING_TIME
#define LOG_SHOW_RECEIVING_TIME

#define INCREMENT_SENDING_PERIOD_MILLIS		5000

#define QUEUE_LENGTH	100

#define RESTART_IF_STOPPED

//#define STOP_ON_QUEUE_OVERFLOW
#define HOLD_INCREMENT_IF_QUEUE_OVERFLOW


typedef struct {
	uint32_t value;
	TickType_t ticks;
} IncrementMessage;


static const char *TAG = "LOG";

QueueHandle_t queue;
TaskHandle_t taskIncrementerHandle;
TaskHandle_t taskLoggerHandle;
TaskHandle_t taskStopperHandle;


void system_stop() {
	vTaskResume(taskStopperHandle);
}

void system_stop_implementation() {
	if(taskLoggerHandle != NULL) {
		vTaskDelete(taskLoggerHandle);
	}
	if(taskIncrementerHandle != NULL) {
		vTaskDelete(taskIncrementerHandle);
	}
	if(queue != NULL) {
		xQueueReset(queue);
	}
	ESP_LOGE(TAG, "Stopped!");

#ifdef RESTART_IF_STOPPED
	ESP_LOGE(TAG, "The device will be restarted in a 3 seconds...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
	ESP_LOGE(TAG, "Restarting now.");
    fflush(stdout);
	esp_restart();
#else
	while(1) {
		vTaskDelay(1);
		// Do nothing
	}
#endif
}


TickType_t update_ticks_period(TickType_t *previousTicksValue) {
	TickType_t currentTicks = xTaskGetTickCount();
	TickType_t ticksDelta = currentTicks - *previousTicksValue;
	*previousTicksValue = currentTicks;
	return ticksDelta;
}


void task_stopper(void *pvParameter)
{
    vTaskDelay(1);
    system_stop_implementation();
}


void task_incrementer(void *pvParameter)
{
	uint32_t value = 0;
	TickType_t previousTicksValue = xTaskGetTickCount();
    TickType_t lastWakeTime = xTaskGetTickCount();

	while(1){
		value++;

		TickType_t ticksDelta = update_ticks_period(&previousTicksValue);

		IncrementMessage message = {value, ticksDelta};
		BaseType_t queueSendingResult = xQueueSend(queue, &message, 0);

		if(queueSendingResult == errQUEUE_FULL){
#ifdef STOP_ON_QUEUE_OVERFLOW
			ESP_LOGW(TAG, "Queue is full. Can't send new message. Device will be stopped.");
			system_stop();
#else
#ifdef HOLD_INCREMENT_IF_QUEUE_OVERFLOW
			value--;
			ESP_LOGW(TAG, "Queue is full. Can't send new message. Increment value is not changed.");
#else
			ESP_LOGW(TAG, "Queue is full. Can't send new message. Just continue.");
#endif
#endif
		}

		vTaskDelayUntil(&lastWakeTime, INCREMENT_SENDING_PERIOD_MILLIS / portTICK_PERIOD_MS);
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
		snprintf(sendingTimeString, sizeof(sendingTimeString), "  sending time: %lums;",  message.ticks * portTICK_PERIOD_MS);
#endif

#ifdef LOG_SHOW_RECEIVING_TIME
		snprintf(receivingTimeString, sizeof(receivingTimeString), "  receiving time: %lums;", ticksDelta * portTICK_PERIOD_MS);
#endif

		ESP_LOGI(TAG, "incremented value: %lu;%s%s", message.value, sendingTimeString, receivingTimeString);
	}
}


void app_main(void)
{
    BaseType_t stopperTaskResult = xTaskCreate(&task_stopper, "task_stopper", 2048, NULL, 5, &taskStopperHandle);
    if(stopperTaskResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
    	ESP_LOGE(TAG, "Not enough memory to allocate for the task 'stopper'.");
    	system_stop_implementation();
		return;
    }
    vTaskSuspend(taskStopperHandle);



	queue = xQueueCreate(QUEUE_LENGTH, sizeof(IncrementMessage));
    if(queue == NULL) {
    	ESP_LOGE(TAG, "Not enough memory to allocate for the queue. Try to reduce queue length.");
		system_stop();
		return;
    }

    BaseType_t loggerTaskResult = xTaskCreate(&task_logger, "task_logger", 2048, NULL, 5, &taskLoggerHandle);
    if(loggerTaskResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
    	ESP_LOGE(TAG, "Not enough memory to allocate for the task 'logger'.");
		system_stop();
		return;
    }

    BaseType_t incrementerTaskResult = xTaskCreate(&task_incrementer, "task_incrementer", 2048, NULL, 5, &taskIncrementerHandle);
    if(incrementerTaskResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
    	ESP_LOGE(TAG, "Not enough memory to allocate for the task 'incrementer'.");
		system_stop();
		return;
    }
}
