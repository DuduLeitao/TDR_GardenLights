#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
//#include <WiFi.h>

#ifndef STASSID
#define STASSID "SSID_name"
#define STAPSK  "SSID_pass"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

// Static IP settings
IPAddress local_IP(192,168,1,149);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress primaryDNS(8,8,8,8);
IPAddress secondaryDNS(1,1,1,1);

ESP8266WebServer server(80);

const int led = LED_BUILTIN;
const int lightswitch = D3;
const int buttonPin = D2;
//const int interruptionPin = D2;
volatile unsigned long debounceDelay = 50;

const String postForms = "<html>\
  <head>\
    <title>ESP8266 Web Server POST handling</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Wellcome to the ESP8266 server!</h1><br>\
  </body>\
</html>";

void handleRoot() {
  server.send(200, "text/html", postForms);
}

void handlePlain() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    server.send(200, "text/plain", "POST body was:\n" + server.arg("plain"));
  }
}

void handleForm() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    String message = "POST form was:\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(200, "text/plain", message);
  }
}

void handleCommand() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    String message = "";

    if ( (server.argName(0) == "lightbox") and (server.arg(0) == "switchlight") ){
      message += "Switch the light";
      digitalWrite(lightswitch, !digitalRead(lightswitch));
      digitalWrite(led, !digitalRead(led));
      Serial.println(message);
    } else if ( (server.argName(0) == "lightbox") and (server.arg(0) == "lightstate") ){
      message += (digitalRead(lightswitch) == HIGH) ? "OFF" : "ON";
      Serial.println(message);
    }
    
    server.send(200, "text/plain", message);
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

//void ICACHE_RAM_ATTR interruptRoutine(void) {
//
//  static unsigned long lastDebounceTime = 0;
//  unsigned long debounceTime = millis();
//
//  if (debounceTime - lastDebounceTime > debounceDelay) {
//    interruptTriggered = true;
//  }
//  lastDebounceTime = debounceTime;
//}

void checkButtonPressedRoutine(void){

  // Variables will change:
  static int lastSteadyButtonState = HIGH;       // the previous steady state from the input pin
  static int lastFlickerableButtonState = HIGH;  // the previous flickerable state from the input pin
  static int buttonState = HIGH;                 // the current reading from the input pin
  static unsigned long lastDebounceTime = 0;
  static unsigned long debounceTime = 0;
  static bool buttonPressed = false;

  // Read the state of the button:
  buttonState = digitalRead(buttonPin);

  // Check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the button changed, due to noise or pressing:
  if (buttonState != lastFlickerableButtonState) {
    // Reset the debouncing timer
    lastDebounceTime = millis();
    // Save the the last flickerable state
    lastFlickerableButtonState = buttonState;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // If the button state has changed:
    if(lastSteadyButtonState == HIGH && buttonState == LOW){
      digitalWrite(lightswitch, !digitalRead(lightswitch));
      digitalWrite(led, !digitalRead(led));
    }
    
    // Save the the last steady state
    lastSteadyButtonState = buttonState;
  }
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  pinMode(lightswitch, OUTPUT);
  digitalWrite(lightswitch, HIGH);
  pinMode(buttonPin, INPUT_PULLUP);
//  pinMode(interruptPin, INPUT_PULLUP);
//  attachInterrupt(digitalPinToInterrupt(interruptPin), interruptRoutine, FALLING);
  Serial.begin(115200);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)){
    Serial.println("Something failed during WiFi static IP configuration.");
  }
  
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/postplain/", handlePlain);
  server.on("/postform/", handleForm);
  server.on("/postcommand/", handleCommand);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  checkButtonPressedRoutine();
}
