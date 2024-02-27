/*
 * FreeRTOS V202212.01
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard includes. */
#include <stdio.h>
#include <string.h>
#include "timers.h"
#include "semphr.h"

/* printf() output uses the UART.  These constants define the addresses of the
required UART registers. */
#define UART0_ADDRESS 	( 0x40004000UL )
#define UART0_DATA		( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 0UL ) ) ) )
#define UART0_STATE		( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 4UL ) ) ) )
#define UART0_CTRL		( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 8UL ) ) ) )
#define UART0_BAUDDIV	( * ( ( ( volatile uint32_t * )( UART0_ADDRESS + 16UL ) ) ) )
#define TX_BUFFER_MASK	( 1UL )

#define mainINTERRUPT_NUMBER 3
#define TASK_STACK_SIZE 1000
#define NUM_TASKS 3

/*
 * Printf() output is sent to the serial port.  Initialise the serial hardware.
 */
static void prvUARTInit( void );

/* Task configuration structures */
typedef struct 
{
    TaskHandle_t handle;
    const char *name;
    uint32_t period;
	uint32_t releaseTime;
	uint32_t responseTime;
	struct TaskInfo *next;
} TaskInfo;

/* Function declarations */
static void vHighPriorityPeriodicTask();
static void vMediumPriorityPeriodicTask();
static void vLowPriorityPeriodicTask();
static void vMsDelay(uint32_t ms, uint32_t taskTickCount);
static void addTaskInstance(TaskInfo **taskList, TaskInfo currentTask);
static void measureMetrics(void);
static void vTimerCallback(TimerHandle_t pxTimer);
static uint32_t countItems(TaskInfo *head);

// Global list pointers for each type of task
TaskInfo *highPriorityTasks = NULL;
TaskInfo *mediumPriorityTasks = NULL;
TaskInfo *lowPriorityTasks = NULL;


#define HIGH_PRIORITY_PERIODIC_TASK_EXECUTION_TIME_MS 18
#define HIGH_PRIORITY_PERIODIC_TASK_PERIOD_MS  70 

#define MEDIUM_PERIODIC_TASK_EXECUTION_TIME_MS 28
#define MEDIUM_PRIORITY_PERIODIC_TASK_PERIOD_MS 80

#define LOW_PRIORITY_PERIODIC_TASK_EXECUTION_TIME_MS 38
#define LOW_PRIORITY_PERIODIC_TASK_PERIOD_MS 100


// Additional constants
#define TIMER_DURATION_MS 200  // Measurement period in milliseconds

// Timer handlers
TimerHandle_t xStopTimer;

/*-----------------------------------------------------------*/

