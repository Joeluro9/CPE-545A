// Header Files
#include "TypeDefines.h"
#include "TimerMgrHeader.h"
#include "TimerAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

/*****************************************************
 * Global Variables
 *****************************************************
 */
// Timer Pool Global Variables
INT8U FreeTmrCount = 0;
RTOS_TMR *FreeTmrListPtr = NULL;

// Tick Counter
INT32U RTOSTmrTickCtr = 0;

// Hash Table
HASH_OBJ hash_table[HASH_TABLE_SIZE];

// Thread variable for Timer Task
pthread_t thread;

// Semaphore for Signaling the Timer Task
sem_t timer_task_sem;

// Mutex for Protecting Hash Table
pthread_mutex_t hash_table_mutex;

// Mutex for Protecting Timer Pool
pthread_mutex_t timer_pool_mutex;

/*****************************************************
 * Timer API Functions
 *****************************************************
 */

// Function to create a Timer
RTOS_TMR* RTOSTmrCreate(INT32U delay, INT32U period, INT8U option, RTOS_TMR_CALLBACK callback, void *callback_arg, INT8	*name, INT8U *err)
{
	RTOS_TMR *timer_obj = NULL;

	// Check the input Arguments for ERROR
	if(*err != RTOS_ERR_NONE)
		return NULL;
	/*
	if(delay >= 10000000){
		*err = RTOS_ERR_TMR_INVALID_DLY;
		return NULL;
	}
	if(period >= 10000000){
		*err = RTOS_ERR_TMR_INVALID_PERIOD;
		return NULL;
	}
	*/
	if(option == NULL){
		*err = RTOS_ERR_TMR_INVALID_OPT;
		return NULL;
	}
	// Allocate a New Timer Obj
	timer_obj = alloc_timer_obj();

	if(timer_obj == NULL) {
		// Timers are not available
		*err = RTOS_ERR_TMR_NON_AVAIL;
		return NULL;
	}

	// Fill up the Timer Object
	timer_obj->RTOSTmrCallback = callback;
	timer_obj->RTOSTmrCallbackArg = callback_arg;
	timer_obj->RTOSTmrNext = NULL;
	timer_obj->RTOSTmrPrev = NULL;
	timer_obj->RTOSTmrMatch = 0;
	timer_obj->RTOSTmrDelay = delay;
	timer_obj->RTOSTmrPeriod = period;
	timer_obj->RTOSTmrName = name;
	timer_obj->RTOSTmrOpt = option;
	timer_obj->RTOSTmrState = RTOS_TMR_STATE_STOPPED;

	*err = RTOS_SUCCESS;

	return timer_obj;
}

// Function to Delete a Timer
INT8U RTOSTmrDel(RTOS_TMR *ptmr, INT8U *perr)
{
	// ERROR Checking
	if(*perr != RTOS_ERR_NONE)
		return RTOS_FALSE;
	if(ptmr == NULL){
		*perr = RTOS_ERR_TMR_INVALID;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
		*perr = RTOS_ERR_TMR_INVALID_TYPE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INACTIVE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_COMPLETED){
		*perr = RTOS_ERR_TMR_INVALID_STATE;
		return RTOS_FALSE;
	}
	// Free Timer Object according to its State
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_RUNNING)
		remove_hash_entry(ptmr);
	free_timer_obj(ptmr);

	*perr = RTOS_SUCCESS;
	return RTOS_TRUE;
}

// Function to get the Name of a Timer
INT8* RTOSTmrNameGet(RTOS_TMR *ptmr, INT8U *perr)
{
	// ERROR Checking
	if(*perr != RTOS_ERR_NONE)
		return NULL;
	if(ptmr == NULL){
		*perr = RTOS_ERR_TMR_INVALID;
		return NULL;
	}
	if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
		*perr = RTOS_ERR_TMR_INVALID_TYPE;
		return NULL;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INACTIVE;
		return NULL;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INVALID_STATE;
		return NULL;
	}
	// Return the Pointer to the String
	return ptmr->RTOSTmrName;
}

// To Get the Number of ticks remaining in time out
INT32U RTOSTmrRemainGet(RTOS_TMR *ptmr, INT8U *perr)
{
	// ERROR Checking
	if(*perr != RTOS_ERR_NONE)
		return NULL;
	if(ptmr == NULL){
		*perr = RTOS_ERR_TMR_INVALID;
		return NULL;
	}
	if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
		*perr = RTOS_ERR_TMR_INVALID_TYPE;
		return NULL;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INACTIVE;
		return NULL;
	}
	if(ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING){
		*perr = RTOS_ERR_TMR_INVALID_STATE;
		return NULL;
	}
	// Return the remaining ticks
	return ptmr->RTOSTmrMatch - RTOSTmrTickCtr;
}

