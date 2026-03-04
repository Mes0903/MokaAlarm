#include "views/AlarmView.h"

#include "imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace alarm::view {

namespace {
constexpr std::array<const char *, 7> kDayAbbr{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
} // namespace

// ─── Constructor ─────────────────────────────────────────────────────────────
AlarmView::AlarmView(controller::AlarmController &ctrl) : ctrl_(ctrl) {}

// ─── render ──────────────────────────────────────────────────────────────────
void AlarmView::render()
{
	const ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(io.DisplaySize);
	constexpr ImGuiWindowFlags kWinFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
																				 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
																				 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::Begin("##AlarmMain", nullptr, kWinFlags);

	// ── Toolbar ──────────────────────────────────────────────────────────────
	ImGui::SetWindowFontScale(1.4f);
	ImGui::Text("Alarms");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::SameLine();

	// Push buttons to the far right
	const float avail		 = ImGui::GetContentRegionAvail().x;
	const float btnWidth = 100.0f;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - btnWidth * 2.0f - 8.0f);

	if (ImGui::Button("Settings", ImVec2(btnWidth, 0)))
		showSettingsDialog_ = true;
	ImGui::SameLine(0, 8);
	if (ImGui::Button("+ Add Alarm", ImVec2(btnWidth, 0)))
		openAddDialog_();

	ImGui::Separator();

	// ── Alarm list ───────────────────────────────────────────────────────────
	renderAlarmList_();

	ImGui::End();

	// ── Dialogs (rendered outside the main window) ───────────────────────────
	renderEditDialog_();
	renderSettingsDialog_();
	renderDeleteConfirm_();
	renderCloseHintDialog_();
}

// ─── renderAlarmList_ ────────────────────────────────────────────────────────
void AlarmView::renderAlarmList_()
{
	const auto &alarms = ctrl_.alarms();

	// Empty state
	if (alarms.empty()) {
		ImGui::Spacing();
		const char *msg = "No alarms.  Click '+ Add Alarm' to create one.";
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(msg).x) * 0.5f +
												 ImGui::GetCursorPosX());
		ImGui::TextDisabled("%s", msg);
		return;
	}

	// One table for all alarm rows
	constexpr ImGuiTableFlags kTableFlags =
			ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
	if (!ImGui::BeginTable("##AlarmTable", 3, kTableFlags, ImVec2(-1.0f, 0.0f)))
		return;

	ImGui::TableSetupColumn("##time", ImGuiTableColumnFlags_WidthFixed, 92.0f);
	ImGui::TableSetupColumn("##info", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("##btns", ImGuiTableColumnFlags_WidthFixed, 158.0f);

	for (const auto &alarm : alarms) {
		ImGui::PushID(alarm.id.c_str());
		renderAlarmItem_(alarm);
		ImGui::PopID();
	}

	ImGui::EndTable();
}

// ─── renderAlarmItem_ ────────────────────────────────────────────────────────
void AlarmView::renderAlarmItem_(const model::AlarmModel &alarm)
{
	// Row tall enough for 1.8× time text + one line of sub-text
	const float bigTextH = ImGui::GetFontSize() * 1.8f;
	const float rowH		 = bigTextH + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 4.0f;
	ImGui::TableNextRow(0, rowH);

	// ── Time column ────────────────────────────────────────────────────────
	ImGui::TableSetColumnIndex(0);
	const float timeOffY = (rowH - bigTextH) * 0.5f - ImGui::GetStyle().FramePadding.y;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + timeOffY);

	if (!alarm.enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

	ImGui::SetWindowFontScale(1.8f);
	char timeBuf[8];
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", alarm.hour, alarm.minute);
	ImGui::TextUnformatted(timeBuf);
	ImGui::SetWindowFontScale(1.0f);

	if (!alarm.enabled)
		ImGui::PopStyleColor();

	// ── Info column ────────────────────────────────────────────────────────
	ImGui::TableSetColumnIndex(1);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y * 2.0f);

	// Label
	if (!alarm.label.empty())
		ImGui::TextUnformatted(alarm.label.c_str());
	else
		ImGui::TextDisabled("(no label)");

	// Repeat days + URL preview on a second line
	std::string info;
	for (int d : alarm.repeat_days) {
		if (d >= 0 && d < 7) {
			if (!info.empty())
				info += ' ';
			info += kDayAbbr[d];
		}
	}
	if (!alarm.youtube_url.empty()) {
		if (!info.empty())
			info += "  ·  ";
		std::string url = alarm.youtube_url;
		if (url.size() > 34)
			url = url.substr(0, 34) + "...";
		info += url;
	}
	if (!info.empty())
		ImGui::TextDisabled("%s", info.c_str());

	// ── Buttons column ─────────────────────────────────────────────────────
	ImGui::TableSetColumnIndex(2);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (rowH - ImGui::GetFrameHeight()) * 0.5f -
											 ImGui::GetStyle().FramePadding.y);

	// Toggle ON/OFF
	if (alarm.enabled) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.78f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.68f, 0.30f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.58f, 0.26f, 1.0f));
		if (ImGui::SmallButton(" ON "))
			ctrl_.setEnabled(alarm.id, false);
		ImGui::PopStyleColor(3);
	}
	else {
		if (ImGui::SmallButton("OFF"))
			ctrl_.setEnabled(alarm.id, true);
	}

	ImGui::SameLine(0, 6);
	if (ImGui::SmallButton("Edit"))
		openEditDialog_(alarm);

	ImGui::SameLine(0, 6);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.18f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.12f, 0.12f, 1.0f));
	if (ImGui::SmallButton(" X ")) {
		pendingDeleteId_	 = alarm.id;
		showDeleteConfirm_ = true;
	}
	ImGui::PopStyleColor(3);
}

