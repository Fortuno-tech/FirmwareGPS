# Backend contract (current firmware)

This contract describes the wire behavior implemented by the firmware on the
`task/t2-backend-contract` baseline. Vehicle topics use `VEH001`, the configured
`MqttIdentity::vehicleId()`. The device identifier is loaded from provisioned
NVS and is not the vehicle ID.

## Topics, QoS, and retain

| Direction | Topic | QoS | Retain | Purpose |
|---|---|---:|---|---|
| Device subscribes | `fleet/VEH001/cmd` | 1 | Backend MUST publish non-retained commands | Signed remote commands |
| Device publishes | `fleet/VEH001/cmd/ack` | 1 | No | Receipt decision (`RECEIVED_ACCEPTED` / `RECEIVED_REJECTED`) |
| Device publishes | `fleet/VEH001/engine` | 1 | No | Engine state and post-execution command confirmation |
| Device publishes | `fleet/VEH001/telemetry` | 1 | No | Aggregate telemetry |
| Device publishes | `fleet/VEH001/gps` | 1 | No | GPS channel |
| Device publishes | `fleet/VEH001/fuel` | 1 | No | Fuel channel |
| Device publishes | `fleet/VEH001/battery` | 1 | No | Battery channel |
| Device publishes | `fleet/VEH001/rfid` | 1 | No | RFID event |
| Device publishes | `fleet/VEH001/alarm` | 1 | No | Alarm |
| Device publishes | `fleet/VEH001/status` | 1 | No | Device status |

The SIM7600 client publishes QoS 1 without a retained flag. The command
subscription requests QoS 1. Although the topic constants include OTA and LWT,
the current active connection path does not subscribe to OTA or configure an
LWT; those are not part of the active wire contract. The status topic is not
retained by the active publisher.

## Command JSON

`fleet/VEH001/cmd` accepts one JSON object with exactly these fields (field
order in the JSON is not significant):

```json
{
  "id": "cmd-valid-001",
  "seq": 1,
  "command": "STOP",
  "timestamp": 1000,
  "mac": "64 lowercase or uppercase hexadecimal characters"
}
```

`id` is 1–63 ASCII letters, digits, `_`, or `-`. `seq` is a positive unsigned
64-bit integer and must be strictly greater than the last accepted sequence
persisted in NVS. Commands are `STOP`, `ENGINE_OFF`, `KILL`, `ENGINE_ON`,
`SOS_ACK`, and `GET_STATUS`. Duplicate IDs in the persisted 16-ID history are
rejected even when the sequence is higher.

`timestamp`, when present, is an unsigned Unix timestamp in whole seconds. A
reliable RTC requires a timestamp; the absolute difference from device time
must be at most 60 seconds (the boundary is inclusive). If the clock is not
reliable, the time-window check is inactive; the persisted sequence and MAC
checks remain mandatory. The timestamp is still authenticated if supplied.

### Canonical HMAC-SHA256 input

The MAC is HMAC-SHA256 using the device's provisioned 32-byte root key. The
input is UTF-8/ASCII bytes, with no trailing newline:

```text
FORTICO-CMD-V1\n<deviceId>\n<id>\n<seq>\n<command>\n<timestamp-or-hyphen>
```

Fields are always in the displayed order, irrespective of JSON property
order. Decimal integers have no sign or leading zeroes. The separators are
single LF bytes (`0x0a`). If the timestamp is absent, its canonical field is
the single ASCII hyphen (`-`). `mac` is not part of the canonical input.

## Receipt ACK and execution confirmation

The receipt ACK on `fleet/VEH001/cmd/ack` is queued after security validation
and command acceptance/rejection. It means **received and accepted/rejected for
processing**, not that a relay action completed:

```json
{
  "phase": "RECEIVED_ACCEPTED",
  "id": "cmd-valid-001",
  "deviceId": "DEVICE-1",
  "vehicleId": "VEH001",
  "timestamp": "2026-11-06T12:34:56Z",
  "command": "STOP",
  "success": true,
  "error": null,
  "firmware": "version"
}
```

After the controller executes a relay action, it queues a correlated message
on `fleet/VEH001/engine`. It is emitted for success or failure:

```json
{
  "event": "command_execution",
  "id": "cmd-valid-001",
  "deviceId": "DEVICE-1",
  "vehicleId": "VEH001",
  "timestamp": "2026-11-06T12:34:56Z",
  "command": "STOP",
  "success": true,
  "error": null,
  "relay": "LOCKED",
  "firmware": "version"
}
```

