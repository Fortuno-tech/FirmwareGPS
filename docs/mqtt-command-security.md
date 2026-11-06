# MQTT command authentication

The device accepts commands only on its configured `fleet/<vehicle-id>/cmd`
topic. A command payload is a JSON object with these fields:

```json
{
  "id": "command-unique-id",
  "seq": 123,
  "command": "STOP",
  "timestamp": 1794000000,
  "mac": "64 hexadecimal characters"
}
```

An accepted `KILL` also clears local authentication and latches engine-on
rejection for the current boot, matching the controller's kill lock. A new
boot is still cut by the normal startup policy.

The command handler's secure constructor requires injection of the provisioned
security service, command-security guard, storage audit service, and local
authentication service. The compatibility constructor fails closed and rejects
commands with `SECURITY_NOT_CONFIGURED`; production startup must use the secure
constructor.

`id` is 1–63 ASCII letters, digits, `_`, or `-`; `seq` is a positive,
strictly increasing unsigned 64-bit number for that device. `command` must be
one of `STOP`, `ENGINE_OFF`, `KILL`, `ENGINE_ON`, `SOS_ACK`, or `GET_STATUS`.
`mac` is HMAC-SHA256 with that device's provisioned 32-byte root secret; the
firmware contains no production key. MAC input is UTF-8 bytes with no trailing
newline:

```text
FORTICO-CMD-V1\n<device-id>\n<id>\n<seq>\n<command>\n<timestamp-or-hyphen>
```

The final field is `-` when `timestamp` is omitted. When present, `timestamp`
is Unix seconds and is covered by the MAC. The
firmware enforces a 60-second absolute validity window only after network time
has synchronized the RTC during the current boot; in that state a timestamp
is mandatory. Without reliable time, sequence persistence and MAC validation
remain mandatory, while timestamp age is not evaluated.

The device persists the highest accepted sequence and a 16-ID sliding history
before performing an action. Repeated IDs within that window and any sequence
not greater than the persisted high-water mark are rejected, including after a
restart. Invalid signatures, expired timestamps, replayed sequences/IDs,
missing replay state, and policy rejections are audited locally when the SD
log is available and emitted through the firmware logger.

Action policy is enforced locally: remote `ENGINE_ON` requires an authorized
RFID badge to remain locally authenticated; `STOP` and `ENGINE_OFF` always
request a relay cut; `KILL` is accepted only with a valid, fresh GPS fix and
reported speed at or below 0.5 km/h.

New firmware no longer seeds a shared RFID UID. Badge records can be added or
removed only through the existing local serial provisioning window; the known
legacy shared UID is removed from the stored allowlist during storage startup.

Devices provisioned before this replay-state format have no persisted command
checkpoint and therefore reject remote commands closed until that state is
initialized through an approved provisioning/migration procedure. The MQTT
backend must implement this payload and canonical MAC format before sending
commands.
