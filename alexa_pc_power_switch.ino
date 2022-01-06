#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <alloca.h>
#include <fauxmoESP.h> // See: https://github.com/vintlabs/fauxmoESP

#define DEBUG
#define SSID "Your wifi SSID"
#define PASSWORD "Your wifi password"
#define DEVICE_NAME "PC" // Name of the discoverable Alexa device

#define F2A(f) (dtostrf(f, 0, 2, (char*)alloca(16))) // Inline conversion of float to string

// Global vars
WiFiClient client;
fauxmoESP fauxmo;
byte ledState = 0;
bool changeState = false;
bool changeStateTo = false;

// Function prototypes
void TryConnectWifi();
void SetLedState(bool state);
void DbgPrint(const char *fmt, ...);
void OnSetFauxmoState(unsigned char deviceId, const char * deviceName, bool state, unsigned char value);
void SetFauxmoState(bool state);

void setup() 
{ 
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println();
#endif

  // Initialise pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D5, INPUT);  

  // Initialise wifi
  WiFi.begin(SSID, PASSWORD);  

  // Initialise fauxmo
  fauxmo.addDevice(DEVICE_NAME);
  fauxmo.setState((unsigned char)0, false, (unsigned char)254); // initial state
  fauxmo.setPort(80); // required for gen3 devices
  fauxmo.enable(true);
  fauxmo.onSetState(OnSetFauxmoState);  
}

void SetLedState(bool state)
{ 
  digitalWrite(LED_BUILTIN, ledState = (byte)!state); 
}

void DbgPrint(const char* fmt, ...)
{
#ifdef DEBUG 
  static char buf[512] = {};

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  Serial.println(buf);
#endif
}

void TryConnectWifi() 
{
  if(WiFi.status() == WL_CONNECTED)
    return;

  DbgPrint("Connecting Wifi");

  while (WiFi.status() != WL_CONNECTED) 
  {
    SetLedState(false); delay(250);    
    SetLedState(true); delay(250);
  }
  
  if (WiFi.status() == WL_CONNECTED) 
    DbgPrint("Local IP: %s", WiFi.localIP().toString().c_str());  
}

void SetFauxmoState(bool state)
{
  fauxmo.setState((unsigned char)0, state, (unsigned char)254);
}

void OnSetFauxmoState(unsigned char deviceId, const char * deviceName, bool state, unsigned char value)
{
  DbgPrint("Device #%d (%s) state: %s value: %d", deviceId, deviceName, state ? "ON" : "OFF", value);

  changeState = true; // Set flag
  changeStateTo = state; // Set new state
}

void loop() 
{  
  TryConnectWifi(); // Potentially blocking. Will only try to reconnect if disconnected.

  fauxmo.handle(); // Handle fauxmo events
  
  // Read current PC state
  bool currentPcState = !digitalRead(D5);

  // Align PC state to the updated fauxmo state
  if (changeState && changeStateTo != currentPcState)
  {
    changeState = false; // We processed the flag
    
    // Pulse the power button for 300 msec and also blink the led
    SetLedState(false);    
    DbgPrint("Pressing power button...");
    digitalWrite(D6, 1); 
    delay(300); 
    digitalWrite(D6, 0);   
    DbgPrint("Released power button.");     
    SetLedState(true);

    delay(5000); // Wait 5 sec for shutdown or boot to complete  
  }
  else
  {
    SetFauxmoState(currentPcState); // Update fauxmo state based on current PC state
  }
} 
