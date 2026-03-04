#pragma once

#include "controllers/AlarmController.h"
#include <string>

namespace alarm::view {

class AlarmView {
public:
	enum class CloseDecision { None, Minimize, Exit };

	explicit AlarmView(controller::AlarmController &ctrl);
	void render();

	// Called by Application when the Win32 close button is pressed.
	void triggerCloseHint() noexcept;
	// Returns (and clears) any close decision made this frame.
	CloseDecision pollCloseDecision() noexcept;
	// Returns true (and clears) if Settings requested a window resize.
	bool pollWindowResize(int &w, int &h) noexcept;

private:
	controller::AlarmController &ctrl_;

	// ── Edit / Add dialog ─────────────────────────────────────────────────────
	bool showEditDialog_ = false;
	bool isAddMode_			 = false;
	std::string editId_;
	char editLabel_[256] = {};
	int editHour_				 = 9;
	int editMinute_			 = 0;
	bool editDays_[7]		 = {};
	char editUrl_[1024]	 = {};
	std::string editError_;

	// ── Settings dialog ───────────────────────────────────────────────────────
	bool showSettingsDialog_ = false;
	std::string pendingChromePath_;
	bool pendingCloseTray_ = true;
	bool pendingShowHint_	 = true;

	// ── Delete confirm ────────────────────────────────────────────────────────
	std::string pendingDeleteId_;
	bool showDeleteConfirm_ = false;

	// ── Close hint dialog ─────────────────────────────────────────────────────
	bool showCloseHintDialog_		 = false;
	bool closeHintDontShow_			 = false;
	CloseDecision closeDecision_ = CloseDecision::None;

	// ── Window resize request ─────────────────────────────────────────────────
	bool pendingWindowResize_ = false;
	int windowResizeW_				= 0;
	int windowResizeH_				= 0;

	void renderAlarmList_();
	void renderAlarmItem_(const model::AlarmModel &alarm);
	void renderEditDialog_();
	void renderSettingsDialog_();
	void renderDeleteConfirm_();
	void renderCloseHintDialog_();

	void openAddDialog_();
	void openEditDialog_(const model::AlarmModel &alarm);
	void saveEditDialog_();
};

} // namespace alarm::view