// To Get the state of the Timer
INT8U RTOSTmrStateGet(RTOS_TMR *ptmr, INT8U *perr)
{
	// ERROR Checking
	if(*perr != RTOS_ERR_NONE)
		return NULL;
	if(ptmr == NULL){
		*perr = RTOS_ERR_TMR_INVALID;
		return NULL;
	}
	if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
		*perr = RTOS_ERR_TMR_INVALID_TYPE;
		return NULL;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INACTIVE;
		return NULL;
	}
	if(ptmr->RTOSTmrState == NULL){
		*perr = RTOS_ERR_TMR_INVALID_STATE;
		return NULL;
	}
	// Return State
	return ptmr->RTOSTmrState;
}

// Function to start a Timer
INT8U RTOSTmrStart(RTOS_TMR *ptmr, INT8U *perr)
{
	// ERROR Checking
	if(*perr != RTOS_ERR_NONE)
		return RTOS_FALSE;
	if(ptmr == NULL){
		*perr = RTOS_ERR_TMR_INVALID;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
		*perr = RTOS_ERR_TMR_INVALID_TYPE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INACTIVE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_RUNNING){
		*perr = RTOS_ERR_TMR_INVALID_STATE;
		return RTOS_FALSE;
	}
	// Based on the Timer State, update the RTOSTmrMatch using RTOSTmrTickCtr, RTOSTmrDelay and RTOSTmrPeriod
	ptmr->RTOSTmrMatch = RTOSTmrTickCtr + ptmr->RTOSTmrDelay;
	ptmr->RTOSTmrState = RTOS_TMR_STATE_RUNNING;
	// You may use the Hash Table to Insert the Running Timer Obj
	insert_hash_entry(ptmr);

	return RTOS_TRUE;
}

// Function to Stop the Timer
INT8U RTOSTmrStop(RTOS_TMR *ptmr, INT8U opt, void *callback_arg, INT8U *perr)
{
	// ERROR Checking
	if(*perr != RTOS_ERR_NONE)
		return RTOS_FALSE;
	if(ptmr == NULL){
		*perr = RTOS_ERR_TMR_INVALID;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
		*perr = RTOS_ERR_TMR_INVALID_TYPE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
		*perr = RTOS_ERR_TMR_INACTIVE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrOpt == NULL){
		*perr = RTOS_ERR_TMR_INVALID_OPT;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState == RTOS_TMR_STATE_STOPPED){
		*perr = RTOS_ERR_TMR_STOPPED;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING){
		*perr = RTOS_ERR_TMR_INVALID_STATE;
		return RTOS_FALSE;
	}
	if(ptmr->RTOSTmrCallback == NULL){
		*perr = RTOS_ERR_TMR_NO_CALLBACK;
		return RTOS_FALSE;
	}
	// Remove the Timer from the Hash Table List
	remove_hash_entry(ptmr);
	// Change the State to Stopped
	ptmr->RTOSTmrState = RTOS_TMR_STATE_STOPPED;
	// Call the Callback function if required
	switch(opt)
	{
	case RTOS_TMR_OPT_NONE:
		break;
	case RTOS_TMR_OPT_CALLBACK:
		ptmr->RTOSTmrCallback(ptmr->RTOSTmrCallbackArg);
		break;
	case RTOS_TMR_OPT_CALLBACK_ARG:
		ptmr->RTOSTmrCallback(callback_arg);
		break;
	default:
		break;
	}
	return RTOS_TRUE;
}

// Function called when OS Tick Interrupt Occurs which will signal the RTOSTmrTask() to update the Timers
void RTOSTmrSignal(int signum)
{
	// Received the OS Tick
	// Send the Signal to Timer Task using the Semaphore
	sem_post(&timer_task_sem);
}

/*****************************************************
 * Internal Functions
 *****************************************************
 */

