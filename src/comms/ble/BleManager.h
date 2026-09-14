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
#define CHARACTERISTIC_UUID_RX "12345678-1234-1234-1234-1234567890cd" // client -> device (write)
#define CHARACTERISTIC_UUID_TX "12345678-1234-1234-1234-1234567890ce" // device -> client (notify)

class BleManager
{
private:
    BLECharacteristic *rxCharacteristic;
    BLECharacteristic *txCharacteristic;
    CommandRouter *commandRouter;

    bool clientConnected = false;

    class ServerCallback : public BLEServerCallbacks
    {
    private:
        BleManager *manager;

    public:
        ServerCallback(BleManager *mgr) : manager(mgr) {}
        void onConnect(BLEServer *server) override { manager->clientConnected = true; }
        void onDisconnect(BLEServer *server) override
        {
            manager->clientConnected = false;
            server->getAdvertising()->start(); // resume advertising after disconnect
        }
    };

    void handleWrite(const String &value)
    {
        // Expected format "module:action", "module:action:key" (e.g. reads), or "module:action:key:value"
        std::vector<String> splits = split(':', value);

        if (splits.size() < 2 || splits.size() > 4)
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
        server->setCallbacks(new ServerCallback(this));
        BLEService *service = server->createService(SERVICE_UUID);

        rxCharacteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
        rxCharacteristic->setCallbacks(new WriteCallback(this));

        txCharacteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
        txCharacteristic->addDescriptor(new BLE2902()); // required for notify subscriptions

        service->start();

        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID(SERVICE_UUID);
        advertising->start();

        Serial.println("BLE advertising started, waiting for writes...");
    }

    void sendResponse(const String &payload)
    {
        if (!clientConnected || txCharacteristic == nullptr)
            return;
        txCharacteristic->setValue(payload.c_str());
        txCharacteristic->notify();
    }
};