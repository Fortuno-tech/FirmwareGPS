#pragma once

#include <cstddef>

#include "types/relay_types.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace fortico {
namespace app {

class RemoteRelayCommandMailbox final {
public:
	static constexpr std::size_t Capacity = 4U;

	RemoteRelayCommandMailbox()
#ifdef ESP32_TARGET
		: _queue(xQueueCreate(Capacity, sizeof(types::RelayCommand)))
#else
		: _commands{}, _head(0U), _tail(0U), _count(0U)
#endif
	{
	}

	static RemoteRelayCommandMailbox& instance()
	{
		static RemoteRelayCommandMailbox mailbox;
		return mailbox;
	}

	bool ready() const
	{
#ifdef ESP32_TARGET
		return _queue != nullptr;
#else
		return true;
#endif
	}

	bool submit(const types::RelayCommand& command)
	{
#ifdef ESP32_TARGET
		return _queue != nullptr &&
			   xQueueSend(_queue, &command, 0U) == pdTRUE;
#else
		if (_count == Capacity) return false;
		_commands[_tail] = command;
		_tail = (_tail + 1U) % Capacity;
		++_count;
		return true;
#endif
	}

	bool receive(types::RelayCommand& command)
	{
#ifdef ESP32_TARGET
		return _queue != nullptr &&
			   xQueueReceive(_queue, &command, 0U) == pdTRUE;
#else
		if (_count == 0U) return false;
		command = _commands[_head];
		_head = (_head + 1U) % Capacity;
		--_count;
		return true;
#endif
	}

	void reset()
	{
#ifdef ESP32_TARGET
		if (_queue != nullptr) (void)xQueueReset(_queue);
#else
		_head = 0U;
		_tail = 0U;
		_count = 0U;
#endif
	}

private:
#ifdef ESP32_TARGET
	QueueHandle_t _queue;
#else
	types::RelayCommand _commands[Capacity];
	std::size_t _head;
	std::size_t _tail;
	std::size_t _count;
#endif
};

}  // namespace app
}  // namespace fortico
