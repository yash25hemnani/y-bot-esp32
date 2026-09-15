#include <Arduino.h>
#include "config/Pins.h"
#include "app/EventBus.h"
#include "drivers/ButtonDriver.h"
#include "drivers/TouchSensorDriver.h"
#include "modules/display/LedModule.h"
#include "modules/display/DisplayModule.h"
#include "modules/touch/TouchSensorModule.h"
#include "modules/settings/SettingsModule.h"
#include "app/CommandRouter.h"
#include "comms/wifi/WifiManager.h"
#include "comms/ble/BleManager.h"


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

  Wire.begin(SDA_PIN, SCK_PIN);
  DisplayModule::init();
  TouchSensorModule::init(&eventBus);

  // pipeline 1: sensor -> EventBus -> LedModule
  eventBus.subscribe(LedModule::onEvent);
  eventBus.subscribe(TouchSensorModule::onEvent);
  eventBus.init();
  ButtonDriver::init(BOOT_PIN, &eventBus);
  TouchSensorDriver::init(TOUCH_PIN, &eventBus);

  // pipeline 2: WiFi -> CommandRouter -> LedModule
  commandRouter.init();
  commandRouter.registerHandler("led", LedModule::onCommand);
  commandRouter.registerHandler("settings", SettingsModule::onCommand);
  wifiManager.init(&commandRouter);
  bleManager.init("Y-Bot", &commandRouter);
  SettingsModule::init(&bleManager);

  Serial.println("Ready. Press BOOT or send /cmd?module=led&action=toggle");
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000)); // main loop does nothing — everything lives in tasks
}