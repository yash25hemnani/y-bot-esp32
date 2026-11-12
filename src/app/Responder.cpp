#include "app/Responder.h"
#include "comms/ble/BleManager.h"
#include "comms/ws/WsClient.h"

namespace
{
    // File-private: only Responder.cpp can see these.
    BleManager *bleManager = nullptr;
    WsClient *wsClient = nullptr;
}

void Responder::init(BleManager *ble, WsClient *ws)
{
    bleManager = ble;
    wsClient = ws;
}

void Responder::reply(const Command &cmd, const String &msg)
{
    switch (cmd.source)
    {
    case CommandSource::BLE:
        if (bleManager)
            bleManager->sendResponse(msg);
        break;

    case CommandSource::WS:
        if (wsClient)
            wsClient->enqueue(msg.c_str()); // sent later by the ws_client task, only while ONLINE
        break;
    }
}
