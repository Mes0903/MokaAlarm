#include "controllers/SchedulerService.h"
#include <combaseapi.h>
#include <format>
#include <taskschd.h>
#include <windows.h>
#include <wrl/client.h>
#include <iostream>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace {

// ─── RAII helpers ─────────────────────────────────────────────────────────────

// Wraps a BSTR with automatic SysFreeString on destruction.
struct BStr {
	BSTR b;
	explicit BStr(const wchar_t *s) : b(SysAllocString(s)) {}
	~BStr() { SysFreeString(b); }
	operator BSTR() const { return b; }
};

// Wraps a VT_BSTR VARIANT with automatic VariantClear on destruction.
struct BStrVar {
	VARIANT v;
	explicit BStrVar(const wchar_t *s)
	{
		VariantInit(&v);
		v.vt			= VT_BSTR;
		v.bstrVal = SysAllocString(s);
	}
	~BStrVar() { VariantClear(&v); }
};

// Manages CoInitializeEx / CoUninitialize for a single call scope.
struct CoGuard {
	HRESULT hr_;
	bool shouldUninit_;
	explicit CoGuard(DWORD model = COINIT_APARTMENTTHREADED)
	{
		hr_						= CoInitializeEx(nullptr, model);
		shouldUninit_ = (hr_ == S_OK);
	}
	~CoGuard()
	{
		if (shouldUninit_)
			CoUninitialize();
	}
	// True if COM is usable (initialized by us, already initialized, or different model).
	bool ok() const { return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE; }
};

// ─── helpers ──────────────────────────────────────────────────────────────────

VARIANT emptyVar()
{
	VARIANT v;
	VariantInit(&v);
	return v;
}

std::wstring toWide(const std::string &s)
{
	if (s.empty())
		return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring ws(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), len);
	return ws;
}

// Builds an ISO 8601 start-boundary string. A fixed past date makes the trigger immediately active.
std::wstring startBoundary(int hour, int minute)
{
	wchar_t buf[32];
	swprintf_s(buf, L"2025-01-01T%02d:%02d:00", hour, minute);
	return buf;
}

// Maps repeat_days (0=Sun ... 6=Sat) to the TASK_DAY_OF_WEEK bitmask.
// TASK_SUNDAY=0x01, TASK_MONDAY=0x02, ..., TASK_SATURDAY=0x40
short daysOfWeekMask(const std::vector<int> &repeatDays)
{
	short mask = 0;
	for (int d : repeatDays)
		mask |= static_cast<short>(1 << d);
	return mask;
}

std::string hrErr(const char *op, HRESULT hr)
{
	return std::format("{} failed (HRESULT 0x{:08X})", op, static_cast<uint32_t>(hr));
}

// Builds the task display name: "<label> <id>", or "unnamed <id>" when label is empty.
std::wstring taskNameFor(const std::string &label, const std::string &id)
{
	const std::string prefix = label.empty() ? "unnamed" : label;
	return toWide(prefix + " " + id);
}

// Opens the \MokaAlarm task folder, creating it if it does not exist.
HRESULT getOrCreateAlarmFolder(ITaskService *svc, ITaskFolder **alarmFolder)
{
	ComPtr<ITaskFolder> root;
	HRESULT hr = svc->GetFolder(BStr(L"\\"), &root);
	if (FAILED(hr))
		return hr;

	hr = root->GetFolder(BStr(L"MokaAlarm"), alarmFolder);
	if (SUCCEEDED(hr))
		return S_OK;

	VARIANT vEmpty = emptyVar();
	return root->CreateFolder(BStr(L"MokaAlarm"), vEmpty, alarmFolder);
}

} // namespace

