#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Ticker.h>

/* WiFi credentials */
#ifndef STASSID
#define STASSID "your_WiFi_SSID"
#define STAPSK  "your_WiFi_pass"
#endif

/* Store WiFi credentials in local constants */
static const char* ssid     = STASSID;
static const char* password = STAPSK;

/* Statig IP configuration */
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(1, 1, 1, 1);

/* ESP8266 server port configuration */
ESP8266WebServer server(80);

/* Web URL were the sunset time data can be found */
#define DATA_FETCH_WEBSITE "www.timeanddate.com"
#define DATA_FETCH_URL "/astronomy/spain/vigo"

/* Variable declaration and initializarion */
static const int led_builtin = LED_BUILTIN;
static const int rele_signal_light = D3;
static const int button_signal_light = D2;

/* Time period for debouncing the button signal [ms] */
volatile unsigned long debounce_delay = 50; // Time period for debouncing the button signal [ms].

/* Local time region offset from UTC time [s] */
const long utc_offset = 3600;

/* Variables to store current time and sunset moment in minutes since 00:00 h */
static int today_sunset_minute;
static int today_current_minute;

static Ticker ticker_lights_switch_on;
static Ticker ticker_routine;

static bool flag_hour = true;

#define TIME_SPAN_HOUR  3600

/* Welcome text showed when the user makes a request to root */
static const String welcome_root_text = \
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
static void handleRoot() {
  server.send(200, "text/html", welcome_root_text);
}

/* Function called when the request path matches URL/postcommand/ */
static void handleCommand()
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
static void handleNotFound()
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

/* Function that checks the current time with a NTP server and returns the time in minutes */
static int updateTodayMinutes(void)
{
  /* Define NTP Client to get time */
  WiFiUDP ntp_udp;
  NTPClient timeClient(ntp_udp, "pool.ntp.org", utc_offset);

  timeClient.begin();
  timeClient.update();
  today_current_minute = timeClient.getHours() * 60 + timeClient.getMinutes();
  Serial.print(F("Current time (minutes): "));
  Serial.println(today_current_minute);
}

