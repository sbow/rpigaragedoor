/**
Garage Door Detector
* V2.0
* Purpose:
 * - Learn GPIO / C / Netowrking on raspberry pi
 * - Notify via email (sms? lights? network notification?)
 *   when the garage door is detected to be open for greater
 *   than some calibratable amount of times
 * 
 * Requires:
 * wiringpi c library for gpio: http://wiringpi.com/download-and-install/
 * 		requires: sudo apt-get install libi2c-dev
 * 
 * Example to compile:
 * gcc -c main.c
 * gcc main.o -lbcm2835 -o output_file_name -lpthread -lwiringPi
 * 
 *  To run at startup:
 * cd /etc
 * sudo pico inittab
 * Find: 1:2345:respawn:/sbin/getty 115200 tty1
 * Replace with: 1:2345:respawn:/bin/login -f USERNAME tty1 </dev/tty1 >/dev/tty1 2>&1
 * (save)
 * cd (ie: go to home)
 * ls -ah (see .bashrc)
 * sudo pico .bashrc
 * 
 * add to the end of the file:
 * if [ $(tty) == /dev/tty1 ]; then
 * 		sudo ./(script name)
 * fi

 * Testing IO at command line:
 * gpio readall 	// this read's all pin states
 * gpio read 5	// read stateof a particular pin
 * gpio write 5 1	// write 1 to pin 5
 *
**/


#include <stdio.h>

#include <wiringPi.h> 	// Good library for GPIO / SPI / I2C access wiringpi.com
#include <stdlib.h> 	// for executing system commands
#include <unistd.h> 	// needed for sleep / usleep; for doing background thred (pid)... may not need
#include <string.h>
#include <pthread.h>	// for multithreading POSIX thread
#include <time.h>		// used for process timing
#include <wiringPi.h>	// used for reading the gpio pins

#include <netdb.h>		// used to test internet connection (also stdio.h, and stdlib.h)
#include <netinet/in.h>	// used to test internet connection
#include <sys/socket.h> // used to test internet connection

#include "garagedoordetector.h" 

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#define THREADID_SENSOR_ONE 0 // monitor magnetic switch position as indicator of garage door state (open/closed)
#define THREADID_SENSOR_NOTIFY 1 // determine if garage door has been open unusual amount of time
#define THREADID_LONGOPEN_SUSPICIOUSTIME 2 // determine if garage door has been open a long time
#define THREADID_TEST_INTERNET 3	// test if internet connection is live, if not, set error light
#define THREADID_LED 4	// run status LED program loop
#define THREADID_SWITCH 5 // run user switch polling loop
#define THREADID_ACOUTLET 6 // run thread controlling the state of the AC Outlet
#define THREADPRIO_SENSOR_ONE NULL // prioritization, 0 max, 99 min, unusued
#define THREADPRIO_SENSOR_NOTIFY NULL // proritization, 0 max, 99 min, unusued
#define THREADPRIO_LONGOPEN_SUSPICIOUSTIME NULL // prioritization, 0 max, 99 min, unused
#define THREADPRIO_TEST_INTERNET NULL // prioritization, 0 max, 99min, unused
#define THREADPRIO_LED NULL // proritization, 0 max, 99min, unused
#define THREADPRIO_SWITCH NULL // prioritization, 0 max, 99min, unused
#define THREADPRIO_ACOUTLET NULL // prioritization, 0 max, 99min, unused


pthread_t threadid[7];

