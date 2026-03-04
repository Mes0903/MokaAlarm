#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace alarm::model {

struct AlarmModel {
	std::string id;
	std::string label;
	int hour		 = 9;
	int minute	 = 0;
	bool enabled = true;
	std::vector<int> repeat_days; // 0=Sun, 1=Mon, ..., 6=Sat
	std::string youtube_url;

	[[nodiscard]] static AlarmModel fromJson(const nlohmann::json &j);
	[[nodiscard]] nlohmann::json toJson() const;
};

// ── Inline implementations ────────────────────────────────────────────────────

inline AlarmModel AlarmModel::fromJson(const nlohmann::json &j)
{
	AlarmModel m;
	m.id					= j.value("id", "");
	m.label				= j.value("label", "");
	m.hour				= j.value("hour", 9);
	m.minute			= j.value("minute", 0);
	m.enabled			= j.value("enabled", true);
	m.youtube_url = j.value("youtube_url", "");
	if (j.contains("repeat_days"))
		m.repeat_days = j["repeat_days"].get<std::vector<int>>();
	return m;
}

inline nlohmann::json AlarmModel::toJson() const
{
	return {
			{"id", id},
			{"label", label},
			{"hour", hour},
			{"minute", minute},
			{"enabled", enabled},
			{"repeat_days", repeat_days},
			{"youtube_url", youtube_url},
	};
}

} // namespace alarm::model
