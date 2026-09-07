#pragma once

#include <cstddef>

#include "interfaces/iservice.h"
#include "managers/telemetry_manager/telemetry_manager.h"

namespace fortico {
namespace services {

class ITelemetrySink {
public:
	virtual ~ITelemetrySink() = default;
	virtual bool publish(const char* payload, std::size_t length) = 0;
};

class TelemetryService : public interfaces::IService {
public:
	TelemetryService(managers::TelemetryManager& manager,
					 ITelemetrySink* sink = nullptr);
	bool start() override;
	void stop() override;
	bool isRunning() const override;
	void update();
	const char* lastPayload() const;

private:
	managers::TelemetryManager& _manager;
	ITelemetrySink* _sink;
	char _payload[1024];
	bool _running;
};

}  // namespace services
}  // namespace fortico
