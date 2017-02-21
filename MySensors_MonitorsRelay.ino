/*
	MySensors MonitorsRelay
	https://github.com/soif/MySensors_MonitorsRelay
	Copyright 2014 François Déchery

	** Description **
	This Arduino ProMini (5V) based project is a MySensors  node which controls a relay, with a toggle button, 2 Status Leds 
	and send the room temperature using a built-in Sensor.

	** Compilation **
		- needs MySensors version 2.0+
*/

// debug #################################################################################
#define MY_DEBUG	// Comment/uncomment to remove/show debug (May overflow Arduino memory when set)

// Define ################################################################################
#define INFO_NAME "MonitorsRelay"
#define INFO_VERS "2.1.00"

// MySensors
#define MY_RADIO_NRF24		// radio module used
#define MY_NODE_ID 101		// Node ID

//https://forum.mysensors.org/topic/5778/mys-library-startup-control-after-onregistration/7
#define MY_TRANSPORT_WAIT_READY_MS (5*1000ul)					// how long to wait for transport ready at boot
#define MY_TRANSPORT_SANITY_CHECK								// check if transport is available
#define MY_TRANSPORT_SANITY_CHECK_INTERVAL_MS (15*60*1000ul)	// how often to  check if transport is available (already set as default)
#define MY_TRANSPORT_TIMEOUT_EXT_FAILURE_STATE (5*60*1000ul)	//  how often to reconnect if no transport

#define MY_REPEATER_FEATURE		// set as Repeater

#define PIN_RELAY			2
#define PIN_ONEWIRE			3
#define PIN_BUTTON			4
#define PIN_LED_RED			5
#define PIN_LED_GREEN		6
#define PIN_LED_BOOT		5		// or 13 for internal led

#define CHILD_ID_RELAY		0
#define CHILD_ID_TEMP		1

#define REPORT_TEMP_SEC		60		// report temperature every x seconds
#define DEBOUNCE_TIME		100		// Button Debounce time (ms)
#define RELAY_ON			HIGH	// my Relay use LOW for ON, but I use 'NC' instead of 'NO' pins
#define RELAY_OFF			LOW		// my Relay use HIGH for OFF, but I use 'NC' instead of 'NO' pins


// includes ##############################################################################
#include <SPI.h>
#include <MySensors.h>

#include "debug.h"
#include <Bounce2.h>
#include <DallasTemperature.h>
#include <OneWire.h>


// Variables #############################################################################
boolean	metric				= true;	// use Celcius
boolean	is_on				= true; // current relay state
float	lastDallasTemp		= -100;	// latest Temperature
unsigned long next_report 	= 3000; // last temperature report time : start 3s after boot
const float temp_offset		= -1.0;	// calibrate DS18B20 deviation
boolean init_msg_sent		= false; // inited ?

// objects ###############################################################################
OneWire				oneWire(PIN_ONEWIRE);
DallasTemperature	dallas(&oneWire);
Bounce 				debouncer = Bounce(); 

MyMessage 			msgRelay(	CHILD_ID_RELAY,	V_LIGHT);
MyMessage 			msgTemp(	CHILD_ID_TEMP,	V_TEMP);



// ###### Setup ##########################################################################
void setup()  {
}

// ###### Loop ##########################################################################
void loop() {
	InitialState();

	if(init_msg_sent){
		//relay & button --------------------
		boolean changed = debouncer.update();
		int button_state = debouncer.read();
		if (changed && button_state == 0) {
			DEBUG_PRINTLN("BUTTON Toggle !!");
			Toggle();
			send(msgRelay.set(is_on), true); // Send new state and request ack back
			DEBUG_PRINT("<-- Sending Relay State : "); 
			DEBUG_PRINTLN(is_on); 
		}

		// Dallas 1820 Sensor ---------------
		ProcessTemperature();
	}
} 





// #######################################################################################
// ############################# FUNCTIONS ###############################################
// #######################################################################################


// --------------------------------------------------------------------
void presentation(){
	DEBUG_PRINTLN("---- presentation START -------");
	sendSketchInfo(INFO_NAME , INFO_VERS );

	present(CHILD_ID_RELAY, 	S_LIGHT);  
	present(CHILD_ID_TEMP,	S_TEMP); 
//	metric = getConfig().isMetric;

	DEBUG_PRINTLN("---- presentation END   -------");
}

// --------------------------------------------------------------------
void receive(const MyMessage &message){
	if (message.isAck() ) {
		DEBUG_PRINTLN("--> ACK from gateway IGNORED !");
		return;
	}

	if (message.type == V_LIGHT) {
		DEBUG_PRINT("--> Receiving change for sensor: ");
		DEBUG_PRINT(message.sensor);
		DEBUG_PRINT(", New state: ");
		DEBUG_PRINTLN(message.getBool());
		// Change relay state
		Switch(message.getBool());
		LedAnim(PIN_LED_RED);
   }
}

