#pragma once
#include <Arduino.h>
#include "comms/protocol/Commands.h"

// Sends a reply back the way the command came (BLE notify or WebSocket outbox).
// Only forward declarations here: including BleManager.h / WsClient.h would create
// an include loop (SettingsModule.h -> Responder.h -> WsClient.h -> SettingsModule.h).
class BleManager;
class WsClient;

namespace Responder
{
    void init(BleManager *ble, WsClient *ws);
    void reply(const Command &cmd, const String &msg);
}
