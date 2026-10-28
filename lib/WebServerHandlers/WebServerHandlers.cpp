#include "WebServerHandlers.h"
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LibConfig.h>

/*============= Battery limits (V) =============*/
#define BATTERY_MAX_VOLTAGE 4.2f
#define BATTERY_MIN_VOLTAGE 3.3f

/*============= Effect periods (ms), speed 1 = slow, speed 100 = fast =============*/
#define BLINK_PERIOD_SLOW 2000
#define BLINK_PERIOD_FAST 100
#define FADE_PERIOD_SLOW 6000
#define FADE_PERIOD_FAST 500

// How often blink/fade outputs are refreshed
#define LED_UPDATE_INTERVAL_MS 10

enum led_effect_t { EFFECT_OFF, EFFECT_CONSTANT, EFFECT_BLINK, EFFECT_FADE };

struct led_chain_t {
  led_effect_t effect;
  uint32_t period_ms;     // full blink/fade cycle
  uint32_t start_ms;      // effect start time, so every effect begins from its start
  uint8_t level;          // last output level 0-255
};

static WebServer *web_server = nullptr;

static CRGB rgb_leds[NUM_RGB_LEDS];
static CRGB rgb_color = CRGB::Black;
static led_chain_t rgb_chain = { EFFECT_OFF, 0, 0, 0 };

static uint8_t red_duty = 0;            // PWM duty for full brightness (0-255)
static led_chain_t red_chain = { EFFECT_OFF, 0, 0, 0 };

static unsigned long last_led_update = 0;

/* ========== START: LED helper functions ========== */

// "off" | "constant" | "blink" | "fade" -> effect, -1 if unknown
static int parse_effect(const char *s){
  if (!s) return -1;
  if (strcmp(s, "off") == 0) return EFFECT_OFF;
  if (strcmp(s, "constant") == 0) return EFFECT_CONSTANT;
  if (strcmp(s, "blink") == 0) return EFFECT_BLINK;
  if (strcmp(s, "fade") == 0) return EFFECT_FADE;
  return -1;
}

// Maps the UI speed slider (1-100) to an effect period
static void chain_set(led_chain_t &chain, led_effect_t effect, int speed){
  speed = constrain(speed, 1, 100);
  uint32_t slow = (effect == EFFECT_FADE) ? FADE_PERIOD_SLOW : BLINK_PERIOD_SLOW;
  uint32_t fast = (effect == EFFECT_FADE) ? FADE_PERIOD_FAST : BLINK_PERIOD_FAST;

  chain.effect = effect;
  chain.period_ms = slow - (uint32_t)(speed - 1) * (slow - fast) / 99;
  chain.start_ms = millis();
}

// Output level (0-255) of a chain at the current time
static uint8_t effect_level(const led_chain_t &chain){
  switch (chain.effect){
    case EFFECT_CONSTANT:
      return 255;

    case EFFECT_BLINK: {
      uint32_t t = (millis() - chain.start_ms) % chain.period_ms;
      // 50% of selected period (Speed) OFF & 50% of selected period ON
      return (t < chain.period_ms / 2) ? 255 : 0;
    }

    case EFFECT_FADE: {
      // Triangle wave 0 -> 255 -> 0, gamma corrected so the fade looks smooth to the eye
      // With speed we select the period and the fading rises half of the period time and falls the other half
      uint32_t t = (millis() - chain.start_ms) % chain.period_ms;
      uint32_t half = chain.period_ms / 2;
      uint32_t lin = (t < half) ? (t * 255 / half) : ((chain.period_ms - t) * 255 / half);
      return fl::dim8_video(min(lin, (uint32_t)255));
    }

    default:
      return 0;
  }
}

static void rgb_refresh(bool force){
  // level: What fraction of the max selected brightness to output right now
  uint8_t level = effect_level(rgb_chain);
  if (!force && level == rgb_chain.level) return;
  rgb_chain.level = level;

  // rgb_color: what color, at what peak brightness
  CRGB c = rgb_color;
  // level: how much of that color to show right now
  c.nscale8_video(level);
  fill_solid(rgb_leds, NUM_RGB_LEDS, c);
  FastLED.show();
}

