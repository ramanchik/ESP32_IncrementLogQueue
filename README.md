# ESP32_IncrementLogQueue


## About this Project  
This project is a demonstration of FreeRTOS functioning on the ESP32-IDF platform, utilizing multitasking, data exchange queues between tasks, and a logging subsystem. Special attention is given to aspects of operational stability and fault tolerance in various scenarios.


## What this Project is Not
The project is not intended as an instructional guide or manual for using specific FreeRTOS or ESP32 functions. It does not serve as a layout or template for any other projects.


## The task
  The initially defined task (with some adjustments) was as follows:

> The firmware should consist of two tasks: One task increments a counter every 5 seconds, sends the increment result to a queue. The second task retrieves messages from the queue and logs them to the standard log output, also logging the time between messages.

Particular emphasis was recommended to be given to the fault tolerance of the code.


## Preliminary Requirements
The ESP32 DEVKIT V1 is used as the target device for deployment and debugging. The ESP-IDF platform and the C programming language are employed for development.


## Environment Setup
In general, the environment setup aligns with the standard process described in [the ESP32-IDF platform guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#introduction).

The basic settings for Linux and macOS platforms can be found by following the [link](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html). Similarly, the environment setup for Windows is also described at the [link](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html).

If setting up the environment for the first time, it is **strongly recommended** to follow all the steps and stages outlined in the guide, **including building and running** the [hello_world test project](https://github.com/espressif/esp-idf/tree/b3f7e2c/examples/get-started/hello_world). This will ensure that there are no errors in the setup process.


## Project Download
  Any attempt to use this project assumes that the installation, configuration, and environment verification stages described above have been successfully completed.

To download the project, standard Git tools can be used. After downloading, the project does not require any modifications, adjustments, or configurations and will work correctly out of the box.


## Building, Flashing, Running
The firmware building, flashing and running stages are standard and are described in the documentation for both [Windows](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html#get-started-windows-first-steps) and [Linux](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#get-started-linux-macos-first-steps), respectively.


## Project Structure and Architecture
All the main code of the project is contained in the main.c file located in the /main directory.

Structurally, the firmware program consists of:
-   queue - the main message exchange queue between the tasks task_incrementer and task_logger.
-   task_incrementer - A task responsible for incrementing a value and sending data to the queue.
-   task_logger - A task responsible for reading messages from the queue and logging them.
-   task_stopper - A utility task used to stop the device's operation in certain cases. Most of the time, this task is in a suspended state and does not perform any actions.

Additionally, the initial part of the main.c file contains a configuration block for the firmware. All configuration parameters and their impact on the system are described in comments within the code itself.


## Behavioral Variability
One of the conditions of the given task was to pay attention to fault tolerance. However, specific criteria and requirements for fault tolerance were not described in the task. Policies for the system's behavior in the event of critical events were also not specified.

As a result, during firmware development, additional attention was given to the configurability of behavior in specific cases.


### Failure Handling
During startup and device operation, events may occur that prevent the program from continuing, leading to the impossibility of further operation.

 * *RESTART_IF_STOPPED* - This parameter allows control over the device's behavior in the event of a stop.

There are two options for behavior:
- In the first option, the execution of all tasks will be terminated, if they are still running. Subsequently, the device will enter a standby mode, perform no actions, and await manual reboot. This behavior is suitable for cases where any exceptional situations require special attention and actions from the operator.
- In the second option, the execution of all tasks will also be stopped, after which the system will wait for 3 seconds and attempt to reboot. This behavior is convenient when the priority is the continuity of operation, regardless of the presence of the system operator.


### Queue Overflow Behavior
The program includes special behavior for cases of queue overflow. In general, given the task conditions, the queue should not overflow. However, it is allowed that future changes to the system may alter operating parameters, task execution times, etc.

 * *STOP_ON_QUEUE_OVERFLOW* - This parameter controls whether to allow the program to continue working in the event of a queue overflow.

There are two main options for behavior upon overflow:
- If it is assumed that queue overflow is not allowed and is critical for the system, there is an option to completely stop the program. The behavior after stopping will be determined by the *RESTART_IF_STOPPED* parameter.
- If queue overflow is not critical and continuing operation is allowed, the system will proceed according to one of two scenarios configured by the *HOLD_INCREMENT_IF_QUEUE_OVERFLOW* parameter.

Also:

* *HOLD_INCREMENT_IF_QUEUE_OVERFLOW* - This parameter controls the behavior of the incrementer in the event of a queue overflow.

There are two options for behavior:
- In one scenario, the incrementing will continue as usual, but the data will not be sent through the queue. This behavior is relevant for a system that requires stability in counting, even in the absence of the ability to process or log data.
-   In another scenario, the incrementing will be held (the counter will be rolled back) until the queue is freed. This option may be relevant when the sequential counting is critically important, even at the expense of time.


### Queue Size
An additional system configuration parameter is the ability to change the size of the queue.

* *QUEUE_LENGTH* - This parameter is responsible for the size of the queue.

Increasing the size of the queue allows the system to work longer in case data accumulates in the buffer. This can be useful if processing data from the queue may take varying amounts of time, causing data to accumulate. The larger the queue size, the more time will pass before a queue overflow event occurs.

In rare cases, it may be necessary to decrease the size of the queue.

One reason for reducing the queue size may be the need for more compact interaction between subtasks and the intolerance of delays. In this case, a short queue will lead to overflow more quickly, triggering one of the predefined overflow behavior scenarios.

Another reason could be to free up occupied memory. This may be required when new tasks are added to be executed in parallel.


## Contacts and Feedback
Within this repository, the publication of personal contacts is not intended. It is assumed that all interested participants already have each other's contact information.
