#pragma once

#include "constants/errors.h"
#include "interfaces/ipublisher.h"
#include "types/system_types.h"

namespace fortico {
namespace core {

class ErrorHandler {
public:
	explicit ErrorHandler(interfaces::IPublisher* publisher = nullptr);
	void report(constants::ErrorCode error, uint64_t timestamp = 0U);
	void clear();
	constants::ErrorCode lastError() const;
	bool hasError() const;
	types::Event lastEvent() const;

private:
	interfaces::IPublisher* _publisher;
	constants::ErrorCode _lastError;
	types::Event _lastEvent;
};

}  // namespace core
}  // namespace fortico
