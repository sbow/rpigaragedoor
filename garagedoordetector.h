/**
 * garagedoordetector.h
 * Header file for garagedoordetector apararatus
 * 2016/01/31
 * Shaun Bowman
 * 
 * v1
 * */
 
 
// AC Outlet - Relay board controls supply lines of an AC outlet.
int b_acoutlet_energize = 1;	// if 1, energize outlet
const int ACOUTLET_HOT_PIN = 6;
const int ACOUTLET_NEUTRAL_PIN = 1;

 // LED Sequences - used to indicate program status - note: MUST be all the same size, otherwise program may crash
// Note: "Allways On" and "Allways Off" are NOT good sequence choices; they indicate a "hung"/crashed program or a lack of power.
const int LIGHT_PERIOD_MILLIS	= 175;	// time in miliseconds, period of each digit of LED
const int LIGHT_N_MSG 		= 5;	// number of light sequences
int LIGHT_LENGTH		= 16; // length of light sequences
int LIGHT_NORMAL_RUNNINGPROG[] 	= {1,1,1,1,	0,0,0,0,	1,1,1,1,	0,0,0,0}; // LED sequence for normal operation
int LIGHT_NORMAL_STOPPEDPROG[] 	= {1,1,1,1,	1,1,1,1,	1,1,1,1,	1,1,1,0}; // LED sequence for normal, program stopped
int LIGHT_NORMAL_DOOROPEN[]	= {0,0,0,0,	0,0,0,0,	1,0,1,0,	1,1,1,1}; // LED sequence for normal, garage open
int LIGHT_NOINTERNET[]		= {1,0,1,0,	0,0,0,0,	0,0,0,0,	0,0,0,0}; // LED sequence for lack of internet
int LIGHT_FAILURESUSPECTED[]	= {1,0,0,0,	0,0,0,0,	0,0,0,0,	0,0,0,0}; // LED sequence for suspected failed hardware		
int *i_light_msg, i_light_bit; // two index's for FOR loops (msg id & bit), 

// MAGNETIC SWITCH SETUP
#define MAGSWITCH_PIN 3	// Paralel w switch and pull down resister. wiringPi pin 3 is labeled pin #22, which is GPIO 3 on the chip.. (confusing)..
#define ALERT_MINUTES 10 // Send notification after X minutes
const double POLL_SWITCH_SECONDS = 2L; // rate at which to poll the magnetic switch
const double NOTIFY_DELAY_SECONDS = 5L; // number of seconds before sending notification email, likely 20*60
const double GARAGE_TIMETOCLOSE_SECONDS = 45L; // number of seconds it should take the garage to close after hitting the button
int b_garage_failure = 0; // boolean status regarding state of control system, zero if all seems well, one otherwise
int b_internet_live = 0; // boolean status of interenet connnection

// STATUS LED SETUP
#define LED_STATUS_PIN 4 // pin connected to LED to indicate internet connection
const double POLL_INTERNET_SECONDS = 3L; // rate at which to test if internet connection is live

// GARAGE DOOR SETUP
#define DOOROPENER_PIN 5 // pin connected to relay/garage door opener circuit
#define OPEN 1
#define CLOSE 0
int garagePosition = CLOSE;
int switchPosition = CLOSE;
int garageOpenLongTime = 0;
static time_t time_open;
const int SUSPICOUSTIME_PM = 5+12; // Past certain time (24 hr format), open garage door is suspicous
const int SUSPICOUSTIME_AM = 6;		// Before certain time (24 hr format), open door is suspicous

// USER SWITCH SETUP
#define USERSWITCH_PIN 0 // pin connected to user switch on exterior of electronics case
#define USERSWITCH_POS_ZERO 1 	// define switch position graphic zero equal voltage read level of 1
#define USERSWITCH_POS_ONE 0	// define switch position graphic one equal voltage read level of 1 
#define USERSWITCH_POLL_SECONDS 1 // define poll rate for user switch
int userswitch_pos = USERSWITCH_POS_ZERO; // define user switch position; default to off

// OTHER
#define FALSE 0
#define TRUE 1


