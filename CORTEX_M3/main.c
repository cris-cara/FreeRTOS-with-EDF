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
#include <queue.h>
#include <semphr.h>
#include "types.h"
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


/*
 * Printf() output is sent to the serial port.  Initialise the serial hardware.
 */
static void prvUARTInit( void );

/* Function definitions */

static int16_t random(int16_t rand_min, int16_t rand_max);
static void vTemperatureSensor();
static void vAirHumiditySensor();
static void vSoilHumiditySensor();
static void vMotionSensor();
static void vLightSensor();
static void vControlCenter();

static void vCriticalTask();
static void sendCriticalValue();

//static uint32_t  ulInterruptServiceRoutine();
//void vPortGenerateSimulatedInterrupt();


/* Global variables */
const int QUEUE_LENGTH = 5;	// size = number of sensor
QueueHandle_t xQueue;

const int QUEUE_LENGTH_CT = 1;	// size = we need just 1 structure 
QueueHandle_t xQueueForCT;

SemaphoreHandle_t xSemaphoreCT;
SemaphoreHandle_t xSemaphoreReturnControl;




/*-----------------------------------------------------------*/
void main( void )
{
	/* See https://www.freertos.org/freertos-on-qemu-mps2-an385-model.html for
	instructions. */


	/* Hardware initialisation.  printf() output uses the UART for IO. */
	prvUARTInit();

	// Create the queue
	xQueue = xQueueCreate( QUEUE_LENGTH, sizeof( Data_t) );
	xQueueForCT = xQueueCreate( QUEUE_LENGTH_CT, sizeof( Data_t) );
	// Crete the semaphores 
	xSemaphoreCT = xSemaphoreCreateBinary();
	xSemaphoreReturnControl = xSemaphoreCreateBinary();

	// Check for failures
	if ( xQueue != NULL && xQueueForCT != NULL && xSemaphoreCT != NULL && xSemaphoreReturnControl != NULL){
		// Creating the tasks
		xTaskCreate( vTemperatureSensor, "temperature sensor", 1000, NULL, 1, NULL);
		xTaskCreate( vAirHumiditySensor, "air humidity sensor", 1000, NULL, 1, NULL);
		xTaskCreate( vSoilHumiditySensor, "soil humidity sensor", 1000, NULL, 1, NULL);
		xTaskCreate( vMotionSensor, "motion sensor", 1000, NULL, 1, NULL);
		xTaskCreate( vLightSensor, "light sensor", 1000, NULL, 1, NULL);
		
		// ControlCenter task has superior priority
		xTaskCreate( vControlCenter, "control center", 1000, NULL, 2, NULL);

		//Let's create the task for the critical values of the sensors 
		xTaskCreate( vCriticalTask, "critical task", 1000, NULL, 3, NULL);
		
		// Starting the scheduler
		vTaskStartScheduler();
	}
	else {
		// Queues or semaphores could not be created (not enough memory) 
		printf("There is a problem with the queques or with the semaphores");
		vTaskEndScheduler();  // kill the OS
	}

	for(;;);
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
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
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

static int16_t random(int16_t rand_min, int16_t rand_max) {
	static uint16_t lfsr = 0xACE1u; 
    lfsr ^= lfsr >> 7;
    lfsr ^= lfsr << 9;
    lfsr ^= lfsr >> 13;


    // Calcolare il valore casuale compreso tra rand_min e rand_max - 1
    uint16_t range = (rand_max > rand_min) ? (rand_max - rand_min) : 1;
    return rand_min + (lfsr % range);  
}

/*-----------------------------------------------------------*/


/*Here we have the implementation of the sets of task corresponding to the different sensors. 
  In each function we generate a random number and we send it to a queque that will be read by the control task*/
static void vTemperatureSensor() 
{
    for( ;; ) 
	{
        int16_t sensor_value = random(10,40);
        Data_t xSensor = {temperature, sensor_value};
        printf("\r\nsensor: %s value: %d°C\r\n", "temperature", xSensor.value);

		BaseType_t xStatus = xQueueSendToBack( xQueue, &xSensor, 0);
		if ( xStatus != pdPASS )
		{
			printf( "Could not send to the queue.\r\n");
		}

        vTaskDelay(pdMS_TO_TICKS( 1000 ));
    } 
}

/*-----------------------------------------------------------*/

static void vAirHumiditySensor() {
    for( ;; ) {
        int16_t sensor_value = random(50,95);
        Data_t xSensor = {airHumidity, sensor_value};
        printf("\r\nsensor: %s value: %d%%\r\n", "airHumidity", xSensor.value);

		BaseType_t xStatus = xQueueSendToBack( xQueue, &xSensor, 0);
		if ( xStatus != pdPASS )
		{
			printf( "Could not send to the queue.\r\n");
		}

        vTaskDelay(pdMS_TO_TICKS( 1000 ));
    } 
}


/*-----------------------------------------------------------*/

static void vSoilHumiditySensor() {
    for( ;; ) {
        int16_t sensor_value = random(50,90);
        Data_t xSensor = {soilHumidity, sensor_value};
        printf("\r\nsensor: %s value: %d%%\r\n", "soilHumidity", xSensor.value);

		BaseType_t xStatus = xQueueSendToBack( xQueue, &xSensor, 0);
		if ( xStatus != pdPASS )
		{
			printf( "Could not send to the queue.\r\n");
		}

        vTaskDelay(pdMS_TO_TICKS( 1000 ));
    } 
}

/*-----------------------------------------------------------*/

static void vMotionSensor() {
    for( ;; ) {
        int16_t sensor_value = random(0,100);
        Data_t xSensor = {motion, sensor_value};
        printf("\r\nsensor: %s value: %d \r\n", "motion", xSensor.value);

		BaseType_t xStatus = xQueueSendToBack( xQueue, &xSensor, 0);
		if ( xStatus != pdPASS )
		{
			printf( "Could not send to the queue.\r\n");
		}

        vTaskDelay(pdMS_TO_TICKS( 1000 ));
    } 
}

/*-----------------------------------------------------------*/

static void vLightSensor() {
    for( ;; ) {
        int16_t sensor_value = random(9000,11000);
        Data_t xSensor = {light, sensor_value};
        printf("\r\nsensor: %s value: %d lux\r\n", "light", xSensor.value);

		BaseType_t xStatus = xQueueSendToBack( xQueue, &xSensor, 0);
		if ( xStatus != pdPASS )
		{
			printf( "Could not send to the queue.\r\n");
		}

        vTaskDelay(pdMS_TO_TICKS( 1000 ));
    } 
}

/*-----------------------------------------------------------*/

/* This task is a control task. It receives the data of the sensors from xQueque, and checks is the values are in a good range. If thwy're not, a critical task will be called
   That's only a simulation, so we've taken thresholds that can have sense, but without a real study behind. 
				THRESHOLDS: 
		AIR HUMIDITY: 65% - 85%
		TEMPERATURE: 15°C - 25°C
		SOIL HUMIDITY: 70% - 80%
		MOTION: da 90-99 (there's movement)
		LIGHTSENSOR: 9500 LUX - 10500 LUX 				*/
static void vControlCenter() {
	Data_t receivedData;
	eSensorID sensor;
    int16_t value;

	for (;;) {
		printf("\n\n### CONTROL CENTER: ANALYSING ALL SENSOR'S VALUE ###\n");

		// loop for reading the values from the queque, until we have values.
		while (uxQueueMessagesWaiting(xQueue) > 0) {
			// Take Data_t data from the queque 
            if (xQueueReceive(xQueue, &receivedData, 0) == pdPASS) {
				sensor = receivedData.eDataSource;
				value = receivedData.value;
				
        		printf("\r\nAnalysing sensor: %s...\r\n", sensors_name[sensor]);
				// depending on the sensor and tha value, we perform some thresholds checks
				switch (sensor)
				{
				case temperature:
					if (value < 15) {
						/*so now we are in a critical situation. The temperature is < 15 degrees.
						  So in this case we want to handle this situation with the critical task, created for this purpose.
						  We synchronise the tasks using semaphores. In few words: 
						        	-  we give a xSemaphoreCT to activate the execution of the CT. Indeed in the critical task we have a take function for this semaphore.
									   Now, the critical task has an higher priority, but we cannot be sure that after the giving of the semaphore, the CT suddenly stats execting.
									   For this reason we use another semaphore...
									-  we take xSemaphoreReturnControl, a semaphore that stops the execution of this task until someone gives the semaphore. This someone is the
									   critical task, that after completing its work, gives this semaphore to unlock the control task.
									   )*/
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nTemperature too low!\n");

						/*we obv need to send the critical data to the crical task*/
						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);

						printf("\n***CRITICAL VALUE HANDLED***\n\n");

					}
					else if (value > 25) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nTemperature too high!\n");	
						
						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);	
						printf("\n***CRITICAL VALUE HANDLED***\n\n");

					}
					else
						printf("OK! No conditioning needed!\n");
					
					break;

				case airHumidity:
					if (value < 65) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nHumidity in the air too low!\n");
					
						sendCriticalValue(receivedData, xQueueForCT); 

					    xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);	
						printf("\n***CRITICAL VALUE HANDLED***\n\n\n");
					}
					else if (value > 85) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nHumidity in the air too high!\n");						

						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);	
    					printf("\n***CRITICAL VALUE HANDLED***\n\n");
					}
					else
						printf("OK! No intervention is necessary!\n");
					
					break;

				case soilHumidity:
					if (value < 70) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nHumidity in the soil too low!\n");
						
						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);	
						printf("\n***CRITICAL VALUE HANDLED***\n\n\n");
					}
					else if (value > 80) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nHumidity in the soil too high!\n");	
							
						sendCriticalValue(receivedData, xQueueForCT); 
					
						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);				
						printf("\n***CRITICAL VALUE HANDLED***\n\n");
					}
					else
						printf("OK! No regolation of sprinklers needed!\n");
					
					break;

				case motion:
				
					if (value > 90) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nALARM! INTRUDER!!!\n");
						
						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);			
						printf("\n***CRITICAL VALUE HANDLED***\n\n");
					}
					else
						printf("OK! No intrusion detected!\n");
					
					break;

				case light:
					if (value < 9500) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nLights too low!\n");
						
						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);	
						printf("\n***CRITICAL VALUE HANDLED***\n\n");
					}
					else if (value > 10500) {
						printf("\n\n***CRITICAL VALUE***\n");
						printf("%s", "\nLights too high!\n");		
					
						sendCriticalValue(receivedData, xQueueForCT); 

						xSemaphoreGive(xSemaphoreCT);
						xSemaphoreTake(xSemaphoreReturnControl, portMAX_DELAY);					
						printf("\n***CRITICAL VALUE HANDLED***\n\n");
					}
					else
						printf("OK! No regolation of lights needed!\n");
					
					break;

				default:
					break;
				}

			}

		}
		
		printf("\n\n%s\n\n", "-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-");
		vTaskDelay(pdMS_TO_TICKS(1000));  // Aggiungi un ritardo per evitare un loop troppo veloce
	}
}






