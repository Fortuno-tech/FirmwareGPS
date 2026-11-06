#pragma once

#include <atomic>
#include <cstdint>

namespace fortico {
namespace app {

class ConnectivityRequestMailbox {
public:
	void requestReconnect()
	{
		_pending.fetch_or(kReconnectRequested, std::memory_order_release);
	}

	bool takeReconnectRequest()
	{
		return (_pending.exchange(0U, std::memory_order_acquire) &
				kReconnectRequested) != 0U;
	}

private:
	static constexpr uint8_t kReconnectRequested = 1U;
	std::atomic<uint8_t> _pending{0U};
};

}  // namespace app
}  // namespace fortico