// ─── renderEditDialog_ ───────────────────────────────────────────────────────
void AlarmView::renderEditDialog_()
{
	static bool open			= true;
	const char *popupName = isAddMode_ ? "Add Alarm##EditDlg" : "Edit Alarm##EditDlg";
	if (showEditDialog_) {
		ImGui::OpenPopup(popupName);
		showEditDialog_ = false;
		open						= true;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(440, 270), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal(popupName, &open, ImGuiWindowFlags_NoSavedSettings))
		return;

	ImGui::Spacing();

	// Label
	constexpr float kLabelW = 70.0f;
	ImGui::Text("Label");
	ImGui::SameLine(kLabelW);
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##lbl", editLabel_, sizeof(editLabel_));

	// Time
	ImGui::Spacing();
	ImGui::Text("Time");
	ImGui::SameLine(kLabelW);
	ImGui::SetNextItemWidth(64.0f);
	ImGui::InputScalar("##hr", ImGuiDataType_S32, &editHour_, nullptr, nullptr, "%02d");
	if (ImGui::IsItemHovered())
		editHour_ += static_cast<int>(ImGui::GetIO().MouseWheel);
	editHour_ = std::clamp(editHour_, 0, 23);
	ImGui::SameLine(0, 4);
	ImGui::TextUnformatted(":");
	ImGui::SameLine(0, 4);
	ImGui::SetNextItemWidth(64.0f);
	ImGui::InputScalar("##mn", ImGuiDataType_S32, &editMinute_, nullptr, nullptr, "%02d");
	if (ImGui::IsItemHovered())
		editMinute_ += static_cast<int>(ImGui::GetIO().MouseWheel);
	editMinute_ = std::clamp(editMinute_, 0, 59);

	// Repeat days
	ImGui::Spacing();
	ImGui::Text("Repeat");
	ImGui::SameLine(kLabelW);
	for (int i = 0; i < 7; i++) {
		if (i > 0)
			ImGui::SameLine(0, 6);
		// Highlight selected days
		if (editDays_[i]) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.85f));
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		}
		char dayId[8];
		snprintf(dayId, sizeof(dayId), "%s##d%d", kDayAbbr[i], i);
		if (ImGui::SmallButton(dayId))
			editDays_[i] = !editDays_[i];
		ImGui::PopStyleColor(2);
	}

	// YT URL
	ImGui::Spacing();
	ImGui::Text("YT URL");
	ImGui::SameLine(kLabelW);
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##url", editUrl_, sizeof(editUrl_));

	// Error message
	if (!editError_.empty()) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
		ImGui::TextWrapped("%s", editError_.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Cancel / Save
	const float bw = 90.0f;
	ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - bw * 2.0f - 8.0f) * 0.5f + ImGui::GetCursorPosX());
	if (ImGui::Button("Cancel", ImVec2(bw, 0))) {
		editError_.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine(0, 8);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.85f));
	if (ImGui::Button("Save", ImVec2(bw, 0)))
		saveEditDialog_();
	ImGui::PopStyleColor(2);

	ImGui::EndPopup();
}