/*This the task that will be activeted in the critical situations.
When the control task will activate an interrupt, the ISR will give a 
semaphore that will be taken by this task.
So if there is even 1 sensor that produces critical values, this task will start.
Obv this task will have the highest priority, no one other tasks can be executed until this task will finish*/

static void vCriticalTask(){

	for(;;){
		xSemaphoreTake( xSemaphoreCT, portMAX_DELAY);

		Data_t receivedDataCT;
		eSensorID sensorCT;
 	  	int16_t valueCT;

		// We receive the criticalData from the queque
        if (xQueueReceive(xQueueForCT, &receivedDataCT, 0) == pdPASS) {
			sensorCT = receivedDataCT.eDataSource;
			valueCT = receivedDataCT.value;
		
			/*So now the vCriticalTask is executed, it receives the sensor that gives the problem and the 
								critical value found */
			/*Now, according with the type of problem, we solve it in some way and wee give the relative semaphore back */
			switch (sensorCT) {
				case temperature:
					printf("We'll try to fix the temperature, that's: %d°C\n", valueCT);
					printf("Changing temperature of the air conditioner...\n");
					printf("Temperature comes back to normal values!\n");

					/*Before to give back the semaphore of the control task, there is the possibility that that semaphore 
					has never been turned on. So let's get the current count of the semaphore */

					xSemaphoreGive(xSemaphoreReturnControl);

					break;

				case airHumidity:
					printf("We'll try to fix the air humidity, that's: %d%%\n", valueCT);
					printf("Advising plumbing technicians of the problem...\n");
					printf("Air humidity comes back to normal values!\n");

					xSemaphoreGive(xSemaphoreReturnControl);

					break;

				case soilHumidity: 
					printf("We'll try to fix the soil humidity, that's: %d%%\n", valueCT);
					printf("Regolation of all sprinklers...\n");  //"irrigatori" in italian
					printf("Soil humidity comes back to normal values!\n");

					xSemaphoreGive(xSemaphoreReturnControl);

					break;	

				case motion: 
					printf("SOMEONE DETECTED IN THE FIELD, with a motion value: %d\n", valueCT);
					printf("\n---INTRUSION PROTOCOL ACTIVATED---\n");
					printf("All doors looked up...\n");
					printf("Calling Security...\n");
					printf("Security will solve the problem! Don't worry!\n");

					xSemaphoreGive(xSemaphoreReturnControl);

					break;

				case light: 
					printf("Uncorrect light detected: %d\n", valueCT);
					printf("Changing the lights...\n");  
					printf("Lights restored!\n");
	
					xSemaphoreGive(xSemaphoreReturnControl);

					break;	

				default:
					break;
			}

		}
	}

	
}



/*function that sends the criticalData to the criticalTask using the a queque*/
static void sendCriticalValue(Data_t crticalData, QueueHandle_t queque){
			BaseType_t xStatusCT = xQueueSendToBack( queque, &crticalData, 0);
			if ( xStatusCT != pdPASS ){
							printf( "Could not send to the queue.\r\n");
			}
			return;
}