Execution `success` requires the controller to permit the action, the relay
driver to report success, and the final relay state to match the request.
Failure uses a stable error string and reports the observed relay state. Either
message can be delayed or lost before delivery; commands are not retried by
the firmware, and the backend must correlate both phases by `id`. A missing
receipt ACK does not prove that the queued action was canceled.

## Telemetry envelope and deduplication

The channel payload is JSON. Its root `deviceId` identifies the device; the
persisted outbox adds an integer `outboxSeq` root property immediately before
publishing, for example:

```json
{
  "outboxSeq": 42,
  "deviceId": "DEVICE-1",
  "vehicleId": "VEH001",
  "timestamp": "2026-11-06T12:34:56Z"
}
```

The stored sequence is monotone and survives reboot. The backend deduplication
key for outbox deliveries is the pair `(deviceId, outboxSeq)`, not command
`seq` and not MQTT packet identifier. The record remains in the SD outbox until
the publish call succeeds; QoS 1 may still result in broker/client redelivery,
so consumers must deduplicate.

Channel payloads are currently produced as follows:

* `telemetry`: `deviceId`, `vehicleId`, nullable ISO-8601 UTC `timestamp`,
  nested `gps`, `fuel`, `battery`, `rfid`, `engine`, plus `network`, `sdcard`,
  and `firmware`.
* `gps`: device/vehicle IDs, nullable timestamp, latitude, longitude, altitude,
  speed, course, satellites, `fix`, and `fixAgeMs`.
* `fuel`: device/vehicle IDs, nullable timestamp, liters, percentage, anomaly.
* `battery`: device/vehicle IDs, nullable timestamp, voltage, percentage, and
  critical flag.
* `rfid`: event data and authorization result.
* `engine`: relay state and local authorization, or the execution confirmation
  schema above.
* `alarm`: alert event and context.
* `status`: uptime, free heap, temperature, connectivity/service states,
  nullable timestamp, and firmware.

An invalid/unavailable RTC is represented by JSON `null` for ordinary
telemetry timestamps. Execution events use the same context builder.

## Error codes and policy outcomes

The receipt's `error` is `null` on acceptance; otherwise the current handler
can report `INVALID_PAYLOAD`, `SECURITY_NOT_CONFIGURED`,
`DEVICE_ID_UNAVAILABLE`, `INVALID_COMMAND`, `TIME_REQUIRED`, `EXPIRED`,
`BAD_MAC`, `REPLAY_STATE_UNAVAILABLE`, `REPLAY`,
`REPLAY_STATE_WRITE_FAILED`, `LOCAL_AUTH_REQUIRED`, `INVALID_TOPIC`,
`ENGINE_OFF_REJECTED`, `KILL_MOTION_UNSAFE`, `KILL_REJECTED`,
`KILL_LATCHED`, `ENGINE_ON_REJECTED`, `STATUS_REJECTED`, or `UNKNOWN_COMMAND`.
Malformed payloads use `id: "unknown"` and `command: "UNKNOWN"` in the
negative receipt ACK; invalid topics are audited but are not ACKed. The
post-execution confirmation uses controller/relay error codes defined by the
controller and carries the actual relay state.

## Test vectors

These are documentation-only keys and commands. The key is the 32-byte
sequence `00` through `1f`; it is never a device credential. The native
`test_mqtt_command_security` tests use these same canonical strings and MACs.
The evaluation clock is Unix second `1000`.

**Valid, then replayed** (the exact same payload is sent twice; first accepted,
second rejected as `REPLAY`):

```json
{"id":"cmd-valid-001","seq":1,"command":"STOP","timestamp":1000,"mac":"1c84b54cfae68b9b598d1e5dc6d777b1cf1fe0c11674b23d9a81913e101f04f5"}
```

Canonical input:

```text
FORTICO-CMD-V1\nDEVICE-1\ncmd-valid-001\n1\nSTOP\n1000
```

**Expired** (valid signature; 100 seconds old, therefore rejected as
`EXPIRED`):

```json
{"id":"cmd-expired-001","seq":2,"command":"STOP","timestamp":900,"mac":"0c7e0dc999d1a3364182f2c789d9d43a0decadb76957a7de8dc87cf0b1002138"}
```

Canonical input:

```text
FORTICO-CMD-V1\nDEVICE-1\ncmd-expired-001\n2\nSTOP\n900
```
