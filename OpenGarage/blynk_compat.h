#pragma once

// OpenGarage compatibility bridge for Blynk 1.3.x / Blynk IoT.
// Define the real template values in build flags when using Blynk IoT.
#ifndef BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_ID "TMPL_OPENGARAGE"
#endif
#ifndef BLYNK_TEMPLATE_NAME
#define BLYNK_TEMPLATE_NAME "OpenGarage"
#endif
#ifndef OG_BLYNK_EVENT_CODE
#define OG_BLYNK_EVENT_CODE "opengarage"
#endif

#include <BlynkSimpleEsp8266.h>

inline void og_blynk_notify(const String& message) {
  Blynk.logEvent(OG_BLYNK_EVENT_CODE, message);
}
