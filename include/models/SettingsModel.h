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
	std::string default_youtube_url =
			"https://www.youtube.com/watch?v=xXMuBMhYXIM&list=PLku7p0RAD_yvD1wStCLyZ5iQqkuPAwz2E&index=37";

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
	s.default_youtube_url		 = j.value("default_youtube_url", s.default_youtube_url);
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
			{"default_youtube_url", default_youtube_url},
	};
}

} // namespace alarm::model
