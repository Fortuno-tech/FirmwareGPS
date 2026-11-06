#include <unity.h>

#include "app/system_controller/system_event_mailbox.hpp"

using fortico::app::SystemEventMailbox;
using fortico::constants::EventID;
using fortico::types::Event;

void setUp() {}
void tearDown() {}

void testControlEventsAreBufferedInFifoOrder()
{
	SystemEventMailbox mailbox;
	const Event events[] = {
		{EventID::SOS_BUTTON_PRESSED, 1U, 0U},
		{EventID::SOS_ACK_RECEIVED, 2U, 0U},
		{EventID::REMOTE_KILL_COMMAND, 3U, 0U},
	};

	for (const Event& event : events) {
		TEST_ASSERT_TRUE(mailbox.submit(event));
	}

	Event received{};
	for (const Event& expected : events) {
		TEST_ASSERT_TRUE(mailbox.receive(received));
		TEST_ASSERT_EQUAL_INT(static_cast<int>(expected.id),
							  static_cast<int>(received.id));
		TEST_ASSERT_EQUAL_UINT64(expected.timestamp, received.timestamp);
	}
	TEST_ASSERT_FALSE(mailbox.receive(received));
}

void testControlEventMailboxRejectsOverflow()
{
	SystemEventMailbox mailbox;
	for (std::size_t index = 0U; index < SystemEventMailbox::Capacity; ++index) {
		TEST_ASSERT_TRUE(mailbox.submit(
			{EventID::NETWORK_CONNECTED, index, 0U}));
	}
	TEST_ASSERT_FALSE(mailbox.submit(
		{EventID::SOS_BUTTON_PRESSED, 99U, 0U}));
}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testControlEventsAreBufferedInFifoOrder);
	RUN_TEST(testControlEventMailboxRejectsOverflow);
	return UNITY_END();
}
