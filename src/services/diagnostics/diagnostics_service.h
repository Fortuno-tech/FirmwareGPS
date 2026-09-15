#pragma once

#include "interfaces/ipublisher.h"
#include "interfaces/iservice.h"
#include "managers/diagnostics_manager/diagnostics_manager.hpp"
#include "core/diagnostics/diagnostics.h"

namespace fortico {
namespace services {

class DiagnosticsService : public interfaces::IService {
public:
	DiagnosticsService(managers::DiagnosticsManager& manager,
					   core::Diagnostics& diagnostics,
					   interfaces::IPublisher* publisher = nullptr);
	bool start() override;
	void stop() override;
	bool isRunning() const override;
	void update();
	types::DiagnosticReport report() const;

private:
	managers::DiagnosticsManager& _manager;
	core::Diagnostics& _diagnostics;
	interfaces::IPublisher* _publisher;
	bool _running;
};

}  // namespace services
}  // namespace fortico
