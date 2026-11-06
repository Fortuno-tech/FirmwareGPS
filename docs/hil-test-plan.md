# Hardware-in-the-loop (HIL) test plan

## Safety and scope

- **Never burn eFuses.** Do not run any eFuse programming command; production
  build checks are compile-only. Use a dedicated, recoverable test device.
- **Do not connect a real engine, vehicle, or hazardous load.** Use an isolated,
  current-limited relay/load simulator. Verify the relay output is safe before
  powering the device and throughout every test.
- **Do not use real secret values in test scripts, logs, screenshots, tickets,
  or this report.** Provision only approved test credentials through a
  controlled fixture; redact serial output and broker traces.
- Use a test broker and test SIM/network where applicable. Coordinate remote
  commands with the test operator and ensure the relay/load simulator cannot
  actuate real equipment.
- Record the board/module revision, firmware build/profile, modem firmware,
  SD-card identity/capacity, fixture setup, test date, and operator with the
  test run. Preserve logs with secrets and personal data removed.

## Test setup

Use an approved, provisioned test unit with an isolated relay/load simulator,
SIM7600 modem, GNSS antenna or controlled GNSS simulator, RTC, RFID reader,
and SD card. Configure a non-production broker/test endpoint and test
certificates. Confirm access to the serial log and broker-side message capture.
Before each test, record the initial relay state, connectivity state, RTC
state, SD contents/capacity, and persisted command/outbox state as applicable.
Restore the baseline between cases.

For MQTT command tests, use the documented `fleet/<vehicle-id>/cmd` topic and
payload contract in [MQTT command authentication](mqtt-command-security.md).
Commands must have a valid test-device HMAC and increasing sequence unless the
case explicitly tests an invalid, replayed, or expired command. For outbox
tests, use the SD-backed `TelemetryOutbox` (`record_<sequence>.msg`) and its
FIFO replay behavior described in [Telemetry outbox](telemetry-outbox.md).

## Scenarios

### HIL-01 — Modem unplugged for 10 minutes

- **Preconditions / procedure:** Start from a stable, connected test baseline.
  Keep the ESP32 and relay/load simulator powered. Disconnect the SIM7600
  modem's approved test connection; do not short or hot-plug exposed power
  contacts. Observe operation for 10 minutes, then reconnect the modem and
  allow background recovery.
- **Expected result:** Relay/control operation remains responsive and safe
  while the modem is absent. Network/MQTT status reports disconnected; modem
  recovery is attempted in the background after reconnection without requiring
  a reboot or relay action.
- **Measurements to record:** Disconnect/reconnect timestamps; relay output
  and control-loop responsiveness; modem/network/MQTT status and relevant
  logs; recovery duration and successful broker subscription; resets,
  watchdog events, or lost queued telemetry.
- **Result:** ____________________

### HIL-02 — Long reconnect after service interruption

- **Preconditions / procedure:** Establish working modem, cellular data, and
  MQTT/TLS connectivity. Interrupt broker reachability or cellular service for
  at least 30 minutes while leaving the unit powered. Restore service without
  rebooting and observe reconnection and command subscription.
- **Expected result:** The device continues local control safely while
  offline, retries in the background, and reconnects/subscribes after service
  returns. Persisted outbox messages replay FIFO and are removed only after
  successful publication; any delivery duplicates are identified by
  `(deviceId, outboxSeq)`.
- **Measurements to record:** Outage and restoration times; connection
  transitions and attempt intervals; relay/control responsiveness; duration
  from restoration to MQTT subscription; outbox sequence range/order, broker
  acknowledgements, duplicates, and any dropped volatile handoff messages.
- **Result:** ____________________

### HIL-03 — Replayed and expired remote commands

- **Preconditions / procedure:** Connect to the test broker with a valid test
  device identity and reliable synchronized RTC. Send a valid, signed command
  with a fresh ID and sequence; capture its result. Resend the identical
  command, then send a separately signed command whose timestamp is more than
  60 seconds from device time, using an otherwise unused increasing sequence
  and ID. Use a harmless `GET_STATUS` command for the accepted baseline.
- **Expected result:** The first valid command is accepted. The repeated ID or
  non-increasing sequence is rejected as `REPLAY`; the out-of-window timestamp
  is rejected as `EXPIRED`. Rejected commands cause no requested action and
  are audited where SD logging is available; rejection ACKs/logs contain no
  secret material.
- **Measurements to record:** Device and broker timestamps; command IDs and
  sequence numbers (test values only); ACK result/error; audit/log entries;
  relay output; persisted replay high-water mark before and after each
  submission.
- **Result:** ____________________

### HIL-04 — Startup without a reliable clock

- **Preconditions / procedure:** On the test unit, arrange for RTC time to be
  unavailable/invalid at boot and prevent network time synchronization during
  initial startup. Keep the relay/load simulator connected. Capture startup
  telemetry/context and MQTT/TLS status. If MQTT becomes available after time
  synchronization, send a valid signed test command without a timestamp and
  one with a timestamp, using fresh IDs/sequences.
- **Expected result:** Firmware does not invent a valid UTC timestamp; MQTT
  payloads without available time use a null/absent timestamp as defined by
  their builder. A timestamp age check is applied only once the RTC is reliable
  for the current boot; without reliable time, MAC and persisted replay checks
  remain mandatory. TLS certificate validation is not bypassed to compensate
  for missing time; a handshake that cannot validate certificate time fails
  closed. Local relay safety remains independent of network/time availability.
- **Measurements to record:** RTC validity and sync time; serialized timestamp
  values; TLS result and diagnostic logs; command ACK/rejection outcomes;
  relay output and boot-to-control readiness time.
- **Result:** ____________________

### HIL-05 — KILL command without valid GPS

