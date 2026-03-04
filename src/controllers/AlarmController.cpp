#include "controllers/AlarmController.h"
#include "controllers/PersistenceService.h"
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace alarm::controller {

// ─── load ─────────────────────────────────────────────────────────────────────
void AlarmController::load()
{
	alarms_		= PersistenceService::loadAlarms();
	settings_ = PersistenceService::loadSettings();
	sortAlarms_();
}

// ─── alarms ───────────────────────────────────────────────────────────────────
const std::vector<model::AlarmModel> &AlarmController::alarms() const noexcept { return alarms_; }

// ─── addAlarm ─────────────────────────────────────────────────────────────────
void AlarmController::addAlarm(model::AlarmModel alarm)
{
	alarm.id = generateId_();
	alarms_.push_back(std::move(alarm));
	sortAlarms_();
	persist_();
}

// ─── updateAlarm ──────────────────────────────────────────────────────────────
void AlarmController::updateAlarm(const model::AlarmModel &alarm)
{
	auto it = std::ranges::find(alarms_, alarm.id, &model::AlarmModel::id);
	if (it != alarms_.end()) {
		*it = alarm;
		sortAlarms_();
		persist_();
	}
}

// ─── deleteAlarm ──────────────────────────────────────────────────────────────
void AlarmController::deleteAlarm(const std::string &id)
{
	std::erase_if(alarms_, [&](const auto &a) { return a.id == id; });
	persist_();
}

// ─── setEnabled ───────────────────────────────────────────────────────────────
void AlarmController::setEnabled(const std::string &id, bool enabled)
{
	auto it = std::ranges::find(alarms_, id, &model::AlarmModel::id);
	if (it != alarms_.end()) {
		it->enabled = enabled;
		persist_();
	}
}

// ─── settings ─────────────────────────────────────────────────────────────────
const model::SettingsModel &AlarmController::settings() const noexcept { return settings_; }

// ─── saveSettings ─────────────────────────────────────────────────────────────
void AlarmController::saveSettings(model::SettingsModel s)
{
	settings_ = std::move(s);
	PersistenceService::saveSettings(settings_);
}

// ─── persist_ ─────────────────────────────────────────────────────────────────
void AlarmController::persist_() { PersistenceService::saveAlarms(alarms_); }

// ─── sortAlarms_ ──────────────────────────────────────────────────────────────
void AlarmController::sortAlarms_()
{
	std::ranges::sort(
			alarms_, [](const auto &a, const auto &b) { return a.hour != b.hour ? a.hour < b.hour : a.minute < b.minute; });
}

// ─── generateId_ ──────────────────────────────────────────────────────────────
std::string AlarmController::generateId_()
{
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint64_t> dis;

	uint64_t hi = dis(gen);
	uint64_t lo = dis(gen);

	// UUID v4: set version bits
	hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
	// Set variant bits
	lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

	std::ostringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(8) << (hi >> 32) << '-' << std::setw(4) << ((hi >> 16) & 0xFFFF)
		 << '-' << std::setw(4) << (hi & 0xFFFF) << '-' << std::setw(4) << (lo >> 48) << '-' << std::setw(12)
		 << (lo & 0x0000FFFFFFFFFFFFULL);
	return ss.str();
}

} // namespace alarm::controller
