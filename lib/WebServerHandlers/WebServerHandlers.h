#ifndef WEB_SERVER_HANDLERS_H
#define WEB_SERVER_HANDLERS_H

#include <WebServer.h>

// Registers the /api endpoints on the given server (call in setup, before server.begin())
void web_handlers_register(WebServer &srv);

// LED chains init (call once in setup)
void leds_init();

// Runs the blink/fade effects without blocking (call every loop)
void leds_update();

#endif /* WEB_SERVER_HANDLERS_H */