// ─── renderSettingsDialog_ ───────────────────────────────────────────────────
void AlarmView::renderSettingsDialog_()
{
	static bool open = true;
	if (showSettingsDialog_) {
		ImGui::OpenPopup("Settings##Dlg");
		showSettingsDialog_ = false;
		const auto &s				= ctrl_.settings();
		pendingChromePath_	= s.chrome_path;
		pendingCloseTray_		= s.close_to_tray;
		pendingShowHint_		= !s.suppress_minimize_hint;
		open								= true;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal("Settings##Dlg", &open, ImGuiWindowFlags_NoSavedSettings))
		return;

	ImGui::Spacing();

	// ── Chrome Path ───────────────────────────────────────────────────────
	ImGui::Text("Chrome Path");
	ImGui::TextWrapped("%s", pendingChromePath_.c_str());
	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.1f, 1.0f));
	ImGui::TextWrapped(
			"Warning: The selected executable is not verified to be Chrome. It will be invoked directly by the task "
			"scheduler.");
	ImGui::PopStyleColor();
	ImGui::Spacing();

	if (ImGui::Button("Browse...")) {
		char path[MAX_PATH] = {};
		OPENFILENAMEA ofn		= {};
		ofn.lStructSize			= sizeof(ofn);
		ofn.lpstrFilter			= "Chrome Executable\0chrome.exe\0All Executables\0*.exe\0\0";
		ofn.lpstrFile				= path;
		ofn.nMaxFile				= MAX_PATH;
		ofn.lpstrTitle			= "Select chrome.exe";
		ofn.lpstrInitialDir = "C:\\Program Files\\Google\\Chrome\\Application";
		ofn.Flags						= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn))
			pendingChromePath_ = path;
	}
	ImGui::SameLine(0, 8);
	if (ImGui::Button("Reset to Default##chrome"))
		pendingChromePath_ = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ── Close button behaviour ────────────────────────────────────────────
	ImGui::Text("Close button");
	ImGui::Checkbox("Minimize to tray##closeOpt", &pendingCloseTray_);
	if (pendingCloseTray_)
		ImGui::Checkbox("Show minimize hint##hintOpt", &pendingShowHint_);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ── Window size ───────────────────────────────────────────────────────
	ImGui::Text("Window Size:");
	const auto &curS = ctrl_.settings();
	ImGui::SameLine(0, 8);
	ImGui::Text("%d x %d", curS.window_width, curS.window_height);
	if (ImGui::Button("Reset window size##wnd")) {
		// Immediately resize and save — no need to wait for Save.
		auto s					= ctrl_.settings();
		s.window_width	= 1280;
		s.window_height = 800;
		ctrl_.saveSettings(std::move(s));
		pendingWindowResize_ = true;
		windowResizeW_			 = 1280;
		windowResizeH_			 = 800;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ── Cancel / Save ─────────────────────────────────────────────────────
	constexpr float kBw = 80.0f;
	ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - kBw * 2.0f - 8.0f) * 0.5f + ImGui::GetCursorPosX());
	if (ImGui::Button("Cancel", ImVec2(kBw, 0)))
		ImGui::CloseCurrentPopup();
	ImGui::SameLine(0, 8);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.85f));
	if (ImGui::Button("Save", ImVec2(kBw, 0))) {
		auto s									 = ctrl_.settings(); // preserves auto-saved window size
		s.chrome_path						 = pendingChromePath_;
		s.close_to_tray					 = pendingCloseTray_;
		s.suppress_minimize_hint = !pendingShowHint_;
		ctrl_.saveSettings(std::move(s));
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopStyleColor(2);

	ImGui::EndPopup();
}

