#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "app/CommandRouter.h"
#include "comms/protocol/Commands.h"
#include "utils/utils.h"

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "12345678-1234-1234-1234-1234567890cd"

class BleManager
{
private:
    BLECharacteristic *characteristic;
    CommandRouter *commandRouter;

    void handleWrite(const String &value)
    {
        // Expected format "module:action" or "module:action:key:value"
        std::vector<String> splits = split(':', value);

        if (splits.size() < 2 || splits.size() == 3 || splits.size() > 4)
        {
            Serial.println("Invalid Command!");
            return;
        }

        Command cmd{};

        String moduleStr = splits[0];
        String actionStr = splits[1];
        String keyStr = splits.size() > 2 ? splits[2] : "";
        String valueStr = splits.size() > 3 ? splits[3] : "";

        // strncpy does not null-terminate when the source is >= the buffer size,
        // and key/value now carry untrusted BLE input that gets passed to Preferences.
        strncpy(cmd.module, moduleStr.c_str(), sizeof(cmd.module) - 1);
        strncpy(cmd.action, actionStr.c_str(), sizeof(cmd.action) - 1);
        strncpy(cmd.key, keyStr.c_str(), sizeof(cmd.key) - 1);
        strncpy(cmd.value, valueStr.c_str(), sizeof(cmd.value) - 1);

        commandRouter->submit(cmd); // Non blocking submit
        Serial.println("BLE queued: " + moduleStr + "/" + actionStr + "/" + keyStr);
    }

    class WriteCallback : public BLECharacteristicCallbacks
    {
    private:
        BleManager *manager;

    public:
        WriteCallback(BleManager *mgr) : manager(mgr) {}

        void onWrite(BLECharacteristic *chr) override
        {
            String value = chr->getValue().c_str();
            manager->handleWrite(value);
        }
    };

public:
    void init(const char *deviceName, CommandRouter *router)
    {
        commandRouter = router;

        BLEDevice::init(deviceName);
        BLEServer *server = BLEDevice::createServer();
        BLEService *service = server->createService(SERVICE_UUID);

        characteristic = service->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);

        characteristic->setCallbacks(new WriteCallback(this));

        characteristic->addDescriptor(new BLE2902());

        service->start();

        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID(SERVICE_UUID);
        advertising->start();

        Serial.println("BLE advertising started, waiting for writes...");
    }
};