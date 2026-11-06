import hashlib
import hmac
import os
import secrets
import unittest
from unittest.mock import patch

from tools.backend_sim.simulator import (
    BrokerConfig,
    ConfigurationError,
    OutboxDeduplicator,
    build_command,
    build_scenario,
    canonical_command,
)


TEST_KEY = secrets.token_bytes(32)


def config(sequence: int = 1) -> BrokerConfig:
    return BrokerConfig(
        host="localhost",
        port=1883,
        tls=False,
        username=None,
        password=None,
        ca_cert=None,
        device_id="DEVICE-1",
        vehicle_id="VEH001",
        hmac_key=TEST_KEY,
        command_sequence=sequence,
    )


class CommandSigningTests(unittest.TestCase):
    def test_canonical_input_matches_contract_order_and_lf_separators(self) -> None:
        self.assertEqual(
            canonical_command("DEVICE-1", "cmd-valid-001", 1, "STOP", 1000),
            b"FORTICO-CMD-V1\nDEVICE-1\ncmd-valid-001\n1\nSTOP\n1000",
        )

    def test_absent_timestamp_uses_hyphen(self) -> None:
        self.assertEqual(
            canonical_command("DEVICE-1", "cmd-1", 1, "GET_STATUS", None),
            b"FORTICO-CMD-V1\nDEVICE-1\ncmd-1\n1\nGET_STATUS\n-",
        )

    def test_signed_json_has_only_contract_fields_and_lowercase_hmac(self) -> None:
        payload = build_command(TEST_KEY, "DEVICE-1", "cmd-valid-001", 1, "STOP", 1000)
        expected = hmac.new(
            TEST_KEY,
            b"FORTICO-CMD-V1\nDEVICE-1\ncmd-valid-001\n1\nSTOP\n1000",
            hashlib.sha256,
        ).hexdigest()
        self.assertEqual(
            list(payload),
            ["id", "seq", "command", "timestamp", "mac"],
        )
        self.assertEqual(payload["mac"], expected)

    def test_invalid_sequence_and_command_are_rejected(self) -> None:
        with self.assertRaises(ValueError):
            canonical_command("DEVICE-1", "cmd-1", 0, "STOP", 1000)
        with self.assertRaises(ValueError):
            canonical_command("DEVICE-1", "cmd-1", 1, "UNAUTHORIZED", 1000)


class ScenarioTests(unittest.TestCase):
    def test_replay_reuses_exact_payload(self) -> None:
        steps = build_scenario("replay", config(), now=1000)
        self.assertEqual(steps[0].payload, steps[1].payload)
        self.assertEqual(steps[1].expected_error, "REPLAY")

    def test_expired_scenario_is_signed_with_old_timestamp(self) -> None:
        step = build_scenario("expired", config(), now=1000)[0]
        self.assertEqual(step.expected_error, "EXPIRED")
        self.assertEqual(step.payload["timestamp"], 900)
        self.assertEqual(
            step.payload["mac"],
            hmac.new(
                TEST_KEY,
                b"FORTICO-CMD-V1\nDEVICE-1\n"
                + str(step.payload["id"]).encode("ascii")
                + b"\n1\nSTOP\n900",
                hashlib.sha256,
            ).hexdigest(),
        )

    def test_invalid_signature_changes_a_valid_mac(self) -> None:
        step = build_scenario("invalid_signature", config(), now=1000)[0]
        canonical = canonical_command(
            "DEVICE-1", str(step.payload["id"]), 1, "STOP", 1000
        )
        valid_mac = hmac.new(TEST_KEY, canonical, hashlib.sha256).hexdigest()
        self.assertNotEqual(step.payload["mac"], valid_mac)
        self.assertEqual(step.expected_error, "BAD_MAC")

    def test_unauthorized_kill_and_decreasing_scenarios_match_contract(self) -> None:
        unauthorized = build_scenario("unauthorized_action", config(), now=1000)[0]
        self.assertEqual(unauthorized.payload["command"], "ENGINE_ON")
        self.assertEqual(unauthorized.expected_error, "LOCAL_AUTH_REQUIRED")

        decreasing = build_scenario("decreasing_sequence", config(10), now=1000)
        self.assertEqual(decreasing[0].payload["seq"], 11)
        self.assertEqual(decreasing[1].payload["seq"], 10)
        self.assertEqual(decreasing[1].expected_error, "REPLAY")

        kill = build_scenario("kill_no_valid_gps", config(), now=1000)[0]
        self.assertEqual(kill.payload["command"], "KILL")
        self.assertEqual(kill.expected_error, "KILL_MOTION_UNSAFE")

    def test_all_scenario_sequence_order_preserves_decreasing_sequence(self) -> None:
        steps = build_scenario("all", config(20), now=1000)
        self.assertEqual(steps[0].payload["seq"], 20)
        self.assertEqual(steps[1].payload["seq"], 21)
        self.assertEqual(steps[2].payload, steps[1].payload)
        self.assertEqual(steps[6].payload["seq"], 24)
        self.assertEqual(steps[7].payload["seq"], 23)
        self.assertEqual(steps[8].payload["seq"], 25)