namespace alarm::controller {

// ─── syncAlarm ────────────────────────────────────────────────────────────────
std::expected<void, std::string> SchedulerService::syncAlarm(const model::AlarmModel &alarm,
																														 const std::string &chromePath)
{
	CoGuard co;
	if (!co.ok())
		return std::unexpected(hrErr("CoInitializeEx", co.hr_));

	ComPtr<ITaskService> svc;
	HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&svc));
	if (FAILED(hr))
		return std::unexpected(hrErr("CoCreateInstance(TaskScheduler)", hr));

	VARIANT v = emptyVar();
	hr				= svc->Connect(v, v, v, v);
	if (FAILED(hr))
		return std::unexpected(hrErr("ITaskService::Connect", hr));

	ComPtr<ITaskFolder> folder;
	hr = getOrCreateAlarmFolder(svc.Get(), &folder);
	if (FAILED(hr))
		return std::unexpected(hrErr("GetOrCreateAlarmFolder", hr));

	ComPtr<ITaskDefinition> taskDef;
	hr = svc->NewTask(0, &taskDef);
	if (FAILED(hr))
		return std::unexpected(hrErr("ITaskService::NewTask", hr));

	// ── Principal: run as current interactive user ──────────────────────────────
	ComPtr<IPrincipal> principal;
	if (SUCCEEDED(taskDef->get_Principal(&principal))) {
		principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
		principal->put_RunLevel(TASK_RUNLEVEL_LUA);
	}

	// ── Task settings ───────────────────────────────────────────────────────────
	ComPtr<ITaskSettings> settings;
	if (SUCCEEDED(taskDef->get_Settings(&settings))) {
		settings->put_StartWhenAvailable(VARIANT_FALSE);
		settings->put_Enabled(alarm.enabled ? VARIANT_TRUE : VARIANT_FALSE);
		settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
		settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
	}

	// ── Weekly trigger ──────────────────────────────────────────────────────────
	ComPtr<ITriggerCollection> triggers;
	hr = taskDef->get_Triggers(&triggers);
	if (FAILED(hr))
		return std::unexpected(hrErr("get_Triggers", hr));

	ComPtr<ITrigger> trigger;
	hr = triggers->Create(TASK_TRIGGER_WEEKLY, &trigger);
	if (FAILED(hr))
		return std::unexpected(hrErr("ITriggerCollection::Create(weekly)", hr));

	ComPtr<IWeeklyTrigger> weekly;
	hr = trigger->QueryInterface(IID_PPV_ARGS(&weekly));
	if (FAILED(hr))
		return std::unexpected(hrErr("QueryInterface(IWeeklyTrigger)", hr));

	const std::wstring sb = startBoundary(alarm.hour, alarm.minute);
	weekly->put_StartBoundary(BStr(sb.c_str()));
	weekly->put_DaysOfWeek(daysOfWeekMask(alarm.repeat_days));
	weekly->put_WeeksInterval(1);

	// ── Exec action: open YouTube URL with Chrome ───────────────────────────────
	ComPtr<IActionCollection> actions;
	hr = taskDef->get_Actions(&actions);
	if (FAILED(hr))
		return std::unexpected(hrErr("get_Actions", hr));

	ComPtr<IAction> action;
	hr = actions->Create(TASK_ACTION_EXEC, &action);
	if (FAILED(hr))
		return std::unexpected(hrErr("IActionCollection::Create(exec)", hr));

	ComPtr<IExecAction> exec;
	hr = action->QueryInterface(IID_PPV_ARGS(&exec));
	if (FAILED(hr))
		return std::unexpected(hrErr("QueryInterface(IExecAction)", hr));

	exec->put_Path(BStr(toWide(chromePath).c_str()));
	exec->put_Arguments(BStr(toWide(alarm.youtube_url).c_str()));

	// ── Register (create or replace) the task ──────────────────────────────────
	BStrVar sddl(L"");
	const std::wstring taskName = taskNameFor(alarm.label, alarm.id);
	ComPtr<IRegisteredTask> registered;
	hr = folder->RegisterTaskDefinition(BStr(taskName.c_str()),
																			taskDef.Get(),
																			TASK_CREATE_OR_UPDATE,
																			emptyVar(),
																			emptyVar(),
																			TASK_LOGON_INTERACTIVE_TOKEN,
																			sddl.v,
																			&registered);
	if (FAILED(hr))
		return std::unexpected(hrErr("RegisterTaskDefinition", hr));

	return {};
}

