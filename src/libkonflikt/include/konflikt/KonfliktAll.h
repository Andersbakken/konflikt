#pragma once

// Main Konflikt library header - includes all public headers

#include "ConfigManager.h"
#include "HttpServer.h"
#include "Konflikt.h"
#include "LayoutManager.h"
#include "Platform.h"
#include "Protocol.h"
#include "Rect.h"
#include "ServiceDiscovery.h"
#include "WebSocketClient.h"
#include "WebSocketServer.h"

namespace konflikt {

/// Library version
constexpr const char *VERSION = "2.0.0";

} // namespace konflikt