// ─── renderDeleteConfirm_ ────────────────────────────────────────────────────
void AlarmView::renderDeleteConfirm_()
{
	if (showDeleteConfirm_) {
		ImGui::OpenPopup("##DeleteConfirm");
		showDeleteConfirm_ = false;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(260, 110), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal("##DeleteConfirm", nullptr, ImGuiWindowFlags_NoSavedSettings))
		return;

	ImGui::Text("Delete this alarm?");
	ImGui::Spacing();

	if (ImGui::Button("Cancel", ImVec2(80, 0))) {
		pendingDeleteId_.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine(0, 8);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.18f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
	if (ImGui::Button("Delete", ImVec2(80, 0))) {
		ctrl_.deleteAlarm(pendingDeleteId_);
		pendingDeleteId_.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopStyleColor(2);

	ImGui::EndPopup();
}

// ─── openAddDialog_ ──────────────────────────────────────────────────────────
void AlarmView::openAddDialog_()
{
	isAddMode_ = true;
	editId_.clear();
	editLabel_[0] = '\0';
	editHour_			= 9;
	editMinute_		= 0;
	// Default: Mon–Fri
	for (int i = 0; i < 7; i++)
		editDays_[i] = (i >= 1 && i <= 5);
	editUrl_[0] = '\0';
	editError_.clear();
	showEditDialog_ = true;
}

// ─── openEditDialog_ ─────────────────────────────────────────────────────────
void AlarmView::openEditDialog_(const model::AlarmModel &alarm)
{
	isAddMode_ = false;
	editId_		 = alarm.id;
	std::strncpy(editLabel_, alarm.label.c_str(), sizeof(editLabel_) - 1);
	editLabel_[sizeof(editLabel_) - 1] = '\0';
	editHour_													 = alarm.hour;
	editMinute_												 = alarm.minute;
	for (int i = 0; i < 7; i++)
		editDays_[i] = false;
	for (int d : alarm.repeat_days)
		if (d >= 0 && d < 7)
			editDays_[d] = true;
	std::strncpy(editUrl_, alarm.youtube_url.c_str(), sizeof(editUrl_) - 1);
	editUrl_[sizeof(editUrl_) - 1] = '\0';
	editError_.clear();
	showEditDialog_ = true;
}

// ─── saveEditDialog_ ─────────────────────────────────────────────────────────
void AlarmView::saveEditDialog_()
{
	editError_.clear();

	// Validate: at least one day
	bool anyDay = false;
	for (int i = 0; i < 7; i++)
		anyDay |= editDays_[i];
	if (!anyDay) {
		editError_ = "Select at least one repeat day.";
		return;
	}

	// Validate: URL not empty
	if (editUrl_[0] == '\0') {
		editError_ = "YouTube URL cannot be empty.";
		return;
	}

	model::AlarmModel alarm;
	alarm.label				= editLabel_;
	alarm.hour				= editHour_;
	alarm.minute			= editMinute_;
	alarm.youtube_url = editUrl_;
	alarm.enabled			= true;
	for (int i = 0; i < 7; i++)
		if (editDays_[i])
			alarm.repeat_days.push_back(i);

	if (isAddMode_) {
		ctrl_.addAlarm(std::move(alarm));
	}
	else {
		alarm.id = editId_;
		// Preserve the existing enabled state
		const auto &existing = ctrl_.alarms();
		auto it = std::ranges::find(existing, editId_, &alarm::model::AlarmModel::id);
		if (it != existing.end())
			alarm.enabled = it->enabled;
		ctrl_.updateAlarm(alarm);
	}

	ImGui::CloseCurrentPopup();
}

// ─── triggerCloseHint ────────────────────────────────────────────────────────
void AlarmView::triggerCloseHint() noexcept
{
	showCloseHintDialog_ = true;
	closeHintDontShow_	 = false;
}

// ─── pollCloseDecision ───────────────────────────────────────────────────────
AlarmView::CloseDecision AlarmView::pollCloseDecision() noexcept
{
	CloseDecision d = closeDecision_;
	closeDecision_	= CloseDecision::None;
	return d;
}

// ─── pollWindowResize ────────────────────────────────────────────────────────
bool AlarmView::pollWindowResize(int &w, int &h) noexcept
{
	if (!pendingWindowResize_)
		return false;
	w										 = windowResizeW_;
	h										 = windowResizeH_;
	pendingWindowResize_ = false;
	return true;
}

// ─── renderCloseHintDialog_ ──────────────────────────────────────────────────
void AlarmView::renderCloseHintDialog_()
{
	if (showCloseHintDialog_) {
		ImGui::OpenPopup("##CloseHint");
		showCloseHintDialog_ = false;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal(
					"##CloseHint", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::Text("Minimize to tray?");
	ImGui::Spacing();
	ImGui::TextWrapped("The window will be minimized to the system tray.\nDouble-click the tray icon to restore it.");
	ImGui::Spacing();
	ImGui::Checkbox("Don't show this again", &closeHintDontShow_);
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	constexpr float kBw = 110.0f;
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.85f));
	if (ImGui::Button("Minimize", ImVec2(kBw, 0))) {
		if (closeHintDontShow_) {
			auto s									 = ctrl_.settings();
			s.suppress_minimize_hint = true;
			ctrl_.saveSettings(std::move(s));
		}
		closeDecision_ = CloseDecision::Minimize;
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopStyleColor(2);

	ImGui::SameLine(0, 8);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.18f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
	if (ImGui::Button("Close App", ImVec2(kBw, 0))) {
		if (closeHintDontShow_) {
			auto s					= ctrl_.settings();
			s.close_to_tray = false;
			ctrl_.saveSettings(std::move(s));
		}
		closeDecision_ = CloseDecision::Exit;
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopStyleColor(2);

	ImGui::EndPopup();
}

} // namespace alarm::view
