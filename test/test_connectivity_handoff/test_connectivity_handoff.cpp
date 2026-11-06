#include <unity.h>

#include "app/connectivity_task/connectivity_request_mailbox.h"

using fortico::app::ConnectivityRequestMailbox;

namespace {

void testControlCyclesContinueWhileNetworkWorkerIsUnavailable()
{
	ConnectivityRequestMailbox mailbox;
	uint32_t relayUpdates = 0U;

	// No network, a hung modem, and a long reconnect all leave the worker
	// unavailable; repeated control cycles only set one bounded request bit.
	for (uint32_t cycle = 0U; cycle < 1000U; ++cycle) {
		mailbox.requestReconnect();
		++relayUpdates;
	}
	TEST_ASSERT_EQUAL_UINT32(1000U, relayUpdates);
	TEST_ASSERT_TRUE(mailbox.takeReconnectRequest());
	TEST_ASSERT_FALSE(mailbox.takeReconnectRequest());
}

void testReconnectRequestCanBeReissuedAfterLongAttempt()
{
	ConnectivityRequestMailbox mailbox;
	mailbox.requestReconnect();
	TEST_ASSERT_TRUE(mailbox.takeReconnectRequest());

	// A completed long reconnect attempt can be followed by one bounded request.
	mailbox.requestReconnect();
	TEST_ASSERT_TRUE(mailbox.takeReconnectRequest());
	TEST_ASSERT_FALSE(mailbox.takeReconnectRequest());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testControlCyclesContinueWhileNetworkWorkerIsUnavailable);
	RUN_TEST(testReconnectRequestCanBeReissuedAfterLongAttempt);
	return UNITY_END();
}
