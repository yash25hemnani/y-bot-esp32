#pragma once
#include <cstdint>

// Where the robot's WebSocket connects.
// Render (production): host = bare domain, port 443, TLS on.
// Laptop (local test): host = your PC's LAN IP, port 4000, TLS off.
static constexpr const char *WS_HOST = "y-bot-admin-panel.onrender.com";
static constexpr uint16_t WS_PORT = 443;
static constexpr const char *WS_PATH = "/device";
static constexpr bool WS_USE_TLS = true;
