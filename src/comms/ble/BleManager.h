#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "app/CommandRouter.h"
#include "comms/protocol/Commands.h"

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "12345678-1234-1234-1234-1234567890cd"

class BleManager
{
private:
    BLECharacteristic *characteristic;
    CommandRouter *commandRouter;

    void handleWrite(const String &value)
    {
        // Expected format "module:action"
        int sep = value.indexOf(':');

        if (sep < 0)
        {
            Serial.println("Bad BLE command format, expected module:action");
            return;
        }

        Command cmd{};
        String moduleStr = value.substring(0, sep);
        String actionStr = value.substring(sep + 1);

        strncpy(cmd.module, moduleStr.c_str(), sizeof(cmd.module));
        strncpy(cmd.action, actionStr.c_str(), sizeof(cmd.action));

        commandRouter->submit(cmd); // Non blocking submit
        Serial.println("BLE queued: " + moduleStr + "/" + actionStr);
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