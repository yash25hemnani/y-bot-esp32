#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h> // NimBLE: BLE-only stack, ~50 KB less heap than Bluedroid (TLS needs it)
#include "app/CommandRouter.h"
#include "comms/protocol/Commands.h"
#include "comms/protocol/CommandParser.h"

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID_RX "12345678-1234-1234-1234-1234567890cd" // client -> device (write)
#define CHARACTERISTIC_UUID_TX "12345678-1234-1234-1234-1234567890ce" // device -> client (notify)

class BleManager
{
private:
    NimBLECharacteristic *rxCharacteristic = nullptr;
    NimBLECharacteristic *txCharacteristic = nullptr;
    CommandRouter *commandRouter;

    bool clientConnected = false;
    SemaphoreHandle_t txMutex = nullptr;

    class ServerCallback : public NimBLEServerCallbacks
    {
    private:
        BleManager *manager;

    public:
        ServerCallback(BleManager *mgr) : manager(mgr) {}

        void onConnect(NimBLEServer *server) override { manager->clientConnected = true; }

        void onDisconnect(NimBLEServer *server) override
        {
            manager->clientConnected = false;
            server->getAdvertising()->start(); // resume advertising after disconnect
        }
    };

    // Parsing lives in CommandParser (shared with the WebSocket). This only decides the BLE reply.
    void handleWrite(const String &input)
    {
        Command cmd;
        ParseResult r = CommandParser::parse(input, cmd);
        if (r != ParseResult::OK)
        {
            sendResponse(CommandParser::errorText(r));
            return;
        }

        cmd.source = CommandSource::BLE;
        if (!commandRouter->submit(cmd))
        {
            sendResponse("error:busy");
            return;
        }

        sendResponse("ok:queued");
        Serial.printf("BLE queued: %s/%s/%s\n", cmd.module, cmd.action, cmd.key); // never log value
    }

    class WriteCallback : public NimBLECharacteristicCallbacks
    {
    private:
        BleManager *manager;

    public:
        WriteCallback(BleManager *mgr) : manager(mgr) {}

        void onWrite(NimBLECharacteristic *chr) override
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

        NimBLEDevice::init(deviceName);
        NimBLEServer *server = NimBLEDevice::createServer();
        server->setCallbacks(new ServerCallback(this));
        NimBLEService *service = server->createService(SERVICE_UUID);

        rxCharacteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE);
        rxCharacteristic->setCallbacks(new WriteCallback(this));

        // NimBLE adds the 2902 (subscribe) descriptor by itself for NOTIFY.
        txCharacteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY);

        service->start();

        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
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