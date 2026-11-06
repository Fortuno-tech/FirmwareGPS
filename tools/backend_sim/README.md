# FirmwareGPS MQTT backend simulator

This Python 3 tool publishes non-retained QoS 1 commands on
`fleet/<VEHICLE_ID>/cmd`, signs them with the contract's HMAC-SHA256 canonical
input, and listens on `cmd/ack`, `engine`, and `telemetry`. Telemetry deliveries
are deduplicated in-process using exactly `(deviceId, outboxSeq)`.

## Configure

Install the only runtime dependency:

```powershell
python -m pip install -r tools\backend_sim\requirements.txt
```

Set the broker and test identity from the shell. `HMAC_TEST_KEY` must be a
32-byte test key encoded as 64 hex characters; use a test-only device key, not
a production credential.

```powershell
$env:MQTT_HOST = "localhost"
$env:MQTT_PORT = "1883"
$env:MQTT_TLS = "false"
$env:MQTT_USERNAME = ""
$env:MQTT_PASSWORD = ""
$env:DEVICE_ID = "DEVICE-1"
$env:VEHICLE_ID = "VEH001"
$env:HMAC_TEST_KEY = "<64-hex-character test key>"
$env:COMMAND_SEQUENCE = "1"
```

Alternatively, set `MQTT_BROKER_URL` to `mqtt://host:1883` or
`mqtts://host:8883`. `MQTT_HOST`/`MQTT_PORT` override URL values; `MQTT_TLS`
overrides the URL's TLS default. `MQTT_USER` is accepted as an alias for
`MQTT_USERNAME`. For a private TLS CA, set `MQTT_TLS_CA_CERT` to its PEM path.
TLS certificate and hostname verification remain enabled.

## Mosquitto

Run Mosquitto locally with its default listener (`mosquitto -v`) or configure a
test-only listener with credentials as appropriate. For a default local broker,
use the configuration above, then attach the simulator and a test device to the
same broker:

```powershell
python -m tools.backend_sim --scenario valid
```

The simulator publishes only after all three response subscriptions are active.
Ensure the device is online and accepts commands for the configured vehicle ID.

## HiveMQ Cloud

Use the cluster hostname from HiveMQ Cloud, its TLS listener, and a dedicated
test account:

```powershell
$env:MQTT_HOST = "<cluster-host>"
$env:MQTT_PORT = "8883"
$env:MQTT_TLS = "true"
$env:MQTT_USERNAME = "<test-account>"
$env:MQTT_PASSWORD = "<test-password>"
python -m tools.backend_sim --scenario valid
```

If the cluster requires a non-system CA bundle, set `MQTT_TLS_CA_CERT` to the
PEM file. Do not disable certificate verification.

## Scenarios

```powershell
python -m tools.backend_sim --scenario valid
python -m tools.backend_sim --scenario replay
python -m tools.backend_sim --scenario expired
python -m tools.backend_sim --scenario invalid_signature
python -m tools.backend_sim --scenario unauthorized_action
python -m tools.backend_sim --scenario decreasing_sequence
python -m tools.backend_sim --scenario kill_no_valid_gps
python -m tools.backend_sim --scenario all
```

The scenarios expect these receipt outcomes from the contract:

| Scenario | Expected receipt |
|---|---|
| `valid` | `RECEIVED_ACCEPTED`, `error: null`, followed by execution confirmation |
| `replay` | First accepted, identical second payload `RECEIVED_REJECTED`, `REPLAY` |
| `expired` | `RECEIVED_REJECTED`, `EXPIRED` |
| `invalid_signature` | `RECEIVED_REJECTED`, `BAD_MAC` |
| `unauthorized_action` | `RECEIVED_REJECTED`, `LOCAL_AUTH_REQUIRED` for `ENGINE_ON` without local auth |
| `decreasing_sequence` | Higher sequence accepted, lower sequence rejected with `REPLAY` |
| `kill_no_valid_gps` | `RECEIVED_REJECTED`, `KILL_MOTION_UNSAFE` |

Use a test device with a reliable clock for timestamp scenarios. `COMMAND_SEQUENCE`
must be greater than the device's last accepted command sequence; reset the test
device's replay state or choose a higher value before each independent run.
`all` uses consecutive values starting at `COMMAND_SEQUENCE` and is intended for
a fresh test device. The KILL scenario cannot control GPS: run it only when the
device has no valid recent GPS fix, as the expected firmware policy requires.
The `unauthorized_action` case likewise assumes no active local authentication.
Accepted STOP commands require an execution confirmation before the scenario
passes; rejected commands are checked against the receipt phase and error.

Telemetry is reported as `new` or `duplicate` by `(deviceId, outboxSeq)`; this
in-memory demonstration is reset when the simulator exits and is not a
persistent production backend.

## Pure-logic tests

No broker is needed:

```powershell
python -m unittest discover -s tools\backend_sim\tests -v
```
