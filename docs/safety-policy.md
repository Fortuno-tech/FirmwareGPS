# Remote relay safety policy

These are compile-time policy settings. Defaults preserve the previously
executed firmware behavior; overrides are supplied as `-D` build flags, not
stored as runtime secrets.

| Build flag | Default | Behavior |
|---|---:|---|
| `FORTICO_KILL_MAX_SPEED_KMH` | `0.5` | KILL requires a finite, nonnegative GPS speed no greater than this value. |
| `FORTICO_KILL_GPS_MAX_AGE_MS` | `25000` | KILL requires a valid fix no older than this age; equality is fresh. |
| `FORTICO_KILL_LOCK_UNTIL_REBOOT` | `1` | KILL leaves the relay/control state locked until reboot. Startup initializes the relay cut and clears the volatile latch. |
| `FORTICO_ENGINE_ON_REQUIRE_LOCAL_AUTH` | `1` | ENGINE_ON requires active local RFID authentication. |
| `FORTICO_ENGINE_ON_REQUIRE_SAFE_STATE` | `1` | ENGINE_ON is accepted only from `WAIT_BADGE` or `TRACKING_ACTIVE`. |

With defaults, speed `0.50 km/h` is permitted and `0.51 km/h` is rejected.
Invalid/missing fixes, stale fixes, negative speeds, NaN, and infinity reject
KILL. A relay-driver rejection still results in a failed execution
confirmation.

## Decisions ouvertes

These values are not product-approved; changing any requires safety review and
hardware validation:

* **KILL speed 0.5 km/h:** accepting a nonzero speed risks cutting/locking the
  engine while the vehicle is moving; lowering the threshold may reject valid
  stationary GPS noise.
* **GPS freshness 25 seconds:** the current GNSS poll cadence is 10 seconds;
  increasing this allows older position/speed data, while lowering it may
  reject KILL during normal satellite dropouts.
* **KILL latch until reboot:** the default prevents re-engagement after a kill
  for the remainder of the boot. Clearing it in software could permit an
  unsafe restart; requiring power-cycle can delay recovery.
* **ENGINE_ON requires RFID and a safe state:** weakening either guard may
  allow remote start without driver presence or from an emergency state.

Native tests exercise policy boundaries only; they do not establish GNSS
accuracy, relay timing, or safety behavior on the installed vehicle.