class OutboxDeduplicationTests(unittest.TestCase):
    def test_deduplicates_by_device_and_outbox_sequence_pair(self) -> None:
        deduplicator = OutboxDeduplicator()
        self.assertEqual(deduplicator.accept({"deviceId": "A", "outboxSeq": 7}), (True, ("A", 7)))
        self.assertEqual(deduplicator.accept({"deviceId": "A", "outboxSeq": 7}), (False, ("A", 7)))
        self.assertEqual(deduplicator.accept({"deviceId": "B", "outboxSeq": 7}), (True, ("B", 7)))
        self.assertEqual(deduplicator.accept({"deviceId": "A", "outboxSeq": 8}), (True, ("A", 8)))

    def test_rejects_malformed_telemetry_keys(self) -> None:
        deduplicator = OutboxDeduplicator()
        for payload in ({}, {"deviceId": "A", "outboxSeq": True}, {"deviceId": "", "outboxSeq": 1}):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                deduplicator.accept(payload)


class EnvironmentConfigTests(unittest.TestCase):
    def test_reads_mqtts_url_and_authentication_from_environment(self) -> None:
        env = {
            "MQTT_BROKER_URL": "mqtts://broker.example:8883",
            "MQTT_USERNAME": "test-" + secrets.token_hex(4),
            "MQTT_PASSWORD": secrets.token_hex(16),
            "DEVICE_ID": "DEVICE-1",
            "VEHICLE_ID": "VEH001",
            "HMAC_TEST_KEY": TEST_KEY.hex(),
            "COMMAND_SEQUENCE": "12",
        }
        with patch.dict(os.environ, env, clear=True):
            result = BrokerConfig.from_env()
        self.assertEqual((result.host, result.port, result.tls), ("broker.example", 8883, True))
        self.assertEqual((result.username, result.password), (env["MQTT_USERNAME"], env["MQTT_PASSWORD"]))
        self.assertEqual(result.hmac_key, TEST_KEY)
        self.assertEqual(result.command_sequence, 12)

    def test_tls_flag_selects_secure_default_port(self) -> None:
        env = {
            "MQTT_HOST": "broker.example",
            "MQTT_TLS": "true",
            "DEVICE_ID": "DEVICE-1",
            "VEHICLE_ID": "VEH001",
            "HMAC_TEST_KEY": TEST_KEY.hex(),
        }
        with patch.dict(os.environ, env, clear=True):
            result = BrokerConfig.from_env()
        self.assertEqual((result.port, result.tls), (8883, True))

    def test_requires_a_32_byte_hex_key(self) -> None:
        env = {
            "MQTT_HOST": "localhost",
            "DEVICE_ID": "DEVICE-1",
            "VEHICLE_ID": "VEH001",
            "HMAC_TEST_KEY": "not-a-key",
        }
        with patch.dict(os.environ, env, clear=True), self.assertRaises(ConfigurationError):
            BrokerConfig.from_env()


if __name__ == "__main__":
    unittest.main()
