/*
 * 	Relay with toggle button and Status Leds + Temperature Sensor
 *	Copyright Francois Dechery 2016
 */

// includes ------------------------------------------------------------------------------
#include <SPI.h>
#include <MySensor.h>
#include <Bounce2.h>
#include <DallasTemperature.h>
#include <OneWire.h>

// Define --------------------------------------------------------------------------------
#define SERIAL_PRINT_DEBUG true	 //do we print serial

#define INFO_NAME 			"MonitorsRelay"
#define INFO_VERS 			"1.0"

#define GW_NODE_ID			101		// 255 for Auto
#define GW_REPEATER			true	// repeater mode

#define PIN_RELAY			2
#define PIN_ONEWIRE			3
#define PIN_BUTTON			4
#define PIN_LED_RED			5
#define PIN_LED_GREEN		6
#define PIN_LED_BOOT		5		// or 13 for internal led

#define CHILD_ID_RELAY		0
#define CHILD_ID_TEMP		1

#define REPORT_TEMP_SEC		60		// report temperature every x seconds
#define DEBOUNCE_TIME		50		// Button Debounce time (ms)
#define RELAY_ON			HIGH	// my Relay use LOW for ON, but I use 'NC' instead of 'NO' pins
#define RELAY_OFF			LOW		// my Relay use HIGH for OFF, but I use 'NC' instead of 'NO' pins

// Variables -----------------------------------------------------------------------------
boolean	metric				= true;	// use Celcius
boolean	is_on				= true; // current relay state
float	lastDallasTemp		= -100;	// latest Temperature
unsigned long next_report 	= 3000; // last temperature report time : start 3s after boot
const float temp_offset		= -1.0;	// calibrate DS18B20 deviation

// objects -------------------------------------------------------------------------------
MySensor			gw;
OneWire				oneWire(PIN_ONEWIRE);
DallasTemperature	dallas(&oneWire);
Bounce 				debouncer = Bounce(); 

MyMessage 			msgRelay(	CHILD_ID_RELAY,	V_LIGHT);
MyMessage 			msgTemp(	CHILD_ID_TEMP,	V_TEMP);



// ###### Setup ##########################################################################
void setup()  {  

	boot();

	// leds -------------------
	pinMode(PIN_LED_RED,	OUTPUT);
	pinMode(PIN_LED_GREEN,	OUTPUT);
	digitalWrite(PIN_LED_RED,	LOW);
	digitalWrite(PIN_LED_GREEN,	LOW);

	// relay -----------------------
	pinMode(PIN_RELAY,OUTPUT);
	//Switch(true);							// start as on
	Switch(gw.loadState(CHILD_ID_RELAY)); 	// start as lastsaved


	// button -----------------------
	pinMode(PIN_BUTTON,INPUT);
	digitalWrite(PIN_BUTTON,HIGH);   // Activate internal pull-up
	debouncer.attach(PIN_BUTTON);
	debouncer.interval(DEBOUNCE_TIME);

 	//My Sensors -----------------
	gw.begin(receiveMessage, GW_NODE_ID, GW_REPEATER);
	gw.sendSketchInfo(INFO_NAME, INFO_VERS);
	gw.present(CHILD_ID_RELAY, 	S_LIGHT);  
	gw.present(CHILD_ID_TEMP,	S_TEMP); 
//	metric = gw.getConfig().isMetric;

 	// Dallas Temps
 	dallas.begin();
	dallas.setWaitForConversion(false);
 
}

// ###### Loop ##########################################################################
void loop() {
	// handle repeater -----------------
	gw.process();

	//relay & button --------------------
	boolean changed = debouncer.update();
	int button_state = debouncer.read();
	if (changed && button_state == 0) {
		if(SERIAL_PRINT_DEBUG){
			Serial.println("BUTTON Toggle !!");
		}
		Toggle();
		gw.send(msgRelay.set(is_on), true); // Send new state and request ack back
	}

	// Dallas 1820 Sensor ---------------
	ProcessTemperature();
} 