void* launchThreads(void *arg)
{
	
	pthread_t id = pthread_self();
	
	// Thread: Controls the AC Outlet power supply using the state variable b_acoutlet_energized
	if(pthread_equal(id, threadid[ THREADID_ACOUTLET ]))
	{
		while(1)
		{
			usleep(1000);
		}
	}
	
	
	// Thread: Controls the status LED to indicate the program/system state
	if(pthread_equal(id, threadid[ THREADID_LED ] ))
	{
		int led_bit = 0;
		i_light_msg = LIGHT_NOINTERNET; // set default status 
		while(1){
			
			for (i_light_bit = 0; i_light_bit < LIGHT_LENGTH; i_light_bit ++){
				
				led_bit = *(i_light_msg + i_light_bit); // note: the message at pointer i_light_msg is volitile based on if/else statements: ie: change of state machine will change msg instantly
				if (led_bit == 0){
					digitalWrite(  LED_STATUS_PIN, LOW );
					
				}
				else {
					digitalWrite( LED_STATUS_PIN, HIGH );
				}
				
				// the following if else statments define the prioritization of various program states with respect to the status LED
				if ( b_garage_failure == TRUE ){
					i_light_msg = LIGHT_FAILURESUSPECTED; // pointer to message array
				}
				else {
					if ( userswitch_pos == USERSWITCH_POS_ZERO ){
						i_light_msg = LIGHT_NORMAL_STOPPEDPROG;
					}
					else {
						if( b_internet_live == TRUE ){
							i_light_msg = LIGHT_NORMAL_RUNNINGPROG;
							if ( switchPosition == OPEN ){
								i_light_msg = LIGHT_NORMAL_DOOROPEN;
							}
						}
						if( b_internet_live == FALSE ){
							i_light_msg = LIGHT_NOINTERNET;			// pointer to message array
						}
					} // end NOT user switch position zero (ie: run program);
				} // end not garage_failure
				usleep( LIGHT_PERIOD_MILLIS * 1000 );
			} // end for loop counting through LED state bits
			
		}
	}
	
	// Thread: Polls the user switch on the exterior of the case & sets behaviour accordingly
	if( pthread_equal(id, threadid[ THREADID_SWITCH ] ))
	{
		while(1){
			userswitch_pos = digitalRead( USERSWITCH_PIN );
			sleep( USERSWITCH_POLL_SECONDS );
			printf("\nSwitch LogicLevel - note 1 equals graphic 'off': %d\n", userswitch_pos);
		}
	}
			
	
	// Thread: Polls magnetic switch & sets its state for use elsewhere in the program
	if(pthread_equal(id,threadid[THREADID_SENSOR_ONE]))
	{
		time_t time_executed;
		time_t time_now;
		double diff_time;
		int is_first = 0;
		
		int switchRead = 0;
		
		while(1){
			if (is_first == 0){
				time_executed = time(NULL);
				is_first = 1;
				printf("\n Sensor One Running\n");
			}
			else {
				time_now = time(NULL);
				diff_time = difftime( time_now, time_executed);
				if ( diff_time >= POLL_SWITCH_SECONDS ) {
					time_executed = time(NULL);
					printf("\n read switch, position: %d \n", switchPosition);
					switchRead = digitalRead( MAGSWITCH_PIN );
					if ( switchRead == OPEN ) {
						usleep( 250 * 1000 ); // suspend thread for 250 miliseconds (debounce)
						if ( switchRead == OPEN ) {
							if ( switchPosition == CLOSE ){
								switchPosition = OPEN;
							}
						}
					}
					else {
						switchPosition = CLOSE;
					}							
				}
			}
			usleep(1000); // suspend thread execution for a milisecond
		}
	}

	// Thread: Check if internet connection is live, if not working set light
	if(pthread_equal(id, threadid[ THREADID_TEST_INTERNET ] ) )
	{
		time_t time_executed;
		time_t time_now;
		double diff_time;
		time_executed = time(NULL);
		
		struct addrinfo* result;
		int error;
		
		b_internet_live = TRUE;
		
		while(1){
			time_now = time(NULL);
			diff_time = difftime( time_now, time_executed );
			if ( diff_time > POLL_INTERNET_SECONDS ){
				error = getaddrinfo("www.google.com", NULL, NULL, &result);
				if (error != 0) {
					b_internet_live = FALSE;
					//digitalWrite( LED_STATUS_PIN, LOW );
				}
				else {
					b_internet_live = TRUE;
					//digitalWrite( LED_STATUS_PIN, HIGH );
					printf("\n Internet Live\n");
				}		
			}
			usleep(4000*1000); // suspend thread execution for 4 seconds
		}
			
			
	}
	
	// Thread: Based on switchRead state, determines if garage door has been open too long & if so sends an email
	if(pthread_equal(id,threadid[THREADID_SENSOR_NOTIFY]))
	{
		static time_t time_notify_now;
		static double diff_time_notify;
		static int isSent = 0;
		while(1){
			if( userswitch_pos == USERSWITCH_POS_ONE ){
				if( switchPosition == CLOSE || isSent == 1 ) {

					while( switchPosition == CLOSE ) {
						garageOpenLongTime = FALSE; // global state variable used elsewhere
						usleep(1000);
						isSent = 0; // re-set "isSent" boolean to allow a new notification email to be sent
					}
					if( switchPosition == OPEN ){
						time_open = time(NULL);
					}
				}
				else {
					time_notify_now = time(NULL);
					diff_time_notify = difftime( time_notify_now, time_open );
					if ( (diff_time_notify >= NOTIFY_DELAY_SECONDS) && isSent == 0 ) {
						garageOpenLongTime = TRUE; // global state variable used elsewhere
						printf("\n Send Notification Email \n");
						system("echo \"Garage Open!\" | mail -s \"Notice!\" shaunsraspberrypi@gmail.com");
						isSent = 1;
					}
				}
			} // end runmode active check
			usleep(1000);	
		}
	}
	
	// Thread: Check if the door has been open a long time & during suspicious time (ie: late at night)
	if(pthread_equal(id, threadid[ THREADID_LONGOPEN_SUSPICIOUSTIME ] ) )
	{
		static time_t time_current;
		struct tm *time_local;
		
		static time_t time_action_taken;
		static int actionTaken = FALSE;
		
		while(1){
			if( userswitch_pos == USERSWITCH_POS_ONE ){
				if( garageOpenLongTime && !actionTaken ){
					time_current = time(NULL);
					time_local = localtime( &time_current );
					//printf("\n Door open long, current time & date: %s\n", asctime(time_local));
					if( time_local->tm_hour > SUSPICOUSTIME_PM || time_local->tm_hour < SUSPICOUSTIME_AM ){
						printf("\n Garage open at suspicious time! take action & close\n");
						digitalWrite( DOOROPENER_PIN, HIGH );
						sleep(2); // hold down garage door opener button for 2 seconds
						digitalWrite( DOOROPENER_PIN, LOW );
						time_action_taken = time(NULL);
						actionTaken = TRUE;
						system("echo \"Took action to auto-close the door due to it being open for a long time during a suspicious time of day.\" | mail -s \"Notice! - Autoclosed\" shaunsraspberrypi@gmail.com");
						
					}
				}
				if( garageOpenLongTime && actionTaken ){
					// waiting for door to close / see if action worked
					// Monitor time_action_taken & detect if failure likely
					sleep(GARAGE_TIMETOCLOSE_SECONDS); // wait for a time to allow the garage to close
					if( garageOpenLongTime && actionTaken ){
						// Send notice of failure to close door email
						printf("\n Garage still open after taking action, suspect system failure\n");
						system("echo \"Garage may be open, failed to close!\" | mail -s \"Notice! - System failure\" shaunsraspberrypi@gmail.com");
						b_garage_failure = TRUE;
					}
						
				}
				if( !garageOpenLongTime && actionTaken ){
					// assume action succeeded in closing the door, notify?
					actionTaken = FALSE;
					b_garage_failure = FALSE;
				}
			} // end userswitch mode equals run check				
			usleep(1000*1000);
		}
	}
	

	return NULL;
}