// Create Pool of Timers
INT8U Create_Timer_Pool(INT32U timer_count)
{
	// Create the Timer pool using Dynamic Memory Allocation
	// You can imagine of LinkedList Creation for Timer Obj
	if(timer_count == 0)
		return RTOS_MALLOC_ERR;
	FreeTmrListPtr = (RTOS_TMR*)malloc(sizeof(RTOS_TMR));
	FreeTmrListPtr->RTOSTmrPrev = NULL;
	FreeTmrListPtr->RTOSTmrType = RTOS_TMR_TYPE;
	FreeTmrListPtr->RTOSTmrState = RTOS_TMR_STATE_UNUSED;
	RTOS_TMR *tempTmr = FreeTmrListPtr;
	for(int i = 1 ; i < timer_count ; i++)
	{
		tempTmr->RTOSTmrNext = (RTOS_TMR*)malloc(sizeof(RTOS_TMR));
		tempTmr->RTOSTmrNext->RTOSTmrPrev = tempTmr;
		tempTmr = tempTmr->RTOSTmrNext;
		tempTmr->RTOSTmrType = RTOS_TMR_TYPE;
		tempTmr->RTOSTmrState = RTOS_TMR_STATE_UNUSED;
	}
	tempTmr->RTOSTmrNext = NULL;
	FreeTmrCount = timer_count;

	return RTOS_SUCCESS;
}

// Initialize the Hash Table
void init_hash_table(void)
{
	for(int i = 0 ; i < HASH_TABLE_SIZE ; i++)
	{
		hash_table[i].timer_count = 0;
		hash_table[i].list_ptr = NULL;
	}

}

// Insert a Timer Object in the Hash Table
void insert_hash_entry(RTOS_TMR *timer_obj)
{
	// Calculate the index using Hash Function
	int index = timer_obj->RTOSTmrMatch % 10;
	// Lock the Resources
	pthread_mutex_lock(&hash_table_mutex);
	// Add the Entry
	timer_obj->RTOSTmrNext = hash_table[index].list_ptr;
	if(hash_table[index].timer_count > 0)
		hash_table[index].list_ptr->RTOSTmrPrev = timer_obj;
	hash_table[index].list_ptr = timer_obj;

	hash_table[index].timer_count++;
	// Unlock the Resources
	pthread_mutex_unlock(&hash_table_mutex);
}

// Remove the Timer Object entry from the Hash Table
void remove_hash_entry(RTOS_TMR *timer_obj)
{
	// Calculate the index using Hash Function
	int index = timer_obj->RTOSTmrMatch % 10;
	// Lock the Resources
	pthread_mutex_lock(&hash_table_mutex);
	// Remove the Timer Obj
	RTOS_TMR *tempTmr = hash_table[index].list_ptr;
	while(tempTmr != timer_obj && tempTmr != NULL)
		tempTmr = tempTmr->RTOSTmrNext;
	if(tempTmr == NULL)
	{
		pthread_mutex_unlock(&hash_table_mutex);
		return;
	}
	if(!tempTmr->RTOSTmrPrev)
		hash_table[index].list_ptr = tempTmr->RTOSTmrNext;
	else
		tempTmr->RTOSTmrPrev->RTOSTmrNext = tempTmr->RTOSTmrNext;
	if(tempTmr->RTOSTmrNext)
		tempTmr->RTOSTmrNext->RTOSTmrPrev = tempTmr->RTOSTmrPrev;
	timer_obj->RTOSTmrNext = timer_obj->RTOSTmrPrev = NULL;

	hash_table[index].timer_count--;
	// Unlock the Resources
	pthread_mutex_unlock(&hash_table_mutex);
}

// Timer Task to Manage the Running Timers
void *RTOSTmrTask(void *temp)
{

	while(1) {
		// Wait for the signal from RTOSTmrSignal()
		sem_wait(&timer_task_sem);
		// Once got the signal, Increment the Timer Tick Counter
		// Check the whole List associated with the index of the Hash Table
		int index = RTOSTmrTickCtr % 10;
		// Compare each obj of linked list for Timer Completion
		RTOS_TMR *tempTmr = hash_table[index].list_ptr;
		for(;tempTmr != NULL;)
		{
			RTOS_TMR *q = tempTmr;
			tempTmr = tempTmr->RTOSTmrNext;
			if(q->RTOSTmrMatch == RTOSTmrTickCtr)
			{
				q->RTOSTmrCallback(q->RTOSTmrCallbackArg);
				q->RTOSTmrState = RTOS_TMR_STATE_COMPLETED;
				remove_hash_entry(q);
				if(q->RTOSTmrOpt == RTOS_TMR_ONE_SHOT)
				{
					free_timer_obj(q);
				}
				else
				{
					q->RTOSTmrMatch = RTOSTmrTickCtr + q->RTOSTmrPeriod;
					q->RTOSTmrState = RTOS_TMR_STATE_RUNNING;
					insert_hash_entry(q);
				}
			}
		}
		RTOSTmrTickCtr++;
		// If the Timer is completed, Call its Callback Function, Remove the entry from Hash table
		// If the Timer is Periodic then again insert it in the hash table
		// Change the State
	}
	return temp;
}

