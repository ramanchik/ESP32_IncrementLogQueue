#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"


/**
 * If LOG_SHOW_SENDING_TIME is defined, the system will include information about the sending time in the default log.
 * Delete or comment out this line if you don't need this behavior.
 */
#define LOG_SHOW_SENDING_TIME

/**
 * If LOG_SHOW_SENDING_TIME is defined, the system will include information about the receiving time in the default log.
 * Delete or comment out this line if you don't need this behavior.
 */
#define LOG_SHOW_RECEIVING_TIME

/**
 * INCREMENT_SENDING_PERIOD_MILLIS describes the interval for sending new values, measured in milliseconds.
 */
#define INCREMENT_SENDING_PERIOD_MILLIS		5000

/**
 * QUEUE_LENGTH describes the length of the main queue.
 * The smaller the size of the queue, the more free memory is available for running other tasks, but there's a higher
 * chance of going off course from normal operation when the buffer overflows. Conversely, the larger the size of the queue,
 * the greater the system's stability during operation, but there is less availability for launching other parallel tasks.
 */
#define QUEUE_LENGTH	100

/**
 * If RESTART_IF_STOPPED is defined, the system will automatically restart 3 seconds after stopping (for any reason).
 * Delete or comment out this line if you don't need this behavior.
 */
#define RESTART_IF_STOPPED

/**
 * If STOP_ON_QUEUE_OVERFLOW is defined, the system will be stopped when the main queue overflows.
 * It can be useful in cases where any deviation from normal behavior is unacceptable.
 * Delete or comment out this line if you don't need this behavior.
 */
//#define STOP_ON_QUEUE_OVERFLOW

/**
 * If HOLD_INCREMENT_IF_QUEUE_OVERFLOW is defined, the system will not increments the value if the main queue if full.
 * If HOLD_INCREMENT_IF_QUEUE_OVERFLOW is not defined, the system will increment as usual, but the value will not be sent
 * to the queue and will be lost. This behavior is useful when there is a higher priority for timely generation, logging
 * is not necessary, and tolerance to gaps is acceptable.
 * This parameter depends on STOP_ON_QUEUE_OVERFLOW. When STOP_ON_QUEUE_OVERFLOW is defined, HOLD_INCREMENT_IF_QUEUE_OVERFLOW
 * will be ignored, as in the case of queue overflows, the system will be stopped by STOP_ON_QUEUE_OVERFLOW.
 * Delete or comment out this line if you don't need this behavior.
 */
#define HOLD_INCREMENT_IF_QUEUE_OVERFLOW


/**
 * Struct describes the message object that is sent through the main queue.
 */
typedef struct {
	uint32_t value;
	TickType_t ticks;
} IncrementMessage;


static const char *TAG = "LOG";

/** Main queue and tasks handles */
QueueHandle_t queue;
TaskHandle_t taskIncrementerHandle;
TaskHandle_t taskLoggerHandle;
TaskHandle_t taskStopperHandle;


/**
 * A simple trigger function to initiate the system stopping process.
 * It doesn't perform any stopping actions but only wakes the Stopper-task from suspend mode.
 *
 */
void system_stop() {
	vTaskResume(taskStopperHandle);
}


/**
 * Implementation of stopping the main program process.
 * It stops all tasks, resets the main queue.
 * Depends on RESTART_IF_STOPPED, it may initiate restarting the device or simply stop all processes
 * without taking any further action.
 */
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


/**
 * Utility function for recalculating the delta time in system ticks.
 */
TickType_t update_ticks_period(TickType_t *previousTicksValue) {
	TickType_t currentTicks = xTaskGetTickCount();
	TickType_t ticksDelta = currentTicks - *previousTicksValue;
	*previousTicksValue = currentTicks;
	return ticksDelta;
}


/**
 * Stopper task function.
 * Just waits for resuming to initiate the main program stopping process when it will be needed.
 */
void task_stopper(void *pvParameter)
{
    vTaskDelay(1);
    system_stop_implementation();
}

/**
 * Incrementor task function.
 * Recalculates the increment value periodically and sends it to the queue.
 * It has adjustable behavior dependent on STOP_ON_QUEUE_OVERFLOW and HOLD_INCREMENT_IF_QUEUE_OVERFLOW.
 */
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


/**
 * Logger task function.
 * Retrieves data from the queue and sends it to the default log.
 * It has adjustable behavior dependent on LOG_SHOW_SENDING_TIME and LOG_SHOW_RECEIVING_TIME.
 */
void task_logger(void *pvParameter)
{
	IncrementMessage message;
	TickType_t previousTicksValue = xTaskGetTickCount();

	while(1) {
		/** portMAX_DELAY allows waiting for new elements in the queue without using any delay function. */
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


/**
 * Main application function.
 */
void app_main(void)
{
	/** The stopper task is an utility function that provides a safer way to stop this program
	 * in certain particular cases. This function is in a suspended mode most of the time.
	 * When this task resumes, it will start the program stopping process. */
    BaseType_t stopperTaskResult = xTaskCreate(&task_stopper, "task_stopper", 2048, NULL, 5, &taskStopperHandle);
    if(stopperTaskResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
    	ESP_LOGE(TAG, "Not enough memory to allocate for the task 'stopper'.");
    	system_stop_implementation();
		return;
    }
    vTaskSuspend(taskStopperHandle);


	/** Main queue - to transmit data from the 'incrementor' task to the 'logger' task. */
	queue = xQueueCreate(QUEUE_LENGTH, sizeof(IncrementMessage));
    if(queue == NULL) {
    	ESP_LOGE(TAG, "Not enough memory to allocate for the queue. Try to reduce queue length.");
		system_stop();
		return;
    }

	/** Logger task */
    BaseType_t loggerTaskResult = xTaskCreate(&task_logger, "task_logger", 2048, NULL, 5, &taskLoggerHandle);
    if(loggerTaskResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
    	ESP_LOGE(TAG, "Not enough memory to allocate for the task 'logger'.");
		system_stop();
		return;
    }

	/** Incrementor task */
    BaseType_t incrementerTaskResult = xTaskCreate(&task_incrementer, "task_incrementer", 2048, NULL, 5, &taskIncrementerHandle);
    if(incrementerTaskResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
    	ESP_LOGE(TAG, "Not enough memory to allocate for the task 'incrementer'.");
		system_stop();
		return;
    }
}
