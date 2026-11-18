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

        void onConnect(NimBLEServer *server) override
        {
            manager->clientConnected = true;
            // Free heap matters: TLS + NimBLE together can run the ESP32 short.
            Serial.printf("[BLE] client connected, free heap: %u\n", ESP.getFreeHeap());
        }

        void onDisconnect(NimBLEServer *server) override
        {
            manager->clientConnected = false;
            Serial.println("[BLE] client disconnected, advertising again");
            server->getAdvertising()->start(); // resume advertising after disconnect
        }
    };

    // Only for logging: shows whether the app turned on notifications for TX.
    // If this never prints "subscribed", every reply is sent to nobody.
    class TxCallback : public NimBLECharacteristicCallbacks
    {
    public:
        void onSubscribe(NimBLECharacteristic *chr, ble_gap_conn_desc *desc, uint16_t subValue) override
        {
            Serial.printf("[BLE] TX notify %s (value %u)\n", subValue ? "subscribed" : "unsubscribed", subValue);
        }
    };

    // Parsing lives in CommandParser (shared with the WebSocket). This only decides the BLE reply.
    void handleWrite(const String &input)
    {
        Command cmd;
        ParseResult r = CommandParser::parse(input, cmd);
        if (r != ParseResult::OK)
        {
            Serial.printf("[BLE] parse failed: %s\n", CommandParser::errorText(r));
            sendResponse(CommandParser::errorText(r));
            return;
        }

        cmd.source = CommandSource::BLE;
        if (!commandRouter->submit(cmd))
        {
            Serial.println("[BLE] router queue full, replying error:busy");
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
            // Log only the length: a write can carry wifi_password or hmac_secret.
            Serial.printf("[BLE] write received, %u bytes\n", value.length());
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
        txCharacteristic->setCallbacks(new TxCallback());

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
        {
            Serial.printf("[BLE] reply dropped (no client): %s\n", payload.c_str());
            return;
        }

        xSemaphoreTake(txMutex, portMAX_DELAY);
        // Pass bytes + length: setValue(const char*) on a characteristic hits NimBLE's
        // generic template and sends the 4-byte pointer address instead of the text.
        txCharacteristic->setValue((const uint8_t *)payload.c_str(), payload.length());
        txCharacteristic->notify();
        xSemaphoreGive(txMutex);
        // Safe to print: secret keys are write-only, so they never appear in a reply.
        Serial.printf("[BLE] notify sent: %s\n", payload.c_str());
    }
};