// Timer Initialization Function
void RTOSTmrInit(void)
{
	INT32U timer_count = 0;
	INT8U	retVal;
	pthread_attr_t attr;

	fprintf(stdout,"\n\nPlease Enter the number of Timers required in the Pool for the OS ");
	scanf("%d", &timer_count);

	// Create Timer Pool
	retVal = Create_Timer_Pool(timer_count);

	// Check the return Value
	if (retVal != RTOS_SUCCESS)
	{
		fprintf(stdout, "\nTimer malloc failed\n");
		return;
	}
	// Init Hash Table
	init_hash_table();

	fprintf(stdout, "\n\nHash Table Initialized Successfully\n");

	// Initialize Semaphore for Timer Task
	sem_init(&timer_task_sem, 0, 0);
	// Initialize Mutex if any
	//hash_table_mutex = PTHREAD_MUTEX_INITIALIZER;
	//timer_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
	// Create any Thread if required for Timer Task
	pthread_create(&thread, NULL, RTOSTmrTask, NULL);

	fprintf(stdout,"\nRTOS Initialization Done...\n");
}

// Allocate a timer object from free timer pool
RTOS_TMR* alloc_timer_obj(void)
{
	RTOS_TMR *tempTmr = NULL;
	// Lock the Resources
	pthread_mutex_lock(&timer_pool_mutex);
	// Check for Availability of Timers
	// Assign the Timer Object
	if(FreeTmrCount != 0)
	{
		tempTmr = FreeTmrListPtr;
		FreeTmrListPtr = FreeTmrListPtr->RTOSTmrNext;
		tempTmr->RTOSTmrNext = NULL;
		if(FreeTmrCount != 1)
			FreeTmrListPtr->RTOSTmrPrev = NULL;
		FreeTmrCount--;
	}
	// Unlock the Resources
	pthread_mutex_unlock(&timer_pool_mutex);

	return tempTmr;
}

// Free the allocated timer object and put it back into free pool
void free_timer_obj(RTOS_TMR *ptmr)
{
	// Lock the Resources
	pthread_mutex_lock(&timer_pool_mutex);
	// Clear the Timer Fields
	ptmr->RTOSTmrCallback = NULL;
	ptmr->RTOSTmrCallbackArg = NULL;
	ptmr->RTOSTmrPrev = NULL;
	ptmr->RTOSTmrMatch = NULL;
	ptmr->RTOSTmrDelay = NULL;
	ptmr->RTOSTmrPeriod = NULL;
	ptmr->RTOSTmrName = NULL;
	ptmr->RTOSTmrOpt = NULL;
	ptmr->RTOSTmrState = RTOS_TMR_STATE_UNUSED;
	ptmr->RTOSTmrNext = FreeTmrListPtr;
	// Change the State
	// Return the Timer to Free Timer Pool
	FreeTmrListPtr = ptmr;
	FreeTmrCount++;
	// Unlock the Resources
	pthread_mutex_unlock(&timer_pool_mutex);
}

// Function to Setup the Timer of Linux which will provide the Clock Tick Interrupt to the Timer Manager Module
void OSTickInitialize(void) {
	timer_t timer_id;
	struct itimerspec time_value;

	// Setup the time of the OS Tick as 100 ms after 3 sec of Initial Delay
	time_value.it_interval.tv_sec = 0;
	time_value.it_interval.tv_nsec = RTOS_CFG_TMR_TASK_RATE;

	time_value.it_value.tv_sec = 0;
	time_value.it_value.tv_nsec = RTOS_CFG_TMR_TASK_RATE;

	// Change the Action of SIGALRM to call a function RTOSTmrSignal()
	signal(SIGALRM, &RTOSTmrSignal);

	// Create the Timer Object
	timer_create(CLOCK_REALTIME, NULL, &timer_id);

	// Start the Timer
	timer_settime(timer_id, 0, &time_value, NULL);
}
