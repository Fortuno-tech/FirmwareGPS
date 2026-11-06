#include <unity.h>

#include "app/system_controller/remote_relay_command_mailbox.hpp"

using fortico::app::RemoteRelayCommandMailbox;
using fortico::app::RemoteRelayCommand;
using fortico::types::RelayCmd;
using fortico::types::RelayCommand;
using fortico::types::RelaySource;

namespace {

void testRemoteCommandsRemainFifoAndBounded()
{
	RemoteRelayCommandMailbox mailbox;
	const RelayCommand commands[] = {
		{RelayCmd::CUT, RelaySource::REMOTE_COMMAND, 1U},
		{RelayCmd::EMERGENCY_CUT, RelaySource::REMOTE_COMMAND, 2U},
		{RelayCmd::ENGAGE, RelaySource::REMOTE_COMMAND, 3U},
		{RelayCmd::CUT, RelaySource::REMOTE_COMMAND, 4U},
	};

	TEST_ASSERT_TRUE(mailbox.ready());
	for (const RelayCommand& command : commands) {
		TEST_ASSERT_TRUE(mailbox.submit(command));
	}
	TEST_ASSERT_FALSE(mailbox.submit(
		{RelayCmd::ENGAGE, RelaySource::REMOTE_COMMAND, 5U}));

	RemoteRelayCommand received{};
	for (const RelayCommand& expected : commands) {
		TEST_ASSERT_TRUE(mailbox.receive(received));
		TEST_ASSERT_EQUAL_INT(static_cast<int>(expected.cmd),
							  static_cast<int>(received.relayCommand.cmd));
		TEST_ASSERT_EQUAL_UINT64(expected.timestamp,
								 received.relayCommand.timestamp);
	}
	TEST_ASSERT_FALSE(mailbox.receive(received));
}

void testResetDiscardsQueuedCommands()
{
	RemoteRelayCommandMailbox mailbox;
	TEST_ASSERT_TRUE(mailbox.submit(
		{RelayCmd::EMERGENCY_CUT, RelaySource::REMOTE_COMMAND, 9U}));
	mailbox.reset();
	RemoteRelayCommand received{};
	TEST_ASSERT_FALSE(mailbox.receive(received));
}

void testCorrelatedCommandKeepsIdAndCanBeConsumedOnlyOnce()
{
	RemoteRelayCommandMailbox mailbox;
	const RelayCommand relayCommand{
		RelayCmd::CUT, RelaySource::REMOTE_COMMAND, 12U};
	TEST_ASSERT_TRUE(mailbox.submit(relayCommand, "cmd-12", "STOP"));

	RemoteRelayCommand received{};
	TEST_ASSERT_TRUE(mailbox.receive(received));
	TEST_ASSERT_TRUE(received.hasCorrelation);
	TEST_ASSERT_EQUAL_STRING("cmd-12", received.commandId);
	TEST_ASSERT_EQUAL_STRING("STOP", received.commandName);
	TEST_ASSERT_FALSE(mailbox.receive(received));
}

void testCommandReceiptMonotonicTimeIsPreserved()
{
	RemoteRelayCommandMailbox mailbox;
	const RelayCommand relayCommand{
		RelayCmd::ENGAGE, RelaySource::REMOTE_COMMAND, 15U};
	TEST_ASSERT_TRUE(mailbox.submit(relayCommand, "cmd-15", "ENGINE_ON",
									0xFFFFFFF0U));

	RemoteRelayCommand received{};
	TEST_ASSERT_TRUE(mailbox.receive(received));
	TEST_ASSERT_EQUAL_UINT32(0xFFFFFFF0U,
							 received.receivedAtMonotonicMs);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testRemoteCommandsRemainFifoAndBounded);
	RUN_TEST(testResetDiscardsQueuedCommands);
	RUN_TEST(testCorrelatedCommandKeepsIdAndCanBeConsumedOnlyOnce);
	RUN_TEST(testCommandReceiptMonotonicTimeIsPreserved);
	return UNITY_END();
}
