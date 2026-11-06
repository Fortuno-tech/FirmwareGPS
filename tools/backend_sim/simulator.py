from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import re
import ssl
import sys
import threading
import time
import uuid
from dataclasses import dataclass, field
from urllib.parse import urlsplit


UINT64_MAX = (1 << 64) - 1
COMMANDS = {"STOP", "ENGINE_OFF", "KILL", "ENGINE_ON", "SOS_ACK", "GET_STATUS"}
ID_PATTERN = re.compile(r"[A-Za-z0-9_-]{1,63}\Z", re.ASCII)
TOPICS = ("cmd/ack", "engine", "telemetry")


class ConfigurationError(ValueError):
    """Raised when simulator configuration is missing or invalid."""


def _required_env(*names: str) -> str:
    for name in names:
        value = os.environ.get(name)
        if value:
            return value
    raise ConfigurationError(f"Set one of these environment variables: {', '.join(names)}")


def _parse_bool(value: str, name: str) -> bool:
    normalized = value.strip().lower()
    if normalized in {"1", "true", "yes", "on"}:
        return True
    if normalized in {"0", "false", "no", "off"}:
        return False
    raise ConfigurationError(f"{name} must be true or false")


@dataclass(frozen=True)
class BrokerConfig:
    host: str
    port: int
    tls: bool
    username: str | None
    password: str | None = field(repr=False)
    ca_cert: str | None
    device_id: str
    vehicle_id: str
    hmac_key: bytes = field(repr=False)
    command_sequence: int = 1

    @classmethod
    def from_env(cls) -> "BrokerConfig":
        broker_url = os.environ.get("MQTT_BROKER_URL") or os.environ.get("MQTT_URL")
        url_host = ""
        url_port = None
        url_tls = False
        if broker_url:
            parsed = urlsplit(broker_url)
            if parsed.scheme not in {"mqtt", "mqtts"} or not parsed.hostname:
                raise ConfigurationError("MQTT_BROKER_URL must use mqtt:// or mqtts://")
            if parsed.path not in {"", "/"} or parsed.query or parsed.fragment:
                raise ConfigurationError("MQTT_BROKER_URL must not contain a path, query, or fragment")
            try:
                url_port = parsed.port
            except ValueError as exc:
                raise ConfigurationError("MQTT_BROKER_URL has an invalid port") from exc
            url_host = parsed.hostname
            url_tls = parsed.scheme == "mqtts"

        host = os.environ.get("MQTT_HOST") or url_host or "localhost"
        raw_tls = os.environ.get("MQTT_TLS")
        tls = _parse_bool(raw_tls, "MQTT_TLS") if raw_tls is not None else url_tls
        default_port = 8883 if tls else 1883
        raw_port = os.environ.get("MQTT_PORT")
        try:
            port = int(raw_port) if raw_port is not None else (url_port or default_port)
        except ValueError as exc:
            raise ConfigurationError("MQTT_PORT must be an integer") from exc
        if not 1 <= port <= 65535:
            raise ConfigurationError("MQTT port must be between 1 and 65535")

        username = os.environ.get("MQTT_USERNAME") or os.environ.get("MQTT_USER")
        password = os.environ.get("MQTT_PASSWORD")

        device_id = _required_env("DEVICE_ID")
        vehicle_id = _required_env("VEHICLE_ID")
        if not ID_PATTERN.fullmatch(device_id):
            raise ConfigurationError("DEVICE_ID must contain 1-63 ASCII letters, digits, '_' or '-'")
        if not ID_PATTERN.fullmatch(vehicle_id):
            raise ConfigurationError("VEHICLE_ID must contain 1-63 ASCII letters, digits, '_' or '-'")

        key_text = _required_env("HMAC_TEST_KEY")
        if not re.fullmatch(r"[0-9a-fA-F]{64}", key_text):
            raise ConfigurationError("HMAC_TEST_KEY must be 64 hexadecimal characters (32 bytes)")
        try:
            command_sequence = int(os.environ.get("COMMAND_SEQUENCE", "1"))
        except ValueError as exc:
            raise ConfigurationError("COMMAND_SEQUENCE must be a positive integer") from exc
        if not 1 <= command_sequence <= UINT64_MAX - 10:
            raise ConfigurationError("COMMAND_SEQUENCE is outside the usable uint64 range")

        return cls(
            host=host,
            port=port,
            tls=tls,
            username=username,
            password=password,
            ca_cert=os.environ.get("MQTT_TLS_CA_CERT") or None,
            device_id=device_id,
            vehicle_id=vehicle_id,
            hmac_key=bytes.fromhex(key_text),
            command_sequence=command_sequence,
        )