// ─── deleteTask ───────────────────────────────────────────────────────────────
std::expected<void, std::string> SchedulerService::deleteTask(const model::AlarmModel &alarm)
{
	CoGuard co;
	if (!co.ok())
		return std::unexpected(hrErr("CoInitializeEx", co.hr_));

	ComPtr<ITaskService> svc;
	HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&svc));
	if (FAILED(hr))
		return std::unexpected(hrErr("CoCreateInstance(TaskScheduler)", hr));

	VARIANT v = emptyVar();
	hr				= svc->Connect(v, v, v, v);
	if (FAILED(hr))
		return std::unexpected(hrErr("ITaskService::Connect", hr));

	ComPtr<ITaskFolder> root;
	hr = svc->GetFolder(BStr(L"\\"), &root);
	if (FAILED(hr))
		return std::unexpected(hrErr("GetFolder(root)", hr));

	ComPtr<ITaskFolder> folder;
	hr = root->GetFolder(BStr(L"MokaAlarm"), &folder);
	if (FAILED(hr))
		return {}; // Folder (and task) doesn't exist, nothing to do.

	const std::wstring taskName = taskNameFor(alarm.label, alarm.id);
	hr													= folder->DeleteTask(BStr(taskName.c_str()), 0);
	if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		return std::unexpected(hrErr("DeleteTask", hr));

	return {};
}

// ─── cleanAllTasks ────────────────────────────────────────────────────────────
std::expected<void, std::string> SchedulerService::cleanAllTasks()
{
	CoGuard co;
	if (!co.ok())
		return std::unexpected(hrErr("CoInitializeEx", co.hr_));

	ComPtr<ITaskService> svc;
	HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&svc));
	if (FAILED(hr))
		return std::unexpected(hrErr("CoCreateInstance(TaskScheduler)", hr));

	VARIANT v = emptyVar();
	hr				= svc->Connect(v, v, v, v);
	if (FAILED(hr))
		return std::unexpected(hrErr("ITaskService::Connect", hr));

	ComPtr<ITaskFolder> root;
	hr = svc->GetFolder(BStr(L"\\"), &root);
	if (FAILED(hr))
		return std::unexpected(hrErr("GetFolder(root)", hr));

	ComPtr<ITaskFolder> folder;
	hr = root->GetFolder(BStr(L"MokaAlarm"), &folder);
	if (FAILED(hr))
		return {}; // Folder doesn't exist, nothing to do.

	// Enumerate all tasks, including disabled/hidden ones.
	ComPtr<IRegisteredTaskCollection> tasks;
	hr = folder->GetTasks(TASK_ENUM_HIDDEN, &tasks);
	if (FAILED(hr))
		return std::unexpected(hrErr("ITaskFolder::GetTasks", hr));

	LONG count = 0;
	hr				 = tasks->get_Count(&count);
	if (FAILED(hr))
		return std::unexpected(hrErr("IRegisteredTaskCollection::get_Count", hr));

	// Collect names first, avoid mutating the folder while iterating the collection.
	std::vector<std::wstring> names;
	names.reserve(static_cast<size_t>(count));
	for (LONG i = 1; i <= count; ++i) {
		VARIANT idx;
		VariantInit(&idx);
		idx.vt	 = VT_I4;
		idx.lVal = i;
		ComPtr<IRegisteredTask> task;
		if (FAILED(tasks->get_Item(idx, &task)))
			continue;
		BSTR name = nullptr;
		if (SUCCEEDED(task->get_Name(&name))) {
			names.emplace_back(name);
			SysFreeString(name);
		}
	}

	for (const auto &name : names) {
		hr = folder->DeleteTask(BStr(name.c_str()), 0);
		if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			return std::unexpected(hrErr("DeleteTask", hr));
	}

	return {};
}

} // namespace alarm::controller
