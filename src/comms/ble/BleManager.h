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
    SemaphoreHandle_t txMutex = nullptr;

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

    // Copy src into dst only if it fits (including '\0'). Never truncates.
    static bool copyField(char *dst, size_t dstSize, const String &src)
    {
        if (src.length() >= dstSize)
            return false;
        memcpy(dst, src.c_str(), src.length() + 1); // +1 copies the '\0'
        return true;
    }

    // Formats: "module:action", "module:action:key", "module:action:key:value"
    // Everything after the 3rd colon is the value, colons included.
    void handleWrite(String input) // by value: we modify our own copy
    {
        // 1. Strip trailing CR/LF only. Spaces are kept (a password may use them).
        while (input.endsWith("\r") || input.endsWith("\n"))
        {
            input.remove(input.length() - 1);
        }

        // 2. Find at most 3 colons. Never search past the third
        int c1 = input.indexOf(":");

        if (c1 <= 0) // missing, or empty module
        {
            sendResponse("error:format");
            return;
        }

        int c2 = input.indexOf(":", c1 + 1);
        int c3 = (c2 == -1) ? -1 : input.indexOf(':', c2 + 1);

        // 3. Slice the fields
        String moduleStr = input.substring(0, c1);
        String actionStr = (c2 == -1) ? input.substring(c1 + 1) : input.substring(c1 + 1, c2);

        String keyStr = "";
        String valueStr = "";

        if (c2 != -1)
            keyStr = (c3 == -1) ? input.substring(c2 + 1)
                                : input.substring(c2 + 1, c3);
        if (c3 != -1)
            valueStr = input.substring(c3 + 1);

        if (actionStr.length() == 0)
        {
            sendResponse("error:format");
            return;
        }

        // 4. Copy into the struct, rejecting anything too long
        Command cmd{};
        if (!copyField(cmd.module, sizeof(cmd.module), moduleStr) ||
            !copyField(cmd.action, sizeof(cmd.action), actionStr) ||
            !copyField(cmd.key, sizeof(cmd.key), keyStr) ||
            !copyField(cmd.value, sizeof(cmd.value), valueStr))
        {
            sendResponse("error:too_long");
            return;
        }

        // 5. Queue it and tell the client
        if (!commandRouter->submit(cmd))
        {
            sendResponse("error:busy");
            return;
        }

        sendResponse("ok:queued");

        Serial.println("BLE queued: " + moduleStr + "/" + actionStr + "/" + keyStr); // never log value
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
        txMutex = xSemaphoreCreateMutex();
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

    // Safe to call from any task: the mutex keeps setValue + notify together.
    void sendResponse(const String &payload)
    {
        if (!clientConnected || txCharacteristic == nullptr || txMutex == nullptr)
            return;

        xSemaphoreTake(txMutex, portMAX_DELAY);
        txCharacteristic->setValue(payload.c_str());
        txCharacteristic->notify();
        xSemaphoreGive(txMutex);
    }
};