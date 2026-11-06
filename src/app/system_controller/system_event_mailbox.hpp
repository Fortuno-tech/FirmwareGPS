#pragma once

#include <cstddef>

#include "types/system_types.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace fortico {
namespace app {

class SystemEventMailbox final {
public:
	static constexpr std::size_t Capacity = 20U;

	SystemEventMailbox()
#ifdef ESP32_TARGET
		: _queue(xQueueCreate(Capacity, sizeof(types::Event)))
#else
		: _events{}, _head(0U), _tail(0U), _count(0U)
#endif
	{
	}

	static SystemEventMailbox& instance()
	{
		static SystemEventMailbox mailbox;
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

	bool submit(const types::Event& event)
	{
#ifdef ESP32_TARGET
		return _queue != nullptr && xQueueSend(_queue, &event, 0U) == pdTRUE;
#else
		if (_count == Capacity) return false;
		_events[_tail] = event;
		_tail = (_tail + 1U) % Capacity;
		++_count;
		return true;
#endif
	}

	bool receive(types::Event& event)
	{
#ifdef ESP32_TARGET
		return _queue != nullptr && xQueueReceive(_queue, &event, 0U) == pdTRUE;
#else
		if (_count == 0U) return false;
		event = _events[_head];
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
	types::Event _events[Capacity];
	std::size_t _head;
	std::size_t _tail;
	std::size_t _count;
#endif
};

}  // namespace app
}  // namespace fortico
