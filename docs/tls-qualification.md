# SIM7600 TLS bench qualification

## Purpose and safety limits

This procedure checks that the SIM7600 refuses MQTT/TLS connections when
certificate validation or time validation should fail. It is a bench test, not
a production deployment procedure.

**Never run any eFuse programming command or procedure. In particular, do not
run `burn_efuse`, enable Secure Boot/Flash Encryption eFuses, or use a
production-profile image for this test.** The development profile does not
program eFuses.

- Use a dedicated development ESP32/SIM7600 assembly, with relay outputs
  disconnected from vehicle or other hazardous loads. Keep actuators disabled.
- Use an isolated, non-production network and an approved non-production MQTT
  test endpoint. Restrict its access to the bench. Never direct a test build at
  the production broker.
- Do not create, copy into this repository, print, or attach credentials,
  private keys, CA certificates, or server certificates. Obtain any test
  endpoint and device provisioning already approved for the lab from its
  controlled owner. Do not use production credentials.
- The repository configuration currently targets the HiveMQ Cloud endpoint on
  port `8883` and expects the SIM7600 CA file named `hivemq-ca.pem`. It enables
  TLS 1.2, server-certificate authentication (`authmode=1`), SNI, and
  `ignorelocaltime=0`. There is no documented safe test-broker override here:
  use only an already-approved bench build/configuration. If one is not
  available, stop rather than editing the production endpoint or using
  production credentials.
- Record only sanitized logs and metadata (board/modem model and firmware,
  test build ID, endpoint label, test date, and outcome). Redact credentials,
  payloads, and certificate contents.

## Setup and baseline

1. Confirm the bench isolation, disabled actuator wiring, approved test
   endpoint, and test-only device provisioning. Confirm the endpoint owner has
   made the required pre-existing test cases available. This procedure does
   not generate or install certificate material.
2. Build the non-production firmware using the team's approved bench
   configuration. Verify that TLS configuration remains strict:
   `authmode=1`, `enableSNI=1`, and `ignorelocaltime=0`. Do not bypass
   certificate, hostname, or clock checks to make the test pass.
3. Power the board and modem from a current-limited bench supply. Capture
   sanitized startup logs. Read the SIM7600 clock with the read-only
   `AT+CCLK?` command and inspect certificate filenames, if needed, with
   `AT+CCERTLIST`; do not dump certificate contents.
4. With the approved endpoint's normal valid chain, matching hostname, and a
   verified current modem clock, establish a baseline. MQTT/TLS should connect
   (the firmware expects `+CMQTTCONNECT: 0,0`). Confirm the device remains
   unable to actuate anything and that no test command or publication is sent.
   Disconnect before changing to each fault case.
5. Test one fault at a time. Use only endpoint variants already maintained by
   the approved test service. For the clock case, use the modem vendor's
   documented, reversible bench method to supply an invalid/unsynchronized
   clock, and verify it using `AT+CCLK?`. Do not guess clock-setting commands.
   Prevent automatic time synchronization from correcting the test clock
   before the TLS attempt; if that cannot be controlled safely, mark the case
   not run and escalate to the lab owner.
6. After each attempt, record the result below, disconnect, restore the normal
   endpoint and reliable time source, and verify the modem clock is current.
   Stop if any fault case connects successfully. Preserve sanitized failure
   diagnostics for investigation; do not weaken TLS settings or repeat against
   a production service.

## Cases and expected outcomes

For each row, pass means the MQTT/TLS connection is refused before a successful
MQTT connection indication (`+CMQTTCONNECT: 0,0`). Exact modem error text can
vary by SIM7600 firmware; capture the sanitized response and modem firmware
version. A TLS failure is not a pass if it is caused by unrelated network,
DNS, SIM, or broker availability problems.

| Case | Bench condition (change only this variable) | Expected outcome | Result |
|---|---|---|---|
| Expired certificate | Normal hostname and trusted CA; approved test endpoint presents its pre-existing expired server certificate. | TLS handshake is rejected for certificate validity; no MQTT connection is established. | |
| Wrong hostname | Current time and trusted CA; connect using a hostname that does not match the endpoint's pre-existing server certificate names. Keep SNI enabled. | TLS handshake is rejected for hostname mismatch; no MQTT connection is established. A successful connection is a qualification failure and release blocker. | |
| Wrong CA | Current time and matching hostname; approved endpoint presents a valid chain rooted in a CA not trusted by the modem's configured trust anchor. | TLS handshake is rejected for untrusted CA; no MQTT connection is established. | |
| Invalid / unsynchronized clock | Normal hostname and trusted CA; verified SIM7600 clock is invalid, unset, or deliberately unsynchronized for this attempt. Keep `ignorelocaltime=0`. | TLS handshake is rejected because certificate time cannot be validated; no permissive fallback or MQTT connection is allowed. | |

## Qualification record

Record the test build/profile, board and SIM7600 firmware revisions, endpoint
labels (not credentials or certificate names/content), sanitized modem result,
operator, and date alongside this document in the approved test system. Mark
any case that could not be isolated or observed as **not run**, not pass.
Hostname validation is a release blocker until the wrong-hostname case is
observed to fail on the target SIM7600 firmware.
