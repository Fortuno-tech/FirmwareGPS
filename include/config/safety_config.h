#pragma once

#include "config/hardware_config.h"

#ifndef FORTICO_KILL_MAX_SPEED_KMH
#define FORTICO_KILL_MAX_SPEED_KMH 0.5F
#endif

#ifndef FORTICO_KILL_GPS_MAX_AGE_MS
#define FORTICO_KILL_GPS_MAX_AGE_MS 25000U
#endif

#ifndef FORTICO_KILL_LOCK_UNTIL_REBOOT
#define FORTICO_KILL_LOCK_UNTIL_REBOOT 1
#endif

#ifndef FORTICO_ENGINE_ON_REQUIRE_LOCAL_AUTH
#define FORTICO_ENGINE_ON_REQUIRE_LOCAL_AUTH 1
#endif

#ifndef FORTICO_ENGINE_ON_REQUIRE_SAFE_STATE
#define FORTICO_ENGINE_ON_REQUIRE_SAFE_STATE 1
#endif

namespace fortico {
namespace safety {

constexpr float KILL_MAX_SPEED_KMH = FORTICO_KILL_MAX_SPEED_KMH;
constexpr uint32_t KILL_GPS_MAX_AGE_MS = FORTICO_KILL_GPS_MAX_AGE_MS;
constexpr bool KILL_LOCK_UNTIL_REBOOT =
    FORTICO_KILL_LOCK_UNTIL_REBOOT != 0;
constexpr bool ENGINE_ON_REQUIRE_LOCAL_AUTH =
    FORTICO_ENGINE_ON_REQUIRE_LOCAL_AUTH != 0;
constexpr bool ENGINE_ON_REQUIRE_SAFE_STATE =
    FORTICO_ENGINE_ON_REQUIRE_SAFE_STATE != 0;

static_assert(KILL_MAX_SPEED_KMH >= 0.0F,
              "KILL speed limit must not be negative");
static_assert(KILL_GPS_MAX_AGE_MS > 0U,
              "KILL GPS freshness limit must be positive");

}  // namespace safety
}  // namespace fortico
