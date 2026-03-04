#pragma once

#include "models/AlarmModel.h"
#include "models/SettingsModel.h"
#include <vector>

namespace alarm::controller {

class PersistenceService {
public:
	[[nodiscard]] static std::vector<model::AlarmModel> loadAlarms();
	static void saveAlarms(const std::vector<model::AlarmModel> &alarms);

	[[nodiscard]] static model::SettingsModel loadSettings();
	static void saveSettings(const model::SettingsModel &settings);

private:
	static constexpr const char *ALARMS_PATH	 = "data/alarms.json";
	static constexpr const char *SETTINGS_PATH = "data/settings.json";
};

} // namespace alarm::controller
