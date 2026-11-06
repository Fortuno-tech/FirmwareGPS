#pragma once

#include <cstring>

#include "types/relay_types.h"

namespace fortico {
namespace app {

struct RemoteCommandExecutionResult {
	char commandId[64];
	char commandName[16];
	char error[40];
	types::RelayState relayState;
	bool success;
};

class RemoteCommandExecution final {
public:
	bool begin(const char* commandId, const char* commandName,
			   types::RelayState expectedRelayState, bool accepted,
			   const char* error)
	{
		if (_pending || _resultReady || commandId == nullptr ||
			commandName == nullptr) {
			return false;
		}
		const std::size_t idLength = std::strlen(commandId);
		const std::size_t nameLength = std::strlen(commandName);
		if (idLength == 0U || idLength >= sizeof(_result.commandId) ||
			nameLength == 0U || nameLength >= sizeof(_result.commandName)) {
			return false;
		}

		_result = {};
		std::memcpy(_result.commandId, commandId, idLength + 1U);
		std::memcpy(_result.commandName, commandName, nameLength + 1U);
		_expectedRelayState = expectedRelayState;
		_accepted = accepted;
		if (error != nullptr) {
			const std::size_t errorLength = std::strlen(error);
			const std::size_t copyLength =
				errorLength < sizeof(_result.error) - 1U
					? errorLength
					: sizeof(_result.error) - 1U;
			std::memcpy(_result.error, error, copyLength);
		}
		_pending = true;
		return true;
	}

	bool complete(bool relayActionSucceeded, types::RelayState actualState)
	{
		if (!_pending) return false;
		_result.relayState = actualState;
		_result.success = _accepted && relayActionSucceeded &&
			actualState == _expectedRelayState;
		if (_result.success) {
			_result.error[0] = '\0';
		} else if (_result.error[0] == '\0') {
			const char* reason = !_accepted
				? "CONTROL_POLICY_REJECTED"
				: (!relayActionSucceeded ? "RELAY_ACTION_FAILED"
										 : "RELAY_STATE_MISMATCH");
			std::strncpy(_result.error, reason, sizeof(_result.error) - 1U);
		}
		_pending = false;
		_resultReady = true;
		return true;
	}

	bool hasPendingResult() const { return _resultReady; }

	bool result(RemoteCommandExecutionResult& output) const
	{
		if (!_resultReady) return false;
		output = _result;
		return true;
	}

	void markPublished()
	{
		_result = {};
		_pending = false;
		_resultReady = false;
	}

	void reset() { markPublished(); }

private:
	RemoteCommandExecutionResult _result{};
	types::RelayState _expectedRelayState{types::RelayState::LOCKED};
	bool _accepted{false};
	bool _pending{false};
	bool _resultReady{false};
};

}  // namespace app
}  // namespace fortico