// #######################################################################################
// ############################# FUNCTIONS ###############################################
// #######################################################################################


// ###### ReceiveMessage #################################################################
void receiveMessage(const MyMessage &message){
	if (message.isAck() ) {
	    if(SERIAL_PRINT_DEBUG){
			Serial.println("This is an ack from gateway. Returning WITHOUT Process ");
		}
		return;
	}

	if (message.type == V_LIGHT) {
		if(SERIAL_PRINT_DEBUG){
			Serial.print("Incoming change for sensor:"); Serial.print(message.sensor);
			Serial.print(", New state: "); Serial.println(message.getBool());
		}
		// Change relay state
		Switch(message.getBool());
		LedAnim(PIN_LED_RED);
   }
}

// ###### Toggle ##########################################################################
void Toggle(){
	if(is_on){
		Switch(false);
	}
	else{
		Switch(true);
	}
}

// ###### Switch #########################################################################
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
	gw.saveState(CHILD_ID_RELAY, is_on);
	
	if(SERIAL_PRINT_DEBUG){
		Serial.print("##### Switched to : "); Serial.print(is_on); Serial.println(" ##############");
	}
}

// ###### ProcessTemperature #############################################################
void ProcessTemperature(){
	//http://playground.arduino.cc/Code/TimingRollover
	if( (long) ( millis() - next_report)  >= 0 ){
		next_report +=  (long) REPORT_TEMP_SEC * 1000 ;
		//Serial.print("next = "); Serial.println(next_report);

		if(SERIAL_PRINT_DEBUG){
			Serial.print("Sensing Temperature... ");
		}

		dallas.requestTemperatures(); // Send the command to get temperatures
		gw.wait( dallas.millisToWaitForConversion(dallas.getResolution()) + 10 ); // make sure we get the latest temps
		float dallasTemp = dallas.getTempCByIndex(0) + temp_offset;	

		if(SERIAL_PRINT_DEBUG){
			Serial.println(dallasTemp);
		}
	
		if (! isnan(dallasTemp)) {
			dallasTemp = ( (int) (dallasTemp * 10 ) )/ 10.0 ; //rounded to 1 dec
			if (dallasTemp != lastDallasTemp	&& dallasTemp != -127.00 && dallasTemp != 85.0) {
				if (!metric) {
					dallasTemp = dallasTemp * 1.8 + 32.0;
				}
				lastDallasTemp = dallasTemp;
				if(SERIAL_PRINT_DEBUG){
					Serial.print("--> Sending New Temperature : "); Serial.println(dallasTemp);
				}
				gw.send(msgTemp.set(dallasTemp, 1)); // Send new temp and request ack back
				LedAnim(PIN_LED_GREEN);
			}
		}
	}
}


// ###### Boot Anim#######################################################################
void boot(){
	if(SERIAL_PRINT_DEBUG){
		Serial.begin(115000);
		Serial.println("+++++++++++++");
		Serial.println(" Booting...");
		Serial.println("+++++++++++++");
	}
	pinMode(PIN_LED_BOOT,	OUTPUT);
	digitalWrite(PIN_LED_BOOT,	LOW);

	for (int i=0; i <=4; i++){
		digitalWrite(PIN_LED_BOOT, HIGH);
		delay(50);
		digitalWrite(PIN_LED_BOOT, LOW);
		delay(90);
	}
}

// ###### Receive Anim ###################################################################
void LedAnim(byte led){
	//remember led state
	boolean initial = false;
	if( ( led == PIN_LED_GREEN && is_on) || led == PIN_LED_RED && !is_on ){
		initial= true;
	}
	
	for (int i=0; i <=2; i++){
		digitalWrite(led, ! initial);
		gw.wait(50);
		digitalWrite(led, initial);
		gw.wait(90);
	}
}




