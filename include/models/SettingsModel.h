#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace alarm::model {

struct SettingsModel {
	std::string chrome_path			= "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
	bool close_to_tray					= true;
	bool suppress_minimize_hint = false;
	int window_width						= 1280;
	int window_height						= 800;

	[[nodiscard]] static SettingsModel fromJson(const nlohmann::json &j);
	[[nodiscard]] nlohmann::json toJson() const;
};

// ── Inline implementations ────────────────────────────────────────────────────

inline SettingsModel SettingsModel::fromJson(const nlohmann::json &j)
{
	SettingsModel s;
	s.chrome_path						 = j.value("chrome_path", s.chrome_path);
	s.close_to_tray					 = j.value("close_to_tray", s.close_to_tray);
	s.suppress_minimize_hint = j.value("suppress_minimize_hint", s.suppress_minimize_hint);
	s.window_width					 = j.value("window_width", s.window_width);
	s.window_height					 = j.value("window_height", s.window_height);
	return s;
}

inline nlohmann::json SettingsModel::toJson() const
{
	return {
			{"chrome_path", chrome_path},
			{"close_to_tray", close_to_tray},
			{"suppress_minimize_hint", suppress_minimize_hint},
			{"window_width", window_width},
			{"window_height", window_height},
	};
}

} // namespace alarm::model
