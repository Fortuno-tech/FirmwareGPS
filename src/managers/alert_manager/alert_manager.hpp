#pragma once

#include <cstdint>

#include "interfaces/imanager.h"
#include "interfaces/ievent_listener.h"
#include "interfaces/isubscriber.h"
#include "types/alert_types.h"
#include "drivers/button/button_driver.h"
#include "drivers/buzzer/buzzer_driver.h"
#include "drivers/led/led_driver.h"

namespace fortico {
namespace managers {

class AlertManager : public interfaces::IManager,
					  public interfaces::IEventListener {
public:
	AlertManager(drivers::ButtonDriver& button, drivers::BuzzerDriver& buzzer,
				 drivers::LedDriver& led);

	void init() override;
	void update() override;
	void process() override;
	void reset() override;
	void onEvent(const types::Event& event) override;
	void subscribeTo(interfaces::ISubscriber& eventBus);

	bool raise(types::AlertCode code, types::AlertSeverity severity,
			   uint64_t timestamp = 0U);
	bool hasPendingAlert() const;
	types::AlertEvent pendingAlert() const;
	void clear();

private:
	drivers::ButtonDriver& _button;
	drivers::BuzzerDriver& _buzzer;
	drivers::LedDriver& _led;
	bool _initialized;
	bool _alertActive;
	bool _eventPending;
	types::Event _pendingEvent;
	types::AlertEvent _pending;
};

}  // namespace managers
}  // namespace fortico