int main( void )
{
	/* See https://www.freertos.org/freertos-on-qemu-mps2-an385-model.html for
	instructions. */


	/* Hardware initialisation.  printf() output uses the UART for IO. */
	prvUARTInit();

    xTaskCreate(vHighPriorityPeriodicTask, "vHighPriorityPeriodicTask", TASK_STACK_SIZE, NULL,
                        configMAX_PRIORITIES-3, NULL);

    xTaskCreate(vMediumPriorityPeriodicTask, "vMediumPriorityPeriodicTask", TASK_STACK_SIZE, NULL,
               			configMAX_PRIORITIES-3, NULL);

    xTaskCreate(vLowPriorityPeriodicTask, "vLowPriorityPeriodicTask", TASK_STACK_SIZE, NULL,
  						configMAX_PRIORITIES-4, NULL);	                    


    xStopTimer = xTimerCreate("StopTimer", pdMS_TO_TICKS(TIMER_DURATION_MS), pdFALSE, 0, vTimerCallback);
    xTimerStart(xStopTimer, 0);
	
	vTaskStartScheduler();

	// Code will only reach here if there is an error on starting the scheduler
    printf("Error starting FreeRTOS scheduler!\n");


	// Terminate the program
	vTaskEndScheduler();

	return 0;
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	printf( "\r\n\r\nStack overflow in %s\r\n", pcTaskName );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vAssertCalled( const char *pcFileName, uint32_t ulLine )
{
volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

	/* Called if an assertion passed to configASSERT() fails.  See
	http://www.freertos.org/a00110.html#configASSERT for more information. */

	printf( "ASSERT! Line %d, file %s\r\n", ( int ) ulLine, pcFileName );

 	taskENTER_CRITICAL();
	{
		/* You can step out of this function to debug the assertion by using
		the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
		value. */
		while( ulSetToNonZeroInDebuggerToContinue == 0 )
		{
			__asm volatile( "NOP" );
			__asm volatile( "NOP" );
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[TASK_STACK_SIZE];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = TASK_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

static void prvUARTInit( void )
{
	UART0_BAUDDIV = 16;
	UART0_CTRL = 1;
}
/*-----------------------------------------------------------*/

static void vHighPriorityPeriodicTask() 
{
	TickType_t xLastWakeTime = 0;
	TaskInfo taskInstance;
	taskInstance.name = "HighPriorityPeriodicTask";
	taskInstance.period = HIGH_PRIORITY_PERIODIC_TASK_PERIOD_MS;
	uint32_t taskTickCount = 0;
	TickType_t xStartTime;

	for( ;; ) 
	{	
		xStartTime = xTaskGetTickCount();
		taskInstance.releaseTime = pdTICKS_TO_MS(xLastWakeTime);

        // Simulate a computation time 
        vMsDelay(HIGH_PRIORITY_PERIODIC_TASK_EXECUTION_TIME_MS, taskTickCount);
		printf("Task 1 executed\n");

		TickType_t xFinishTime = xTaskGetTickCount();
		taskInstance.responseTime = pdTICKS_TO_MS(xFinishTime) - pdTICKS_TO_MS(xLastWakeTime);	

		// Add the new task instance to the list
		addTaskInstance(&highPriorityTasks, taskInstance);

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(HIGH_PRIORITY_PERIODIC_TASK_PERIOD_MS));
	}
}

static void vMediumPriorityPeriodicTask() 
{																							
	TickType_t xLastWakeTime = 0;
	TaskInfo taskInstance;
	taskInstance.name = "MediumPriorityPeriodicTask";
	taskInstance.period = MEDIUM_PRIORITY_PERIODIC_TASK_PERIOD_MS;
	uint32_t taskTickCount = 0;
	TickType_t xStartTime;

	for( ;; ) 
	{	
		xStartTime = xTaskGetTickCount();
		taskInstance.releaseTime = pdTICKS_TO_MS(xLastWakeTime);

        // Simulate a computation time 
        vMsDelay(MEDIUM_PERIODIC_TASK_EXECUTION_TIME_MS, taskTickCount);
		printf("Task 2 executed\n");

		TickType_t xFinishTime = xTaskGetTickCount();
		taskInstance.responseTime = pdTICKS_TO_MS(xFinishTime) - pdTICKS_TO_MS(xLastWakeTime);

		// Add the new task instance to the list
		addTaskInstance(&mediumPriorityTasks, taskInstance);

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MEDIUM_PRIORITY_PERIODIC_TASK_PERIOD_MS));
	}
   
}

static void vLowPriorityPeriodicTask() 
{
    TickType_t xLastWakeTime = 0;
	TaskInfo taskInstance;
	taskInstance.name = "LowPriorityPeriodicTask";
	taskInstance.period = LOW_PRIORITY_PERIODIC_TASK_PERIOD_MS;
	uint32_t taskTickCount = 0;
	TickType_t xStartTime;
	for ( ;; ) 
	{	
		xStartTime = xTaskGetTickCount();
		taskInstance.releaseTime = pdTICKS_TO_MS(xLastWakeTime);

        // Simulate a computation time
        vMsDelay(LOW_PRIORITY_PERIODIC_TASK_EXECUTION_TIME_MS, taskTickCount);
		printf("Task 3 executed\n");

		TickType_t xFinishTime = xTaskGetTickCount();

		taskInstance.responseTime = pdTICKS_TO_MS(xFinishTime) - pdTICKS_TO_MS(xLastWakeTime);

		// Add the new task instance to the list
		addTaskInstance(&lowPriorityTasks, taskInstance);

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(LOW_PRIORITY_PERIODIC_TASK_PERIOD_MS));
    }
}

static void vMsDelay(uint32_t milliseconds, uint32_t taskTickCount)
{
	TickType_t currentTime = xTaskGetTickCount();
    while (taskTickCount < milliseconds) {
		taskTickCount++;
		while(xTaskGetTickCount() == currentTime);
		currentTime = xTaskGetTickCount();
	}
}

// Function to add a new instance to the task list
static void addTaskInstance(TaskInfo **taskList, TaskInfo currentTask) { 
    TaskInfo *newTask = (TaskInfo *)pvPortMalloc(sizeof(TaskInfo));
    if (newTask != NULL) {
        // Copy task properties from currentTask to the new task instance
        memcpy(newTask, &currentTask, sizeof(TaskInfo));
		newTask->next = *taskList;
		*taskList = newTask;
    }
}


