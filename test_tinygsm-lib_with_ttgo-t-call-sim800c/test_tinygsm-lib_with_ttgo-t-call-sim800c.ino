/*
install dependency library files
- TinyGSM: https://github.com/vshymanskyy/TinyGSM
- AXP202X: https://github.com/lewisxhe/AXP202X_Library
*/

// please select the corresponding T-Call model
#define SIM800C_AXP192_VERSION_20200609

// possible T-Call boards are: 
// #define SIM800L_IP5306_VERSION_20190610
// #define SIM800L_AXP192_VERSION_20200327
// #define SIM800C_AXP192_VERSION_20200609
// #define SIM800L_IP5306_VERSION_20200811

// sleep time in seconds 
#define TIME_TO_SLEEP  40          /* Time ESP32 will go to sleep (in seconds) */
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

// Server details
//const char server[] = "vsh.pp.ua";
//const char resource[] = "/TinyGSM/logo.txt";
const char server[]   = "open-hive.org";
const char resource[] = "/apiary/upload.php?user=[your-user-name]&id=[your-id]&node=10&dataset=timestamp-by-server,";
const int  port = 80;


// Your GPRS credentials (leave empty, if missing)
const char apn[]      = "iot.1nce.net"; // Your APN
const char gprsUser[] = ""; // User
const char gprsPass[] = ""; // Password
const char simPIN[]   = ""; // SIM card PIN code, if any

// sensor variables
char voltageChar[6];  // should handle xx.xx and null terminator


// define the serial console for debug prints, if needed
#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG SerialMon

#include "ttgo-t-call_power-management.h"

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to the module)
#define SerialAT  Serial1


// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800          // Modem is SIM800
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb


#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);


// modem functions

void setupModem()
{
#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    // Initialize the indicator as an output
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF);
}

void turnOffNetlight()
{
    SerialMon.println("Turning off SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=0");
}

void turnOnNetlight()
{
    SerialMon.println("Turning on SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=1");
}


void setup()
{
    // set console baud rate
    SerialMon.begin(115200);
    delay(10);

    // Start power management
    if (setupPMU() == false) {
        Serial.println("Setting power error");
    }

    // Some start operations
    setupModem();

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);
}

void loop()
{
    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    modem.restart();
//modem.init();
    // Turn off network status lights to reduce current consumption
    turnOffNetlight();

    // The status light cannot be turned off, only physically removed
    //turnOffStatuslight();

    // Or, use modem.init() if you don't need the complete restart
//    String modemInfo = modem.getModemInfo();
//    SerialMon.print("Modem: ");
//    SerialMon.println(modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
        modem.simUnlock(simPIN);
    }

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork(240000L)) {
//    if (!modem.waitForNetwork()) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" OK");

    // When the network connection is successful, turn on the indicator
    digitalWrite(LED_GPIO, LED_ON);

    if (modem.isNetworkConnected()) {
        SerialMon.println("Connected to mobile network");
    }

//gsm_info();
  
    SerialMon.print(F("Connecting to APN: "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
//        delay(10000);

        // in case the GSM / GPRS connection could not enabled
//???        gsm_disconnect();
   
        SerialMon.flush(); 
        // put ESP32 into deep sleep mode (with timer wake up)
        // Configure the wake up source as timer wake up  
        esp_sleep_enable_timer_wakeup(120 * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
        delay(100);
             
        return;
    }
    SerialMon.println(" OK");

    SerialMon.print("Connecting to ");
    SerialMon.print(server);
    if (!client.connect(server, port)) {
        SerialMon.println(" fail");
        delay(1000);
        return;
    }
    SerialMon.println(" OK");


    // read sensors 
    SerialMon.println("Read sensors:");
    // basic voltage reading 
    int batteryLevelRaw = analogRead(35);
    delay(100);
    float voltage = axp.getBattVoltage() / 1000;  // axp.getBattVoltage returns mV, we need V
    dtostrf(voltage, 4, 2, voltageChar);  // write to char array 
    SerialMon.print("basic battery lavel: ");
    SerialMon.println(voltageChar);
  
    // update URL 
    // 200 char should be ok for testing 
    char resource_data[200];
    sprintf(resource_data,"%s%s",resource,voltageChar);
    Serial.println(resource_data);
  

    // Make a HTTP GET request:
    SerialMon.println("Performing HTTP GET request...");
    client.print(String("GET ") + resource_data + " HTTP/1.1\r\n");
    client.print(String("Host: ") + server + "\r\n");
    client.print("Connection: close\r\n\r\n");
    client.println();

    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000L) {
        // Print available data
        while (client.available()) {
            char c = client.read();
            SerialMon.print(c);
            timeout = millis();
        }
    }
    SerialMon.println();

    // Shutdown
    client.stop();
    SerialMon.println(F("Server disconnected"));

    modem.gprsDisconnect();
    SerialMon.println(F("GPRS disconnected"));


    // DTR is used to wake up the sleeping Modem
#ifdef MODEM_DTR
    bool res;

//    modem.sleepEnable();

    delay(100);
/*
    // test modem response , res == 0 , modem is sleep
    res = modem.testAT();
    Serial.print("SIM800 Test AT result -> ");
    Serial.println(res);

    delay(1000);

    Serial.println("Use DTR Pin Wakeup");
    pinMode(MODEM_DTR, OUTPUT);
    //Set DTR Pin low , wakeup modem .
    digitalWrite(MODEM_DTR, LOW);


    // test modem response, res == 1, modem is wakeup
    res = modem.testAT();
    Serial.print("SIM800 Test AT result -> ");
    Serial.println(res);

*/
#endif

    // Make the LED blink three times before going to sleep
    int i = 3;
    while (i--) {
        digitalWrite(LED_GPIO, LED_ON);
        modem.sendAT("+SPWM=0,1000,80");
        delay(500);
        digitalWrite(LED_GPIO, LED_OFF);
        modem.sendAT("+SPWM=0,1000,0");
        delay(500);
    }

    // after all off
    modem.poweroff();

    SerialMon.println(F("Poweroff"));
    SerialMon.flush(); 

    // put ESP32 into deep sleep mode (with timer wake up)
    // Configure the wake up source as timer wake up  
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();

    /*
    The sleep current using AXP192 power management is about 500uA,
    and the IP5306 consumes about 1mA
    */
}
