#pragma once
#include <cstdint>

// Where the robot's WebSocket connects.
// WS_HOST = your PC's LAN IP
static constexpr const char *WS_HOST = "10.157.118.20";
static constexpr uint16_t WS_PORT = 4000;
static constexpr const char *WS_PATH = "/device";