/* Function that fetches the data from website to get the sunset time */
static void fetchSunsetTime(void)
{
  /* Web from where the data will be fetched */
  const char* host = DATA_FETCH_WEBSITE;

  /* Use WiFiClient class to create TCP connections */
  WiFiClientSecure client;

  /* Avoid the need to use IFTTT SSL certificates */
  client.setInsecure();

  Serial.print(F("Connecting to "));
  Serial.println(host);

  /* Try to open connection with the web */
  const int httpPort = 443;
  if (!client.connect(host, httpPort))
  {
    Serial.println(F("Connection failed :("));
    return;
  }

  /* Give the ESP8266 processing time */
  yield();

  /* Create a URI for the request */
  String url = DATA_FETCH_URL;

  Serial.print("Requesting URL: ");
  Serial.println(url);

  /* Send HTTP request */
  client.print(F("GET "));
  /* Send second half of a request (everything that comes after the base URL) */
  client.print(url);
  client.println(F(" HTTP/1.1"));
  /* Headers */
  client.print(F("Host: "));
  client.println(host);
  /* Do not use cache */
  client.println(F("Cache-Control: no-cache"));
  /* Check request state */
  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
    return;
  }

  /* Check HTTP response status */
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0)
  {
    Serial.print(F("Unexpected response for sunset data request: "));
    Serial.println(status);
    return;
  }

  /* Skip HTTP response headers */
  char end_of_headers[] = "\r\n\r\n";
  if (!client.find(end_of_headers))
  {
    Serial.println(F("Failed while skiping headers from sunset data response"));
    return;
  }

  /* Buffer to store the sunset time data */
  char sunset_time_char_array[32] = {0};

  /* Sequence of HTML that precedes the desired data */
  char sunset_data_preceder[] = "Civil Twilight</th><td class=tr>";

  /* Sequence of HTML that directly precedes the desired data. (We want the falue that comes after the '-') */
  char sunset_data_preceder_after_dash_string[] = "; ";

  /* Skip the first appearance of the sunset_data_preceder code sequence. (we are interested in the second one) */
  if (!client.find(sunset_data_preceder))
  {
    Serial.print(F("There was no match for \""));
    Serial.print(sunset_data_preceder);
    Serial.println(F("\""));
    return;
  }
  /* Locate the second appearance of the sunset_data_preceder code sequence */
  else if (!client.find(sunset_data_preceder))
  {
    Serial.print(F("There was no second match for \""));
    Serial.print(sunset_data_preceder);
    Serial.println(F("\""));
    return;
  }
  /* Skip the first time data value and position the reading pointer after the sunset_data_preceder_after_dash_string sequence of HTML code */
  else if (!client.find(sunset_data_preceder_after_dash_string))
  {
    Serial.print(F("There was no match for \""));
    Serial.println(sunset_data_preceder_after_dash_string);
    Serial.println(F("\""));
    return;
  }
  /* At this point the sunset time data has been located */
  else
  {
    /* Read until the reading pointer reaches the HTML character that comes directly after the sunset time data ('<') */
    client.readBytesUntil('<', sunset_time_char_array, sizeof(sunset_time_char_array));
//    Serial.print("Sunset time today: ");
//    Serial.print(sunset_time_char_array);
//    Serial.println(" h");

    /* Convert char array to minutes of the day */
    char time_hour_today_char_array[3];
    int time_hour_today;
    char time_minute_today_char_array[3];
    int time_minute_today;
    time_hour_today_char_array[0] = sunset_time_char_array[0];
    time_hour_today_char_array[1] = sunset_time_char_array[1];
    time_hour_today_char_array[2] = '\0';
    time_minute_today_char_array[0] = sunset_time_char_array[3];
    time_minute_today_char_array[1] = sunset_time_char_array[4];
    time_minute_today_char_array[2] = '\0';
    sscanf(time_hour_today_char_array, "%d", &time_hour_today);
    sscanf(time_minute_today_char_array, "%d", &time_minute_today);
    today_sunset_minute = time_hour_today * 60 + time_minute_today;
    Serial.print(F("Sunset time today (minutes): "));
    Serial.println(today_sunset_minute);
  }
  
  Serial.print(F("Connection to "));
  Serial.print(DATA_FETCH_WEBSITE);
  Serial.println(F(" closed"));  
}

/* Function in charche of getting the debounced value of the button_signal_light button signal */
static void checkButtonPressedRoutine(void)
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

static void switchOnLights(void)
{
  /* Switch on the lights */
  digitalWrite(led_builtin, LOW);

  /* Disable ticker_lights_switch_on ticker */
  ticker_lights_switch_on.detach();

  Serial.println(F("LED ON!"));
}


static void lightTickerManager(void)
{
  flag_hour = true;
}

void checkTimeManager(void)
{
  if (flag_hour)
  {
    flag_hour = false;
    updateTodayMinutes();
    if (today_current_minute > today_sunset_minute)
    {
      /* Disable ticker_lights_switch_on ticker */
      ticker_lights_switch_on.detach();

      /* Update sunset time variable */
      fetchSunsetTime();
    }
    if (((today_sunset_minute - today_current_minute) >= 0) && (((today_sunset_minute - today_current_minute) * 60) <= TIME_SPAN_HOUR))
    {
      /* Update time on ticker_lights_switch_on ticker */
      ticker_lights_switch_on.attach((today_sunset_minute - today_current_minute) * 60, switchOnLights);
      Serial.print(F("Lights will be switched on in "));
      Serial.print(today_sunset_minute - today_current_minute);
      Serial.println(F(" minutes"));
    }
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
    Serial.println(F("Something failed during WiFi static IP configuration."));
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
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  /* Request paths */
  server.on("/", handleRoot);
  server.on("/postcommand/", handleCommand);
  server.onNotFound(handleNotFound);

  /* Initialize HTTP server */
  server.begin();
  Serial.println(F("HTTP server started"));

  ticker_routine.attach(TIME_SPAN_HOUR, lightTickerManager);
}

void loop(void) {
  server.handleClient();
  checkButtonPressedRoutine();
  checkTimeManager();
}
