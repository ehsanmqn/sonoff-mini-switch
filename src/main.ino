#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "tedon.h"

// Set web server and wifi client
WiFiServer server(80);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Definition of public variables
int pressedMoreThanFiveLoop = 0;
int lightFlashCounter = 0;
int whiteFlashStatus = 0;
int reconnectCounter = 0;
bool isPressed = false;
bool isConnected = false;

void setup() {
	// Setup serial
  Serial.begin(115200);

  // Set output mode for all GPIO pins
  pinMode(blueLED, OUTPUT);
  pinMode(midSwitchRelay, OUTPUT);

  delay(10);

	// Set IO status
  digitalWrite(blueLED, LOW);
  digitalWrite(midSwitchRelay, LOW);

  Serial.println("[ INFO ] Init WiFi");

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Uncomment and run it once, if you want to erase all the stored information
  // wifiManager.resetSettings();

  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // fetches ssid and pass from eeprom and tries to connect if it does not connect it starts an
  // access point with the specified name and goes into a blocking loop awaiting configuration
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect(wifiSSID);

  // or use this for auto generated name ESP + ChipID
  // wifiManager.autoConnect();

  // Configuration portal timeout
  wifiManager.setConfigPortalTimeout(60);

  // if you get here you have connected to the WiFi
  Serial.println("[ INFO ] Connected to WiFi: ");

  server.begin();
  client.setServer(dmiotServer, mqttPort);
  client.setCallback(on_message);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void loop() {
  // Check for connection
  if (reconnectCounter >= CHECK_CONNETCTION_ITERATIONS) {
    if ( !client.connected() )
    {
      reconnect();
    }

    reconnectCounter = 0;
  }

  client.loop();
  reconnectCounter++;
}

// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("[ INFO ] Topic: ");
  Serial.println(topic);
  Serial.print("[ INFO ] Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("[ EROR ] parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("getGpioStatus")) {

		// Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);

		String responseTopic = String(topic);

		responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  }
}

// Get GPIO RPC
String get_gpio_status() {

  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();

  data[String(1)] = gpioState[0] ? true : false;

  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

// Set GPIO RPC
void set_gpio_status(int pole, boolean enabled) {
  if (pole == 1) {
    if (enabled )
    {
      digitalWrite(midSwitchRelay, HIGH);
      digitalWrite(blueLED, HIGH);
    }
    else
    {
      digitalWrite(midSwitchRelay, LOW);
      digitalWrite(blueLED, LOW);
    }

    // Update GPIOs state
    gpioState[0] = enabled;
  }
}

// Reconnect to the server function
void reconnect() {
  // Connecting to the server
  Serial.println("[ INFO ] Connecting to DMIoT server ...");

  // Attempt to connect (clientId, username, password)
  if ( client.connect("Tedon 1 Pole Device", TOKEN, NULL) ) {
    Serial.println( "[ INFO ] Connected to the server" );

		digitalWrite(blueLED, LOW);
    isConnected = true;

		// Subscribing to receive RPC requests
    client.subscribe("v1/devices/me/rpc/request/+");

		// Sending current GPIO status
    Serial.println("[ INFO ] Sending current GPIO status ...");
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  } else {
    isConnected = false;
    Serial.print( "[FAILED] [ rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying in 5 seconds]" );
  }
}