// --------------------------------------------------------------------
void before(){
	Serial.begin(115000);
	DEBUG_PRINTLN("++++++++++++++");
	DEBUG_PRINTLN(" Booting...");
	DEBUG_PRINTLN("++++++++++++++");

	pinMode(PIN_LED_BOOT,	OUTPUT);
	digitalWrite(PIN_LED_BOOT,	LOW);

	for (int i=0; i <=4; i++){
		digitalWrite(PIN_LED_BOOT, HIGH);
		delay(50);
		digitalWrite(PIN_LED_BOOT, LOW);
		delay(90);
	}

	// leds -------------------
	pinMode(PIN_LED_RED,	OUTPUT);
	pinMode(PIN_LED_GREEN,	OUTPUT);
	digitalWrite(PIN_LED_RED,	LOW);
	digitalWrite(PIN_LED_GREEN,	LOW);

	// relay -----------------------
	pinMode(PIN_RELAY,OUTPUT);

	// button -----------------------
	pinMode(PIN_BUTTON,INPUT);
	digitalWrite(PIN_BUTTON,HIGH);   // Activate internal pull-up
	debouncer.attach(PIN_BUTTON);
	debouncer.interval(DEBOUNCE_TIME);

 	// Dallas Temps
 	dallas.begin();
	dallas.setWaitForConversion(false);

	DEBUG_PRINTLN("++ Boot END ++");

}

// --------------------------------------------------------------------
void InitialState(){	
	if (init_msg_sent == false && isTransportReady() ) {
		DEBUG_PRINTLN("______ initialState START ______");

	   	init_msg_sent = true;
		Switch(loadState(CHILD_ID_RELAY)); 	// start as lastsaved
		send(msgRelay.set(is_on), true); // Send new state and request ack back
		DEBUG_PRINT("<-- Sending Relay State : "); 
		DEBUG_PRINTLN(is_on); 

		DEBUG_PRINTLN("______ initialState END   ______");
	}
}

// --------------------------------------------------------------------
void Toggle(){
	if(is_on){
		Switch(false);
	}
	else{
		Switch(true);
	}
}

// --------------------------------------------------------------------
void Switch(boolean state){
	if(state){
		digitalWrite(PIN_RELAY,		RELAY_ON);
		digitalWrite(PIN_LED_RED,	LOW);
		digitalWrite(PIN_LED_GREEN,	HIGH);
	}
	else{
		digitalWrite(PIN_RELAY,		RELAY_OFF);
		digitalWrite(PIN_LED_RED,	HIGH);
		digitalWrite(PIN_LED_GREEN,	LOW);		
	}

	is_on = state;

	// Store state in eeprom
	saveState(CHILD_ID_RELAY, is_on);
	
	DEBUG_PRINT("### Switched to : "); DEBUG_PRINT(is_on); DEBUG_PRINTLN(" ###");
}

// --------------------------------------------------------------------
void ProcessTemperature(){
	//http://playground.arduino.cc/Code/TimingRollover
	if( (long) ( millis() - next_report)  >= 0 ){
		next_report +=  (long) REPORT_TEMP_SEC * 1000 ;
		//Serial.print("next = "); Serial.println(next_report);

		DEBUG_PRINT("Sensing Temperature... ");

		dallas.requestTemperatures(); // Send the command to get temperatures
		wait( dallas.millisToWaitForConversion(dallas.getResolution()) + 10 ); // make sure we get the latest temps
		float dallasTemp = dallas.getTempCByIndex(0) + temp_offset;	

		DEBUG_PRINTLN(dallasTemp);
	
		if (! isnan(dallasTemp)) {
			dallasTemp = ( (int) (dallasTemp * 10 ) )/ 10.0 ; //rounded to 1 dec
			if (dallasTemp != lastDallasTemp	&& dallasTemp != -127.00 && dallasTemp != 85.0) {
				if (!metric) {
					dallasTemp = dallasTemp * 1.8 + 32.0;
				}
				lastDallasTemp = dallasTemp;
				DEBUG_PRINT("<--- Sending New Temperature : "); 
				DEBUG_PRINTLN(dallasTemp);
				send(msgTemp.set(dallasTemp, true)); // Send new temp and request ack back
				LedAnim(PIN_LED_GREEN);
			}
		}
	}
}

// --------------------------------------------------------------------
void LedAnim(byte led){
	//remember led state
	boolean initial = false;
	if( ( led == PIN_LED_GREEN && is_on) || led == PIN_LED_RED && !is_on ){
		initial= true;
	}
	
	for (int i=0; i <=2; i++){
		digitalWrite(led, ! initial);
		wait(50);
		digitalWrite(led, initial);
		wait(90);
	}
}
