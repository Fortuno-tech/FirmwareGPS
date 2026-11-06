# Connectivity and control-task isolation

## Runtime boundaries

`SchedulerTask` runs the 100 ms `Application::scheduledUpdate` callback on
`CORE_APP_CRITICAL` (core 1, priority 5). It owns the control-cycle call into
`SystemController`; that path uses connectivity status and queues relay work,
but does not perform modem registration, PDP setup, MQTT connection, or GNSS
polling. `ConnectivityTask` runs on `CORE_NETWORK` (core 0, priority 4) and
performs modem initialization, network retries, GNSS polling, RTC time sync,
and provisioning requests. Its caller-facing provisioning waits and shutdown
wait have finite timeouts.

The cellular MQTT client is also used by `MqttPublishTask` (core 0, priority
4), so ConnectivityTask is not the only network-task caller of the modem.
Both it and the MQTT task use the same `Sim7600Driver`; the driver's recursive
transaction mutex protects AT command exchanges, and the MQTT client's outer
transaction guard serializes its multi-command MQTT operations. The modem
mutex acquisition is bounded by `SIM7600_UART_MUTEX_TIMEOUT_MS` (50 ms);
nonblocking incoming-message polling requests a zero-timeout lock. GNSS uses
the same driver and hence the same transaction mutex. Certificate transfer
holds the recursive mutex across the prompt, raw UART payload, and modem
confirmation. These are software-level serialization properties, not proof
that a physical modem/UART is electrically or temporally reliable.

## Shared I2C bus and lock order

RTC and PN532 share `I2cHal`'s recursive mutex. RTC driver operations take it
with `I2C_MUTEX_TIMEOUT_MS` before using the HAL; HAL transactions also take
the same recursive mutex. PN532 setup/read transactions hold that mutex while
using the Adafruit driver directly, and PN532 availability probes now acquire
it for each Wire transaction. All mutex waits are finite. The current
connectivity code obtains network time over UART and releases that modem
transaction before writing the RTC over I2C. RFID/RTC access therefore
serializes on I2C, while the inspected paths do not nest the UART and I2C
mutexes. This describes the source-level acquisition paths only; it is not a
hardware lock-order or bus-integrity guarantee.

## Watchdog and wait budgets

The configured watchdog remains 8 seconds. `SchedulerTask` and
`ConnectivityTask` are registered with the hardware watchdog before their
startup gates open, and each feeds from its own task context. `MqttPublishTask`
also registers before opening its startup gate. The SIM7600
driver additionally feeds the calling task at safe points at least every
500 ms while polling bounded AT-command, raw-write, network-registration, or
certificate-confirmation waits. This also covers long (up to 60 second) MQTT
modem responses without increasing the global watchdog timeout. Connectivity
feeds before and after the modem power-on sequence; its current configured
delays total 6.6 seconds.

Network/task wait budgets in this code are: modem transaction lock 50 ms;
provisioning mutex 1 second; provisioning completion 60 seconds; connectivity
shutdown 5 seconds; task startup gate 1 second; worker notification/poll
interval 250 ms; and finite modem/MQTT command timeouts at each call site.
The connectivity retry interval is 30 seconds, but it is a scheduled retry
period, not a blocking wait.

There are no remaining actual network-path waits configured with
`portMAX_DELAY`. The generic `Sim7600Driver::lockTransaction` and `I2cHal::lock`
APIs map a `portMAX_DELAY` argument back to their respective configured finite
mutex timeout. Other loops in network code are bounded by command/data sizes or
timeouts; the MQTT publish worker is intentionally long-lived and has bounded
delays when idle. EventBus subscription-lock acquisition, which can be reached
when dispatching network events, is bounded to 100 ms. A timeout there returns
without changing the subscription table or dispatching that event.

One unbounded boot hold remains outside network I/O: while the device is
unprovisioned, `main` remains in its provisioning/HALT loop with a 100 ms
delay, serial provisioning checks, and the relay held cut. It intentionally
has no deadline so missing credentials cannot start the engine; it does not
wait on ConnectivityTask, UART, MQTT, or a mutex. Persistent scheduler,
EventBus, ConnectivityTask, and MQTT task loops also have no fixed lifetime,
but their blocking queue/notification waits and idle delays are bounded.

## Optional timing instrumentation

Defining `FORTICO_ENABLE_CONTROL_TIMING=1` enables periodic reporting of the
maximum observed whole scheduler callback duration and maximum completed-MQTT-
message-to-relay-driver processing latency. The latter starts when the SIM7600
parser completes an incoming MQTT message and ends after the relay manager's
processing attempt; it excludes earlier modem buffering and is not a
measurement of GPIO electrical transition or vehicle response. The flag is
absent from default build flags, so instrumentation is off by default.
`ControlTimingMetrics` and network
timeout arithmetic have native software tests; no hardware timing is inferred
from them.

## Evidence and limits

This note records code-level behavior from the task creation, modem, I2C,
RTC, RFID, and MQTT call paths. Native tests and ESP32 builds validate
software compilation/logic only. They do not prove scheduling latency,
watchdog behavior on a board, UART/I2C electrical behavior, mutex fairness,
modem response bounds, relay actuation, or absence of hardware races under
real concurrent load.
