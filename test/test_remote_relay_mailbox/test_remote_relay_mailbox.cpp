#include <unity.h>

#include "app/system_controller/remote_relay_command_mailbox.hpp"

using fortico::app::RemoteRelayCommandMailbox;
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

	RelayCommand received{};
	for (const RelayCommand& expected : commands) {
		TEST_ASSERT_TRUE(mailbox.receive(received));
		TEST_ASSERT_EQUAL_INT(static_cast<int>(expected.cmd),
							  static_cast<int>(received.cmd));
		TEST_ASSERT_EQUAL_UINT64(expected.timestamp, received.timestamp);
	}
	TEST_ASSERT_FALSE(mailbox.receive(received));
}

void testResetDiscardsQueuedCommands()
{
	RemoteRelayCommandMailbox mailbox;
	TEST_ASSERT_TRUE(mailbox.submit(
		{RelayCmd::EMERGENCY_CUT, RelaySource::REMOTE_COMMAND, 9U}));
	mailbox.reset();
	RelayCommand received{};
	TEST_ASSERT_FALSE(mailbox.receive(received));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testRemoteCommandsRemainFifoAndBounded);
	RUN_TEST(testResetDiscardsQueuedCommands);
	return UNITY_END();
}