static void measureMetrics(void) {
	uint32_t highPriorityTasksInstances = countItems(highPriorityTasks);
	uint32_t mediumPriorityTasksInstances = countItems(mediumPriorityTasks);
	uint32_t lowPriorityTasksInstances = countItems((int)lowPriorityTasks);

	printf("\nTotal instances high priority task : %d\n", highPriorityTasksInstances);
	printf("Total instances medium priority task: %d\n", mediumPriorityTasksInstances);
	printf("Total instances low priority task: %d\n", lowPriorityTasksInstances);


	TaskInfo *currentHighPriorityTask = highPriorityTasks;
	TaskInfo *currentMediumPriorityTask = mediumPriorityTasks;
	TaskInfo *currentLowPriorityTask = lowPriorityTasks;

	uint32_t totalDeadlineMissesHigh = 0;
	uint32_t totalDeadlineMissesMedium = 0;
	uint32_t totalDeadlineMissesLow = 0;

	uint32_t totalResponseTimeHigh = 0;
	uint32_t totalResponseTimeMedium = 0;
	uint32_t totalResponseTimeLow = 0;

	for (uint32_t i = 0; i < highPriorityTasksInstances; i++){
		// Check for deadline miss
        if (currentHighPriorityTask->responseTime > currentHighPriorityTask->period) {
            totalDeadlineMissesHigh++;
        }
		totalResponseTimeHigh += currentHighPriorityTask->responseTime;
		currentHighPriorityTask = currentHighPriorityTask->next;
	}

	for (uint32_t i = 0; i<  mediumPriorityTasksInstances; i++){
		// Check for deadline miss
        if (currentMediumPriorityTask->responseTime > currentMediumPriorityTask->period) {
            totalDeadlineMissesMedium++;
        }
		totalResponseTimeMedium += currentMediumPriorityTask->responseTime;
		currentMediumPriorityTask = currentMediumPriorityTask->next;
	}

	for (uint32_t i = 0; i < lowPriorityTasksInstances; i++){
		// Check for deadline miss
        if (currentLowPriorityTask->responseTime> currentLowPriorityTask->period) {
            totalDeadlineMissesLow++;
        }
		totalResponseTimeLow += currentLowPriorityTask->responseTime;
		currentLowPriorityTask = currentLowPriorityTask->next;
	}

	printf("\nTotal response time high priority task : %u\n", totalResponseTimeHigh);
	printf("Total response time medium priority task : %u\n", totalResponseTimeMedium);
	printf("Total response time low priority task : %u\n", totalResponseTimeLow);


	// Calculate average response time for each type of task
    double avgResponseTimeHigh = (double)(totalResponseTimeHigh / highPriorityTasksInstances);
    double avgResponseTimeMedium = (double)(totalResponseTimeMedium / mediumPriorityTasksInstances);
    double avgResponseTimeLow = (double)(totalResponseTimeLow / lowPriorityTasksInstances);

	// Print metrics
    printf("\nHigh Priority Task Metrics:\n");
    printf("Total Deadline Misses: %u\n", totalDeadlineMissesHigh);
    printf("Average Response Time: %f ms\n", avgResponseTimeHigh);

    printf("\nMedium Priority Task Metrics:\n");
    printf("Total Deadline Misses: %u\n", totalDeadlineMissesMedium);
    printf("Average Response Time: %f ms\n", avgResponseTimeMedium);

    printf("\nLow Priority Task Metrics:\n");
    printf("Total Deadline Misses: %u\n", totalDeadlineMissesLow);
    printf("Average Response Time: %f ms\n", avgResponseTimeLow);	
}

// Timer callback to notify the task to stop 
static void vTimerCallback(TimerHandle_t pxTimer) {
	// Prevent unused parameter warning
    (void)pxTimer;

	vTaskSuspendAll();

	// Measure and print system metrics
    measureMetrics();

	// Stop the sheduler
	//vTaskEndScheduler();

}

// Function to count the number of items in the linked list of TaskInfo
static uint32_t countItems(TaskInfo *head) {
    uint32_t count = 0;
    TaskInfo *current = head;

    // Traverse the list and count nodes
    while (current != NULL) {
        count++;
        current = (TaskInfo *) current->next;
    }

    return count;
}
