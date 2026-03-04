#pragma once

#include "models/AlarmModel.h"
#include "models/SettingsModel.h"
#include <string>
#include <vector>

namespace alarm::controller {

class AlarmController {
public:
	void load();

	[[nodiscard]] const std::vector<model::AlarmModel> &alarms() const noexcept;

	void addAlarm(model::AlarmModel alarm);
	void updateAlarm(const model::AlarmModel &alarm);
	void deleteAlarm(const std::string &id);
	void setEnabled(const std::string &id, bool enabled);

	[[nodiscard]] const model::SettingsModel &settings() const noexcept;
	void saveSettings(model::SettingsModel s);

private:
	std::vector<model::AlarmModel> alarms_;
	model::SettingsModel settings_;

	void persist_();
	void sortAlarms_();
	[[nodiscard]] static std::string generateId_();
};

} // namespace alarm::controller
