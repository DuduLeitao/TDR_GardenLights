#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

/* WiFi credentials */
#ifndef STASSID
#define STASSID "DharmaWAN"
#define STAPSK  "MVNpWH5u"
#endif

/* Store WiFi credentials in local constants */
const char* ssid     = STASSID;
const char* password = STAPSK;

/* Statig IP configuration */
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(1, 1, 1, 1);

/* ESP8266 server port configuration */
ESP8266WebServer server(80);

/* Variable declaration and initializarion */
const int led_builtin = LED_BUILTIN;
const int rele_signal_light = D3;
const int button_signal_light = D2;
//const int interruptionPin = D2;
volatile unsigned long debounce_delay = 50;

/* Welcome text showed when the user makes a request to root */
const String welcome_root_text = \
"<html>\
  <head>\
    <title>ESP8266 Web Server POST handling</title>\
    <style>\
      body {background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #006600;}\
    </style>\
  </head>\
  <body>\
    <h1>Welcome to the TDR_GardenLight server!</h1><br>\
  </body>\
</html>";

/* Response given to requests to root */
void handleRoot() {
  server.send(200, "text/html", welcome_root_text);
}

/* Function called when the request path matches URL/postcommand/ */
void handleCommand()
{
  String message = "";

  /* Check if the request method used is POST */
  if (server.method() != HTTP_POST)
  {
    message += "Sorry, only POST requests allowed :(\n\nThe method you used was \"";
    message += (server.method() == HTTP_GET) ? "GET\"" : "POST\"";
    server.send(405, "text/plain", message);
  }
  else
  {
    /* Check if it is the command to switch light state */
    if ((server.argName(0) == "lightbox") && (server.arg(0) == "switchlight"))
    {
      message += "Switch the light";
      digitalWrite(rele_signal_light, !digitalRead(rele_signal_light));
      digitalWrite(led_builtin, !digitalRead(led_builtin));
      Serial.println(message);
    }
    /* Check if it is the command to check light state */
    else if ((server.argName(0) == "lightbox") && (server.arg(0) == "lightstate"))
    {
      message += (digitalRead(rele_signal_light) == HIGH) ? "OFF" : "ON";
      Serial.println(message);
    }
    server.send(200, "text/plain", message);
  }
}


/* Function called when request path does not match any of the ones available */
void handleNotFound()
{
  String message = "Sorry, there is no handler for your request :(\n\nYour request data:\n";
  message += "\tURI: ";
  message += server.uri();
  message += "\n\tMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n\tArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void checkButtonPressedRoutine(void)
{
  /* Local variables */
  static int last_steady_button_state = HIGH;       // The previous steady state from the input pin.
  static int last_flickerable_button_state = HIGH;  // The previous flickerable state from the input pin.
  static int button_state = HIGH;                   // The current reading from the input pin.
  static unsigned long last_debounce_time = 0;
  static unsigned long debounce_time = 0;
  static bool button_pressed = false;

  /* Read the state of the button */
  button_state = digitalRead(button_signal_light);

  /*
      Check to see if you just pressed the button
      (i.e. the input went from LOW to HIGH), and
      you've waited long enough since the last
      press to ignore any noise.
  */

  /* Check if the button changed due to noise or pressing */
  if (button_state != last_flickerable_button_state)
  {
    /* Reset the debouncing timer */
    last_debounce_time = millis();
    /* Save the the last flickerable state */
    last_flickerable_button_state = button_state;
  }

  if ((millis() - last_debounce_time) > debounce_delay)
  {
    /*
       Whatever the reading is at, it's been there
       for longer than the debounce delay, so take
       it as the actual current state.
    */

    /* Check if the button state has changed from unpressed (HIGH) to pressed (LOW) */
    if ((last_steady_button_state == HIGH) && (button_state == LOW))
    {
      digitalWrite(rele_signal_light, !digitalRead(rele_signal_light));
      digitalWrite(led_builtin, !digitalRead(led_builtin));
    }

    /* Save the the last steady state */
    last_steady_button_state = button_state;
  }
}

void setup(void)
{
  /* Configure GPIOs */
  pinMode(led_builtin, OUTPUT);
  digitalWrite(led_builtin, HIGH);
  pinMode(rele_signal_light, OUTPUT);
  digitalWrite(rele_signal_light, HIGH);
  pinMode(button_signal_light, INPUT_PULLUP);

  /* Configure serial port baudrate */
  Serial.begin(115200);

  /* Configure static IP address for the ESP8266 */
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("Something failed during WiFi static IP configuration.");
  }

  /* Initialize WiFi network settings */
  WiFi.begin(ssid, password);

  /* Wait for connection */
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  /* Show WiFi connection data through serial port */
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /* Request paths */
  server.on("/", handleRoot);
  server.on("/postcommand/", handleCommand);
  server.onNotFound(handleNotFound);

  /* Initialize HTTP server */
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  checkButtonPressedRoutine();
}
