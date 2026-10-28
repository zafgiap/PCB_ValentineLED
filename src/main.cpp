#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <LibConfig.h>
#include "project_helper_func.h"
#include "WebServerHandlers.h"

WebServer server(80);

bool battery_charges = false;
float battery_voltage = 0;
uint8_t wifi_led_state = 0;
unsigned long someone_connected_time = 0;
bool someone_connected_indication = false;

/* ========== START: Monitor helper functions & variables ========== */

unsigned long time1;
unsigned long time2;

void print_bat_volt(){

  // ======== Mock serial print values ========
  if (millis() - time1 > 10000){
    Serial.printf("Battery voltage: %f\n", battery_voltage);
    time1 = millis();
  }
}

void print_bat_chg(){

  // ======== Mock serial print values ========
  if (millis() - time2 > 10000){
    if (battery_charges == false)
      Serial.print("Battery does not charge\n");
    else
      Serial.print("Battery is charging !!!\n");

    time2 = millis();
  }
}

/* ========== END: Monitor helper functions & variables ========== */

void WiFi_monitor(){
  // If someone is connected to the SoftAP blink periodically the WiFi LED, else use the WiFi LED to indicate SoftAP is ready
  if (WiFi.softAPgetStationNum() > 0){
    if (!someone_connected_indication){
      someone_connected_time = millis();
      someone_connected_indication = true;
    }

    if (millis() - someone_connected_time > 500){
      wifi_led_state = !wifi_led_state;
      digitalWrite(WIFI_LED_PIN, wifi_led_state);
      someone_connected_time = millis();
    }

  }
  else{
    someone_connected_indication = false;

    // Constantly checking if WiFi SoftAP mode is working
    if (WiFi.getMode() == WIFI_MODE_AP){
      wifi_led_state = 1;
      digitalWrite(WIFI_LED_PIN, HIGH);
    }
    else{
      wifi_led_state = 0;
      digitalWrite(WIFI_LED_PIN, LOW);
    }

  }

}

void handleRoot() {
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    server.send(500, "text/plain", "index.html not found");
    return;
  }
  server.streamFile(f, "text/html");
  f.close();
}

void setup() {

  // Serial and I/O pins Init
  Serial.begin(115200);
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(BATTERY_CHARGING_PIN, INPUT);
  pinMode(BATTERY_VOLTAGE_PIN, INPUT);
  pinMode(LP_LEDS_PIN, OUTPUT);
  pinMode(RGB_LEDS_PIN, OUTPUT);

  // RGB (FastLED) and low power red (PWM) LED chains Init
  leds_init();

  // LittleFS mount
  if (!LittleFS.begin(true)) // format if not present (set to false if you don't want format)
    Serial.printf("LittleFS mount failed!\n");

  Serial.printf("LittleFS mount successful!\n");
  
  // SoftAP Init
  if (WiFi.softAP(ssid, password)){
    digitalWrite(WIFI_LED_PIN, HIGH);
    wifi_led_state = 1;
  }
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP()); Serial.print("\n");

  // UI begin page
  server.on("/", HTTP_GET, handleRoot);

  // LED & battery API endpoints
  web_handlers_register(server);

  server.begin();
  
}

void loop() {

  // serve HTTP files
  server.handleClient();

  // Constantly monitoring battery voltage
  battery_voltage = battery_volt_calc(analogRead(BATTERY_VOLTAGE_PIN));
  print_bat_volt();

  // Constantly monitoring if battery charges
  battery_charges = (digitalRead(BATTERY_CHARGING_PIN) == HIGH) ? false : true;
  print_bat_chg();

  // Constantly monitoring if WiFi clients are connected to the SoftAP & if the SoftAP is working
  WiFi_monitor();

  // Run LED blink/fade effects
  leds_update();

}