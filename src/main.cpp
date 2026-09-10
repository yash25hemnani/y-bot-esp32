#include <Arduino.h>
#include "config/Pins.h"
#include "app/EventBus.h"
#include "drivers/ButtonDriver.h"
#include "modules/display/LedModule.h"
#include "app/CommandRouter.h"
#include "comms/wifi/WifiManager.h"
#include "comms/ble/BleManager.h"

const char *WIFI_SSID = "Nokia 5.4";
const char *WIFI_PASSWORD = "alohmoraa";

EventBus eventBus;
CommandRouter commandRouter;
WifiManager wifiManager;
BleManager bleManager;

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("Booting...");

  LedModule::init(LED_PIN);

  // pipeline 1: sensor -> EventBus -> LedModule
  eventBus.subscribe(LedModule::onEvent);
  eventBus.init();
  ButtonDriver::init(BOOT_PIN, &eventBus);

  // pipeline 2: WiFi -> CommandRouter -> LedModule
  commandRouter.init();
  commandRouter.registerHandler("led", LedModule::onCommand);
  wifiManager.init(WIFI_SSID, WIFI_PASSWORD, &commandRouter);
  bleManager.init("Y-Bot", &commandRouter);

  Serial.println("Ready. Press BOOT or send /cmd?module=led&action=toggle");
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000)); // main loop does nothing — everything lives in tasks
}