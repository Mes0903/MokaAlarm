#pragma once

#include "models/AlarmModel.h"
#include <expected>
#include <string>

namespace alarm::controller {

// Encapsulates Windows Task Scheduler COM API.
// All methods are static. COM init/uninit is handled internally per call.
class SchedulerService {
public:
	// Creates or updates the Task Scheduler task for the given alarm.
	// Task path: \MokaAlarm\<label> <id>  (or "unnamed <id>" when label is empty)
	// If alarm.enabled is false the task is registered but disabled.
	[[nodiscard]] static std::expected<void, std::string> syncAlarm(const model::AlarmModel &alarm,
																																	const std::string &chromePath);

	// Deletes the Task Scheduler task for the given alarm.
	// Returns success even if the task did not exist.
	[[nodiscard]] static std::expected<void, std::string> deleteTask(const model::AlarmModel &alarm);

	// Deletes ALL tasks inside the \MokaAlarm\ folder (including any orphans not in alarms.json).
	// Tasks outside \MokaAlarm\ are never touched.
	[[nodiscard]] static std::expected<void, std::string> cleanAllTasks();
};

} // namespace alarm::controller