def canonical_command(
    device_id: str, command_id: str, sequence: int, command: str, timestamp: int | None
) -> bytes:
    if not ID_PATTERN.fullmatch(command_id):
        raise ValueError("Command id must contain 1-63 ASCII letters, digits, '_' or '-'")
    if not 1 <= sequence <= UINT64_MAX:
        raise ValueError("Command sequence must be a positive uint64")
    if command not in COMMANDS:
        raise ValueError(f"Unsupported command: {command}")
    if timestamp is not None and not 0 <= timestamp <= UINT64_MAX:
        raise ValueError("Command timestamp must be an unsigned uint64")
    timestamp_field = "-" if timestamp is None else str(timestamp)
    return (
        f"FORTICO-CMD-V1\n{device_id}\n{command_id}\n{sequence}\n"
        f"{command}\n{timestamp_field}"
    ).encode("ascii")


def build_command(
    key: bytes,
    device_id: str,
    command_id: str,
    sequence: int,
    command: str,
    timestamp: int | None,
) -> dict[str, object]:
    mac = hmac.new(
        key,
        canonical_command(device_id, command_id, sequence, command, timestamp),
        hashlib.sha256,
    ).hexdigest()
    payload: dict[str, object] = {
        "id": command_id,
        "seq": sequence,
        "command": command,
    }
    if timestamp is not None:
        payload["timestamp"] = timestamp
    payload["mac"] = mac
    return payload


def new_command_id() -> str:
    return f"sim-{uuid.uuid4().hex}"


@dataclass(frozen=True)
class ScenarioStep:
    payload: dict[str, object]
    expected_phase: str
    expected_error: str | None
    expect_engine_confirmation: bool = False


def _step(
    config: BrokerConfig,
    command: str,
    sequence: int,
    now: int,
    phase: str,
    error: str | None,
    *,
    command_id: str | None = None,
    timestamp: int | None = None,
    expect_engine: bool = False,
    corrupt_mac: bool = False,
) -> ScenarioStep:
    payload = build_command(
        config.hmac_key,
        config.device_id,
        command_id or new_command_id(),
        sequence,
        command,
        timestamp,
    )
    if corrupt_mac:
        mac = str(payload["mac"])
        payload["mac"] = ("0" if mac[0] != "0" else "1") + mac[1:]
    return ScenarioStep(payload, phase, error, expect_engine)