// used to detect if any key is pressed, kinda shit
int KeyPressed(int* pKeyCode)
{
	int KeyIsPressed = 0;
	struct timeval tv;
	fd_set rdfs;

	tv.tv_sec = tv.tv_usec = 0;

	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);

	select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);

	if(FD_ISSET(STDIN_FILENO, &rdfs))
	{
		int KeyCode = getchar();
		if(pKeyCode != NULL) 
			*pKeyCode = KeyCode;
		KeyIsPressed = 1;
	}

	return KeyIsPressed;
}

int main(int argc, char **argv)
{
	
	//setup pins
	wiringPiSetup();
	pinMode( MAGSWITCH_PIN, INPUT );
	pinMode( USERSWITCH_PIN, INPUT );	
	pinMode( LED_STATUS_PIN, OUTPUT );
	pinMode( DOOROPENER_PIN, OUTPUT );
	digitalWrite( DOOROPENER_PIN, LOW ); // set garage door opener to low
	digitalWrite( LED_STATUS_PIN, HIGH); // flash LED on power on
	delay(250);
	digitalWrite( LED_STATUS_PIN, LOW);
	int pinVal = 0;
	pinVal = digitalRead( MAGSWITCH_PIN );
	printf("\n Pin Val: %d \n", pinVal);
	
	
	// Create Sensor Thread
	// form: int pthread_create(pthread_t *restrict tidp, const pthread_attr_t *restrict attr, void *(*start_rtn)(void), void *restrict arg)
	int err;
	err = pthread_create( &(threadid[ THREADID_SENSOR_ONE ]), THREADPRIO_SENSOR_ONE, &launchThreads, NULL);

	if (err != 0)
		printf("\nFailed to start thread\n");
	else
		printf("\nCreated Sensor Thread\n");
		
	// Create Notification Thread
	err = pthread_create( &(threadid[ THREADID_SENSOR_NOTIFY ]), THREADPRIO_SENSOR_NOTIFY, &launchThreads, NULL);
	
	if (err != 0)
		printf("\nFailed to start thread\n");
	else
		printf("\nCreated Notification Thread\n");

	// Create Suspicous Time Thread
	err = pthread_create( &(threadid[ THREADID_LONGOPEN_SUSPICIOUSTIME ]), THREADPRIO_LONGOPEN_SUSPICIOUSTIME, &launchThreads, NULL);
	
	if (err != 0)
		printf("\nFailed to start thread\n");
	else
		printf("\nCreated Suspicous Time Thread\n");
		
	// Create Test Internet Connection Thread
	err = pthread_create( &(threadid[ THREADID_TEST_INTERNET ]), THREADPRIO_LONGOPEN_SUSPICIOUSTIME, &launchThreads, NULL);
	
	if (err != 0)
		printf("\nFailed to start Internet Connection thread\n");
	else
		printf("\nCreated Interenet Connectio thread\n");
			
	
	// Create Status LED Thread
	err = pthread_create( &(threadid[ THREADID_LED ]), THREADPRIO_LED, &launchThreads, NULL);
	
	if (err != 0)
		printf("\nFailed to start LED status thread\n");
	else
		printf("\nCreated start LED status thread\n");
	
	
	// Create User Switch Polling Thread
	err = pthread_create( &(threadid[ THREADID_SWITCH ]), THREADPRIO_SWITCH, &launchThreads, NULL);
	
	if (err != 0)
		printf("\nFailed to start Switch Polling thread\n");
	else
		printf("\nCreated Switch Polling Thread\n");

	// Create AC Outlet Control Thread
	err = pthread_create( &(threadid[ THREADID_ACOUTLET ]), THREADPRIO_ACOUTLET, &launchThreads, NULL);
	
	if (err != 0)
		printf("\nFailed to start the AC Outlet Control thread\n");
	else
		printf("\nCreated  the AC Outlet Control thread\n");
		
		
	sleep(15);
	printf("\n still running! \n");	
	while( KeyPressed( NULL ) == 0 ){
		sleep(1);
	}
	
	//Shutdown code
	digitalWrite( LED_STATUS_PIN, LOW);
	return 0;
}

