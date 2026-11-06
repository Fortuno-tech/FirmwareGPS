#include <unity.h>

#include "app/system_controller/remote_command_execution.hpp"

using fortico::app::RemoteCommandExecution;
using fortico::app::RemoteCommandExecutionResult;
using fortico::types::RelayState;

void setUp() {}
void tearDown() {}

void testExecutionConfirmationFollowsRelayProcessing()
{
	RemoteCommandExecution execution;
	TEST_ASSERT_TRUE(execution.begin(
		"cmd-1", "ENGINE_ON", RelayState::ENGAGED, true, nullptr));
	RemoteCommandExecutionResult result{};
	TEST_ASSERT_FALSE(execution.result(result));

	TEST_ASSERT_TRUE(execution.complete(true, RelayState::ENGAGED));
	TEST_ASSERT_TRUE(execution.result(result));
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL_STRING("cmd-1", result.commandId);
	TEST_ASSERT_EQUAL_STRING("ENGINE_ON", result.commandName);
}

void testRelayFailureProducesExecutionFailure()
{
	RemoteCommandExecution execution;
	TEST_ASSERT_TRUE(execution.begin(
		"cmd-2", "STOP", RelayState::LOCKED, true, nullptr));
	TEST_ASSERT_TRUE(execution.complete(false, RelayState::ENGAGED));

	RemoteCommandExecutionResult result{};
	TEST_ASSERT_TRUE(execution.result(result));
	TEST_ASSERT_FALSE(result.success);
	TEST_ASSERT_EQUAL_STRING("RELAY_ACTION_FAILED", result.error);
}

void testUnexpectedFinalRelayStateProducesExecutionFailure()
{
	RemoteCommandExecution execution;
	TEST_ASSERT_TRUE(execution.begin(
		"cmd-2b", "ENGINE_ON", RelayState::ENGAGED, true, nullptr));
	TEST_ASSERT_TRUE(execution.complete(true, RelayState::LOCKED));

	RemoteCommandExecutionResult result{};
	TEST_ASSERT_TRUE(execution.result(result));
	TEST_ASSERT_FALSE(result.success);
	TEST_ASSERT_EQUAL_STRING("RELAY_STATE_MISMATCH", result.error);
}

void testExecutionConfirmationDoesNotDependOnReceiptAckDelivery()
{
	RemoteCommandExecution execution;
	TEST_ASSERT_TRUE(execution.begin(
		"cmd-3", "KILL", RelayState::EMERGENCY_CUT, true, nullptr));
	// The receipt ACK can be lost; the controller result remains independent.
	TEST_ASSERT_TRUE(execution.complete(true, RelayState::EMERGENCY_CUT));
	RemoteCommandExecutionResult result{};
	TEST_ASSERT_TRUE(execution.result(result));
	TEST_ASSERT_TRUE(result.success);
}

void testOneAcceptedCommandProducesOnlyOneExecutionResult()
{
	RemoteCommandExecution execution;
	TEST_ASSERT_TRUE(execution.begin(
		"cmd-4", "ENGINE_OFF", RelayState::LOCKED, true, nullptr));
	TEST_ASSERT_TRUE(execution.complete(true, RelayState::LOCKED));
	TEST_ASSERT_FALSE(execution.complete(true, RelayState::LOCKED));

	RemoteCommandExecutionResult result{};
	TEST_ASSERT_TRUE(execution.result(result));
	execution.markPublished();
	TEST_ASSERT_FALSE(execution.result(result));
}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testExecutionConfirmationFollowsRelayProcessing);
	RUN_TEST(testRelayFailureProducesExecutionFailure);
	RUN_TEST(testUnexpectedFinalRelayStateProducesExecutionFailure);
	RUN_TEST(testExecutionConfirmationDoesNotDependOnReceiptAckDelivery);
	RUN_TEST(testOneAcceptedCommandProducesOnlyOneExecutionResult);
	return UNITY_END();
}