def build_scenario(
    name: str, config: BrokerConfig, now: int | None = None
) -> list[ScenarioStep]:
    current_time = int(time.time()) if now is None else now
    base = config.command_sequence

    def make(command: str, seq: int, phase: str, error: str | None, **kwargs: object) -> ScenarioStep:
        return _step(config, command, seq, current_time, phase, error, **kwargs)

    if name == "valid":
        return [make("STOP", base, "RECEIVED_ACCEPTED", None, timestamp=current_time, expect_engine=True)]
    if name == "replay":
        first = make("STOP", base, "RECEIVED_ACCEPTED", None, timestamp=current_time, expect_engine=True)
        return [
            first,
            ScenarioStep(first.payload, "RECEIVED_REJECTED", "REPLAY"),
        ]
    if name == "expired":
        return [make("STOP", base, "RECEIVED_REJECTED", "EXPIRED", timestamp=current_time - 100)]
    if name == "invalid_signature":
        return [
            make(
                "STOP",
                base,
                "RECEIVED_REJECTED",
                "BAD_MAC",
                timestamp=current_time,
                corrupt_mac=True,
            )
        ]
    if name == "unauthorized_action":
        return [
            make(
                "ENGINE_ON",
                base,
                "RECEIVED_REJECTED",
                "LOCAL_AUTH_REQUIRED",
                timestamp=current_time,
            )
        ]
    if name == "decreasing_sequence":
        first = make("STOP", base + 1, "RECEIVED_ACCEPTED", None, timestamp=current_time, expect_engine=True)
        lower = make("STOP", base, "RECEIVED_REJECTED", "REPLAY", timestamp=current_time)
        return [first, lower]
    if name == "kill_no_valid_gps":
        return [
            make(
                "KILL",
                base,
                "RECEIVED_REJECTED",
                "KILL_MOTION_UNSAFE",
                timestamp=current_time,
            )
        ]
    if name == "all":
        valid = make("STOP", base, "RECEIVED_ACCEPTED", None, timestamp=current_time, expect_engine=True)
        replay = make(
            "STOP", base + 1, "RECEIVED_ACCEPTED", None, timestamp=current_time, expect_engine=True
        )
        expired = make("STOP", base + 2, "RECEIVED_REJECTED", "EXPIRED", timestamp=current_time - 100)
        bad_mac = make(
            "STOP",
            base + 2,
            "RECEIVED_REJECTED",
            "BAD_MAC",
            timestamp=current_time,
            corrupt_mac=True,
        )
        unauthorized = make(
            "ENGINE_ON",
            base + 2,
            "RECEIVED_REJECTED",
            "LOCAL_AUTH_REQUIRED",
            timestamp=current_time,
        )
        decreasing_first = make(
            "STOP", base + 4, "RECEIVED_ACCEPTED", None, timestamp=current_time, expect_engine=True
        )
        decreasing_second = make(
            "STOP", base + 3, "RECEIVED_REJECTED", "REPLAY", timestamp=current_time
        )
        kill = make(
            "KILL",
            base + 5,
            "RECEIVED_REJECTED",
            "KILL_MOTION_UNSAFE",
            timestamp=current_time,
        )
        return [
            valid,
            replay,
            ScenarioStep(replay.payload, "RECEIVED_REJECTED", "REPLAY"),
            expired,
            bad_mac,
            unauthorized,
            decreasing_first,
            decreasing_second,
            kill,
        ]
    raise ValueError(f"Unknown scenario: {name}")


class OutboxDeduplicator:
    """Track delivery keys using the backend contract's (deviceId, outboxSeq)."""

    def __init__(self) -> None:
        self._seen: set[tuple[str, int]] = set()

    def accept(self, payload: object) -> tuple[bool, tuple[str, int]]:
        if not isinstance(payload, dict):
            raise ValueError("Telemetry payload must be a JSON object")
        device_id = payload.get("deviceId")
        sequence = payload.get("outboxSeq")
        if not isinstance(device_id, str) or not device_id:
            raise ValueError("Telemetry payload requires a non-empty deviceId")
        if isinstance(sequence, bool) or not isinstance(sequence, int) or sequence < 0:
            raise ValueError("Telemetry payload requires an integer outboxSeq")
        key = (device_id, sequence)
        if key in self._seen:
            return False, key
        self._seen.add(key)
        return True, key


def _topic(vehicle_id: str, channel: str) -> str:
    return f"fleet/{vehicle_id}/{channel}"


