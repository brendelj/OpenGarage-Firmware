# Arduino IDE 2 build notes

This fork targets Arduino IDE 2.x and ESP8266 core 3.1.2+ while preserving the current OpenGarage firmware behavior.

## Compatibility changes

- Use Blynk 1.3.5 through an OpenGarage compatibility wrapper instead of calling removed `Blynk.notify()` directly.
- Use the modern ESP8266 HTTPClient API: `begin(WiFiClient, url)`.
- Use **PubSubClient by Nick O'Leary** (`knolleary/pubsubclient`). Do not use unrelated MQTT libraries that install a `src/MQTT.cpp` implementation.
- Include `library.properties` so Arduino IDE 2 can recognize the OpenGarage firmware directory when installed as a library.

## Blynk

Modern Blynk requires `BLYNK_TEMPLATE_ID` and `BLYNK_TEMPLATE_NAME`. The compatibility layer provides compile-time defaults, but a real Blynk IoT deployment should define the values assigned by Blynk and configure an event code for OpenGarage notifications.

## ESP8266

The modernization baseline is ESP8266 Arduino core 3.1.2, matching the environment used while repairing the older OpenGarage installation.
