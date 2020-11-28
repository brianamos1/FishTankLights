/*
   Copyright (c) 2020, Amos Computing
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * This Version removes the delay code and replaces it with milli's to allow querying the chip at any time, instead of waiting for the light cycle to run.
 * 
 * 
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "SSID_PSK"
#endif

//setup pixels
#define NUM_LEDS 50
#define DATA_PIN 4

CRGB leds[NUM_LEDS];

const char *ssid = STASSID;
const char *password = STAPSK;

int buttonState = 0;
int light_reason = 0; 
int light_on = 0;
int tzone = -60*60*7;
int hh_old = 0;
int hh = 0;
int mm = 0;
int state_change=0; // tells the loop which state change is in progress
unsigned long interval = 10000; //10 second interval
unsigned long last_millis;
unsigned long led_millis;
int cycle_number=0;
const int brightness_max = 255;
int brightness_interval = 15;
int brightness_current;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org",tzone,60000);

ESP8266WebServer server(80);

const int led = 2;
const int buttonPin = 5;

void handleRoot() {
  digitalWrite(led, 1);
  char temp[500];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
    char cur_time[100]; 
  //cur_time = timeClient.getFormattedTime();
  snprintf(cur_time, 100, timeClient.getFormattedTime().c_str());
  
  char status_reason[255];
  if(light_reason == 0){
    snprintf(status_reason, 20, "Status not set"); 
  }else if(light_reason == 1){
    snprintf(status_reason, 92, "Lights turned on because time is between 3pm and 9pm, they were off, and the button is off");
  }else if(light_reason == 2){
    snprintf(status_reason, 80, "Lights turned off because it's after 9pm and they were on and the button is off");
  }else if(light_reason == 3){
    snprintf(status_reason, 82, "Lights turned off because it's before 3pm and they were on and the button is off");
  }else if(light_reason == 4){
    snprintf(status_reason, 60, "Lights turned on because they were off and the button is on");
  }else{
    snprintf(status_reason, 45, "Error, status indicating non-existant code");
  }

  char light_state[40];
  if(light_on == 1){
    snprintf(light_state,3, "On");
  }else if(light_on == 0){
    snprintf(light_state, 4, "Off");
  }else{
    snprintf(light_state,30, "Error, light state not set");
  }

  
  snprintf(temp, 500,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>FTL v0.3</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Fish Tank Lights v0.3</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Time: %s</p>\
    <p>Light Status: %s</p>\
    <p>Button: %02d</p>\
    <p>Status: %s</p>\
  </body>\
</html>",

           hr, min % 60, sec % 60, cur_time, light_state, buttonState, status_reason
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
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
  digitalWrite(led, 0);
}

void drawGraph() {
  String out;
  out.reserve(2600);
  char temp[70];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}



void light_off(){
  digitalWrite(led,0);
  Serial.println("Lights Off");
  LEDS.showColor(CRGB(0,0,0));
  
}

void test_leds(){
  Serial.println("Testing LED String Performance");
  // Move a single white led 
  digitalWrite(led,1);
  FastLED.setBrightness(50);
  for(int whiteLed = 0; whiteLed < 50; whiteLed = whiteLed + 1) {
      // Turn our current led on to white, then show the leds
      leds[whiteLed] = CRGB::White;

      // Show the leds (only one of which is set to white, from above)
      FastLED.show();

      // Wait a little bit
      delay(200);

      // Turn our current led back to black for the next loop around
      //leds[whiteLed] = CRGB::Black;
  }
  LEDS.showColor(CRGB(0,0,0));
  delay(10000);
  digitalWrite(led,0);
  Serial.println("Test Completed");
     
}

void lightStatus(){
  if(light_reason == 0){
    Serial.println("Status not set"); 
  }else if(light_reason == 1){
    Serial.println("Lights turned on because time is between 3pm and 9pm, they were off, and the button is off");
  }else if(light_reason == 2){
    Serial.println("Lights turned off because it's after 9pm and they were on and the button is off");
  }else if(light_reason == 3){
    Serial.println("Lights turned off because it's before 3pm and they were on and the button is off");
  }else if(light_reason == 4){
    Serial.println("Lights turned on because they were off and the button is on");
  }else{
    Serial.println("Error, status indicating non-existant code");
  }
}

void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(buttonPin, INPUT);
  digitalWrite(led, 0);
  delay(2000); //sanity check - allows reprogramming if accidentally blowing power with LEDs
  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.showColor(CRGB(0,0,0));
  
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
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

  //setup NTP 
  timeClient.begin();
  timeClient.update();

  hh=timeClient.getHours();
  mm=timeClient.getMinutes();


  server.on("/", handleRoot);
  server.on("/test.svg", drawGraph);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  test_leds();
}

void loop(void) {
  digitalWrite(16,HIGH);
  timeClient.update();
  buttonState = digitalRead(buttonPin);
  unsigned long current_millis = millis();

  if(current_millis - led_millis >= interval){
    //flash led
    digitalWrite(16, LOW);
    delay(5);
    digitalWrite(16, HIGH);
    led_millis = current_millis;
  }
  hh = timeClient.getHours();
  mm = timeClient.getMinutes();
 
  if((hh>=15) && (hh<=20) && (light_on==0) && (buttonState==LOW)){ //turn on light between 3pm and 9pm

    //Serial.println("Lights turned on because time is between 3pm and 9pm, they were off, and the button is off");
    state_change = 1; // start_light();
    
    light_reason = 1;
  }else if((hh>20) && (light_on==1) && (buttonState==LOW)){ //turn off at 9 or later
    //Serial.println("Lights turned off because it's after 9pm and they were on and the button is off");
    state_change = 2;
    
    light_reason = 2;
  }else if((hh<15) && (light_on==1) && (buttonState==LOW)){ //turn off earlier than 3
    //Serial.println("Lights turned off because it's before 3pm and they were on and the button is off");
    state_change = 2; //end_light();
    
    light_reason = 3;
  }else if((buttonState == HIGH) && (light_on == 0)){ //turn on light when switch forces it on 
    //Serial.println("Lights turned on because they were off and the button is on");
    state_change = 1;//start_light();
    
    light_reason = 4;
  }

//  if(buttonState == HIGH){
//    digitalWrite(led,LOW);
//  }else{
//    digitalWrite(led,HIGH);
//  }

  if(state_change == 1){ //turn lights on
    //check last millis and see if interval has been reached
    if(current_millis - last_millis >= interval){

      if(cycle_number == 0){//initial cycle
        Serial.println("Turning Lights On");
        digitalWrite(led,LOW);
        LEDS.showColor(CRGB(0,0,0));
        brightness_current = 0;
        lightStatus();
      }
      cycle_number++;
      brightness_current += brightness_interval;
      
      if(brightness_current >= brightness_max){ //process state change end
        brightness_current = brightness_max;
        state_change = 0;
        cycle_number = 0;
        Serial.println("Lights turned on");
        light_on = 1;
      }
      Serial.print("Brightness: ");
      Serial.println(brightness_current);
      //set light brighness
      FastLED.setBrightness(brightness_current);
      LEDS.showColor(CRGB(255,255,255));
      
      //reset millis counter
      last_millis = current_millis;
      
      
    }
  }else if(state_change == 2){ //turn lights off
    //check last millis and see if interval has been reached
    if(current_millis - last_millis >= interval){

      if(cycle_number == 0){//initial cycle
        Serial.println("Turning Lights Off");
        digitalWrite(led,HIGH);
        lightStatus();
        
        brightness_current = brightness_max;
      }
      cycle_number++;
      brightness_current -= brightness_interval;
      
      Serial.print("Brightness: ");
      Serial.println(brightness_current);
      
      //set light brighness
      FastLED.setBrightness(brightness_current);
      LEDS.showColor(CRGB(255,255,255));
      
      if(brightness_current <= 0){ //process state change end
        brightness_current = 0;
        state_change = 0;
        cycle_number = 0;
        light_off();
        light_on = 0;
        Serial.println("Lights turned off");
      }

      
      
      //reset millis counter
      last_millis = current_millis;
      
      
    }
  }

  
  server.handleClient();
  MDNS.update();

}