- **Preconditions / procedure:** Establish reliable time and MQTT connectivity.
  Use the isolated relay/load simulator and ensure GPS is invalid, absent, or
  older than `GPS_LAST_FIX_MAX_AGE_MS` (25 seconds). Send a correctly signed,
  fresh, increasing-sequence `KILL` command. Repeat, if useful, with a fresh
  fix reporting speed above 0.5 km/h, without simulating vehicle motion.
- **Expected result:** `KILL` is rejected (`KILL_MOTION_UNSAFE`) when there is
  no valid/fresh fix or speed is outside the safe threshold. The command must
  not issue an emergency relay cut or latch the KILL lock. Confirm the
  simulator remains in its safe state.
- **Measurements to record:** GPS fix-valid/last-fix age/speed; command
  timestamp/sequence; ACK and audit reason; relay output before and after;
  KILL latch/engine-on policy state.
- **Result:** ____________________

### HIL-06 — Network loss for 30 minutes

- **Preconditions / procedure:** Establish normal cellular, MQTT/TLS, and
  broker connectivity. Disable cellular service or isolate the test network
  for 30 minutes without removing modem power or restarting the ESP32. During
  the outage, continue local monitoring and allow telemetry to accumulate.
  Restore the network and observe reconnection and backlog replay.
- **Expected result:** Relay/control remains responsive and is not blocked by
  network operations. Connectivity is reported offline; telemetry is queued
  and persisted/replayed in order when capacity permits. After service returns,
  MQTT reconnects and persisted messages are removed only after publish
  success. Bounded RAM handoff is not treated as durable storage.
- **Measurements to record:** Outage duration; relay/control response times;
  network and MQTT status transitions; SD free space; queued/persisted
  sequence counts; replay order, publish results, duplicates, and any
  admission failures.
- **Result:** ____________________

### HIL-07 — Power loss during outbox write

- **Preconditions / procedure:** Use a disposable test SD card and maintain a
  known outbox baseline. Generate telemetry until the `MqttPublishTask` is
  writing an outbox record, then cut device power using the test fixture's
  controlled power switch. Do not remove the SD card while powered. Restore
  power and inspect outbox recovery, logs, sequence metadata, and broker replay.
- **Expected result:** Only a complete, validated record is promoted/replayed;
  an incomplete temporary record is not delivered as valid data. Previously
  committed records remain readable and FIFO ordered. A reserved sequence may
  leave a gap after interruption. Messages still only in the volatile handoff
  or in-progress RAM slot before persistence may be lost; record this limitation
  rather than treating RAM admission as durable acknowledgement.
- **Measurements to record:** Power-cut timing relative to write/flush/rename;
  pre/post SD file names and sequence numbers; recovery result; CRC/read errors;
  broker deliveries and ordering; number of lost, duplicated, or recovered
  records; relay state on reboot.
- **Result:** ____________________

### HIL-08 — SD card full

- **Preconditions / procedure:** Use a disposable test SD card containing a
  recoverable baseline. With broker access blocked, fill the card using
  non-sensitive test data until the outbox cannot persist another record.
  Generate additional telemetry, then restore broker access and observe
  recovery. Do not fill or alter a production card.
- **Expected result:** Outbox write/admission failure is reported; existing
  records are not automatically evicted or overwritten. The publisher does
  not acknowledge or discard a message it failed to persist, and backpressure
  eventually makes bounded new admissions fail explicitly. Local control and
  relay remain responsive. Once space is available, persisted records replay
  FIFO.
- **Measurements to record:** Free/used capacity at first failure; outbox and
  queue error logs; oldest/newest sequence and file integrity; telemetry
  admission results; relay/control responsiveness; replay results after space
  is restored.
- **Result:** ____________________

### HIL-09 — New RFID badge

- **Preconditions / procedure:** Use a test device with a known test
  allowlist and a test badge not currently enrolled. Verify the badge is denied
  and does not authorize `ENGINE_ON`. Enroll it only through the approved
  controlled local serial provisioning workflow on an already provisioned
  device: during its 5-second post-start serial window at 115200 baud, send
  `PROV:RFID:ADD:<test-UID>` with the test badge UID. Verify the `ADDED`
  response, then present the badge again. During a fresh post-start window,
  send `PROV:RFID:REMOVE:<test-UID>` and verify the `REMOVED` response; present
  the badge again to verify denial. Never add an automatic enrollment path.
- **Expected result:** An unknown badge is denied and cannot engage the relay.
  Only after explicit local provisioning is the new badge authorized; removal
  revokes authorization. No shared/default badge is accepted, and remote
  `ENGINE_ON` remains gated by active local authentication and the KILL lock.
- **Measurements to record:** Test badge identifier in redacted/approved form;
  provisioning operation result; allowlist state before/after; RFID
  authorization result; local-authentication state; relay output and
  `ENGINE_ON` ACK/audit outcome.
- **Result:** ____________________

### HIL-10 — Expired server certificate

- **Preconditions / procedure:** Configure a dedicated test broker endpoint
  that presents an expired server certificate chained to the test CA already
  provisioned on the SIM7600. Confirm the test device has a reliable clock.
  Attempt MQTT/TLS connection, then restore the normal valid-certificate test
  endpoint and retry. Do not replace a production CA or weaken TLS settings.
- **Expected result:** TLS/MQTT connection to the expired-certificate endpoint
  fails closed; no permissive time-validation fallback is used and no MQTT
  command channel is established. With the valid test endpoint restored, the
  device can connect normally.
- **Measurements to record:** Test certificate validity interval and test
  endpoint identity (no private key material); RTC time; TLS/MQTT status and
  SIM7600 diagnostics; connection/subscription outcome; recovery time after
  switching back to the valid endpoint.
- **Result:** ____________________

