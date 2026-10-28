// Project universal include variables

/*============= ESP32 C3 pin definitions =============*/
#define WIFI_LED_PIN 2 // 3
#define BATTERY_CHARGING_PIN 27 // 6                    // 0: Charging, 1: Not charging
#define BATTERY_VOLTAGE_PIN 34 // 0                     
#define LP_LEDS_PIN 32 // 4
#define RGB_LEDS_PIN 33 // 5

/*============= LED chains =============*/
#define NUM_RGB_LEDS 16

/*============= SoftAP Credentials =============*/
static const char* const ssid = "LED_Project";
static const char* const password = "pass1234";

/*====== Battery voltage variable ======*/
extern float battery_voltage;

/*====== Battery charging boolean ======*/
extern bool battery_charges;