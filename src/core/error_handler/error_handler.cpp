#include "core/error_handler/error_handler.h"

namespace fortico {
namespace core {

ErrorHandler::ErrorHandler(interfaces::IPublisher* publisher)
	: _publisher(publisher), _lastError(constants::ErrorCode::OK), _lastEvent{} {}

void ErrorHandler::report(constants::ErrorCode error, uint64_t timestamp)
{
	if (error == constants::ErrorCode::OK) return;
	_lastError = error;
	_lastEvent = {constants::EventID::SYSTEM_FAULT, timestamp,
				  static_cast<uint32_t>(error)};
	if (_publisher != nullptr) _publisher->publish(_lastEvent);
}

void ErrorHandler::clear()
{
	_lastError = constants::ErrorCode::OK;
	_lastEvent = {};
}

constants::ErrorCode ErrorHandler::lastError() const { return _lastError; }
bool ErrorHandler::hasError() const { return _lastError != constants::ErrorCode::OK; }
types::Event ErrorHandler::lastEvent() const { return _lastEvent; }

}  // namespace core
}  // namespace fortico