class MqttBackendSimulator:
    def __init__(self, config: BrokerConfig, timeout: float) -> None:
        try:
            import paho.mqtt.client as mqtt
        except ImportError as exc:
            raise ConfigurationError(
                "Missing dependency; install tools/backend_sim/requirements.txt"
            ) from exc

        self._mqtt = mqtt
        self._config = config
        self._timeout = timeout
        self._condition = threading.Condition()
        self._connected = threading.Event()
        self._subscribed = threading.Event()
        self._receipts: dict[str, list[dict[str, object]]] = {}
        self._confirmations: dict[str, list[dict[str, object]]] = {}
        self._deduplicator = OutboxDeduplicator()
        self._client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=f"backend-sim-{uuid.uuid4().hex[:12]}",
        )
        if config.username is not None or config.password is not None:
            self._client.username_pw_set(config.username, config.password)
        if config.tls:
            self._client.tls_set(
                ca_certs=config.ca_cert,
                cert_reqs=ssl.CERT_REQUIRED,
                tls_version=ssl.PROTOCOL_TLS_CLIENT,
            )
        self._client.on_connect = self._on_connect
        self._client.on_subscribe = self._on_subscribe
        self._client.on_message = self._on_message

    def _on_connect(self, client: object, userdata: object, flags: object, reason_code: object, properties: object) -> None:
        if getattr(reason_code, "is_failure", False):
            print(f"Broker connection rejected: {reason_code}", file=sys.stderr)
            self._connected.set()
            return
        self._connected.set()
        subscriptions = [(_topic(self._config.vehicle_id, channel), 1) for channel in TOPICS]
        client.subscribe(subscriptions)

    def _on_subscribe(self, client: object, userdata: object, mid: int, reason_code_list: list[object], properties: object) -> None:
        if any(getattr(reason, "is_failure", False) for reason in reason_code_list):
            print(f"Broker rejected a subscription: {reason_code_list}", file=sys.stderr)
        else:
            self._subscribed.set()

    def _on_message(self, client: object, userdata: object, message: object) -> None:
        try:
            payload = json.loads(message.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            print(f"Invalid JSON on {message.topic}: {exc}", file=sys.stderr)
            return
        if message.topic == _topic(self._config.vehicle_id, "cmd/ack"):
            if isinstance(payload, dict):
                command_id = payload.get("id")
                if isinstance(command_id, str):
                    with self._condition:
                        self._receipts.setdefault(command_id, []).append(payload)
                        self._condition.notify_all()
            print(f"ACK {json.dumps(payload, separators=(',', ':'))}")
        elif message.topic == _topic(self._config.vehicle_id, "engine"):
            if isinstance(payload, dict) and payload.get("event") == "command_execution":
                command_id = payload.get("id")
                if isinstance(command_id, str):
                    with self._condition:
                        self._confirmations.setdefault(command_id, []).append(payload)
                        self._condition.notify_all()
            print(f"ENGINE {json.dumps(payload, separators=(',', ':'))}")
        elif message.topic == _topic(self._config.vehicle_id, "telemetry"):
            try:
                is_new, key = self._deduplicator.accept(payload)
                state = "new" if is_new else "duplicate"
                print(f"TELEMETRY {state} key={key} {json.dumps(payload, separators=(',', ':'))}")
            except ValueError as exc:
                print(f"Invalid telemetry envelope: {exc}", file=sys.stderr)

    def connect(self) -> None:
        self._client.connect(self._config.host, self._config.port, keepalive=60)
        self._client.loop_start()
        if not self._connected.wait(self._timeout):
            raise TimeoutError("Timed out connecting to the MQTT broker")
        if not self._subscribed.wait(self._timeout):
            raise TimeoutError("Timed out subscribing to ACK, engine, and telemetry topics")

    def close(self) -> None:
        self._client.disconnect()
        self._client.loop_stop()

    def _wait_for(self, events: dict[str, list[dict[str, object]]], command_id: str) -> dict[str, object]:
        deadline = time.monotonic() + self._timeout
        with self._condition:
            while not events.get(command_id):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(f"Timed out waiting for command event {command_id}")
                self._condition.wait(remaining)
            return events[command_id].pop(0)

    def publish_and_check(self, step: ScenarioStep) -> None:
        payload = step.payload
        command_id = str(payload["id"])
        encoded = json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        info = self._client.publish(
            _topic(self._config.vehicle_id, "cmd"),
            payload=encoded,
            qos=1,
            retain=False,
        )
        if info.rc != self._mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT publish failed with code {info.rc}")
        info.wait_for_publish(timeout=self._timeout)
        if not info.is_published():
            raise TimeoutError("Timed out waiting for MQTT QoS 1 publish acknowledgement")

        receipt = self._wait_for(self._receipts, command_id)
        phase = receipt.get("phase")
        error = receipt.get("error")
        if phase != step.expected_phase or error != step.expected_error:
            raise AssertionError(
                f"Command {command_id}: expected phase={step.expected_phase} "
                f"error={step.expected_error!r}, received phase={phase!r} error={error!r}"
            )
        expected_success = step.expected_phase == "RECEIVED_ACCEPTED"
        expected_fields = {
            "id": command_id,
            "deviceId": self._config.device_id,
            "vehicleId": self._config.vehicle_id,
            "command": payload["command"],
            "success": expected_success,
        }
        for field_name, expected_value in expected_fields.items():
            if receipt.get(field_name) != expected_value:
                raise AssertionError(
                    f"Command {command_id}: receipt {field_name} was "
                    f"{receipt.get(field_name)!r}, expected {expected_value!r}"
                )
        if not isinstance(receipt.get("timestamp"), str) or not isinstance(receipt.get("firmware"), str):
            raise AssertionError(f"Command {command_id}: receipt is missing timestamp or firmware")
        print(f"PASS command={command_id} phase={phase} error={error}")
        if step.expect_engine_confirmation:
            confirmation = self._wait_for(self._confirmations, command_id)
            confirmation_fields = {
                "event": "command_execution",
                "id": command_id,
                "deviceId": self._config.device_id,
                "vehicleId": self._config.vehicle_id,
                "command": payload["command"],
                "success": True,
            }
            for field_name, expected_value in confirmation_fields.items():
                if confirmation.get(field_name) != expected_value:
                    raise AssertionError(
                        f"Command {command_id}: execution {field_name} was "
                        f"{confirmation.get(field_name)!r}, expected {expected_value!r}"
                    )
            if confirmation.get("error") is not None:
                raise AssertionError(f"Command {command_id}: successful execution has a non-null error")
            if not isinstance(confirmation.get("timestamp"), str) or not isinstance(
                confirmation.get("firmware"), str
            ) or not isinstance(confirmation.get("relay"), str):
                raise AssertionError(f"Command {command_id}: execution confirmation is incomplete")
            print(f"CONFIRMED command={command_id} success=true")


SCENARIOS = (
    "valid",
    "replay",
    "expired",
    "invalid_signature",
    "unauthorized_action",
    "decreasing_sequence",
    "kill_no_valid_gps",
    "all",
)


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Publish signed FirmwareGPS test commands over MQTT.")
    parser.add_argument("--scenario", choices=SCENARIOS, default="valid")
    parser.add_argument("--timeout", type=float, default=30.0, help="seconds to wait for each broker/device response")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    if args.timeout <= 0:
        print("--timeout must be positive", file=sys.stderr)
        return 2
    try:
        config = BrokerConfig.from_env()
        simulator = MqttBackendSimulator(config, args.timeout)
        try:
            print(
                f"Connecting to {config.host}:{config.port} tls={config.tls}; "
                f"vehicle={config.vehicle_id} device={config.device_id}"
            )
            simulator.connect()
            steps = build_scenario(args.scenario, config)
            for step in steps:
                simulator.publish_and_check(step)
        finally:
            simulator.close()
        return 0
    except (ConfigurationError, OSError, RuntimeError, TimeoutError, AssertionError, ValueError) as exc:
        print(f"Simulator failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