static void red_refresh(bool force){
  // level: What fraction of the max selected brightness to output right now
  uint8_t level = effect_level(red_chain);
  if (!force && level == red_chain.level) return;
  red_chain.level = level;

  // red_duty: selected max brightness, scale8_video: what level of this brightness to show
  analogWrite(LP_LEDS_PIN, scale8_video(red_duty, level));
}

/* ========== END: LED helper functions ========== */

void leds_init(){
  FastLED.addLeds<WS2812B, RGB_LEDS_PIN, GRB>(rgb_leds, NUM_RGB_LEDS);
  FastLED.clear(true);
  analogWrite(LP_LEDS_PIN, 0);
}

void leds_update(){
  if (millis() - last_led_update < LED_UPDATE_INTERVAL_MS) return;
  last_led_update = millis();

  rgb_refresh(false);
  red_refresh(false);
}

/* ========== START: Endpoint handlers ========== */

// POST /api/rgb  body: {"effect":"blink","color":"#FF0000","speed":50}
static void handle_rgb_post(){
  JsonDocument doc;
  if (deserializeJson(doc, web_server->arg("plain"))){
    web_server->send(400, "text/plain", "Invalid JSON");
    return;
  }

  int effect = parse_effect(doc["effect"].as<const char*>());
  const char *color = doc["color"].as<const char*>();
  if (effect < 0 || !color || strlen(color) != 7 || color[0] != '#'){
    web_server->send(400, "text/plain", "Invalid effect or color");
    return;
  }

  rgb_color = CRGB(strtoul(color + 1, nullptr, 16));
  chain_set(rgb_chain, (led_effect_t)effect, doc["speed"] | 50);
  rgb_refresh(true);

  // Brightness is already folded into the color, the brightest channel equals the UI brightness slider
  int rgb_brightness = max(rgb_color.r, max(rgb_color.g, rgb_color.b)) * 100 / 255;
  Serial.printf("[RGB] Apply -> effect: %s, color: %s, brightness: %d%%", doc["effect"].as<const char*>(), color, rgb_brightness);
  if (effect == EFFECT_BLINK || effect == EFFECT_FADE)
    Serial.printf(", speed: %d (period %lu ms)", doc["speed"] | 50, (unsigned long)rgb_chain.period_ms);
  Serial.print("\n");

  web_server->send(200, "application/json", "{\"ok\":true}");
}

// POST /api/red  body: {"effect":"fade","brightness":80,"speed":50}
static void handle_red_post(){
  JsonDocument doc;
  if (deserializeJson(doc, web_server->arg("plain"))){
    web_server->send(400, "text/plain", "Invalid JSON");
    return;
  }

  int effect = parse_effect(doc["effect"].as<const char*>());
  if (effect < 0){
    web_server->send(400, "text/plain", "Invalid effect");
    return;
  }

  int brightness = constrain(doc["brightness"] | 100, 0, 100);
  red_duty = brightness * 255 / 100;
  chain_set(red_chain, (led_effect_t)effect, doc["speed"] | 50);
  red_refresh(true);

  Serial.printf("[RED] Apply -> effect: %s, brightness: %d%% (duty %u)", doc["effect"].as<const char*>(), brightness, red_duty);
  if (effect == EFFECT_BLINK || effect == EFFECT_FADE)
    Serial.printf(", speed: %d (period %lu ms)", doc["speed"] | 50, (unsigned long)red_chain.period_ms);
  Serial.print("\n");

  web_server->send(200, "application/json", "{\"ok\":true}");
}

// GET /api/battery  response: {"voltage":3.92,"percent":69,"charging":false}
static void handle_battery_get(){
  float percent = (battery_voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 100.0f;

  JsonDocument doc;
  doc["voltage"] = serialized(String(battery_voltage, 2));
  doc["percent"] = (int)roundf(constrain(percent, 0.0f, 100.0f));
  doc["charging"] = battery_charges;

  String out;
  serializeJson(doc, out);
  web_server->send(200, "application/json", out);
}

/* ========== END: Endpoint handlers ========== */

void web_handlers_register(WebServer &srv){
  web_server = &srv;
  srv.on("/api/rgb", HTTP_POST, handle_rgb_post);
  srv.on("/api/red", HTTP_POST, handle_red_post);
  srv.on("/api/battery", HTTP_GET, handle_battery_get);
}
