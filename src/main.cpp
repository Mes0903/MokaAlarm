#include "Application.h"

// Defined in Application.cpp alongside WndProcDispatch.
extern Application *g_App;

// ─── Entry point ─────────────────────────────────────────────────────────────
// Using int main() with /ENTRY:mainCRTStartup (Release) keeps a single entry
// point for both Debug (console subsystem) and Release (Windows subsystem).
int main()
{
	// ── Single-instance guard ─────────────────────────────────────────────
	HANDLE mutex = ::CreateMutexW(nullptr, TRUE, L"AlarmAppSingleInstanceMutex");
	if (::GetLastError() == ERROR_ALREADY_EXISTS) {
		// Another instance is running: find its window, restore and focus it.
		HWND existing = ::FindWindowW(L"MokaAlarmWndClass", nullptr);
		if (existing) {
			::ShowWindow(existing, SW_SHOW);
			::SetForegroundWindow(existing);
		}
		if (mutex)
			::CloseHandle(mutex);
		return 0;
	}

	Application app;
	g_App		= &app;
	int ret = app.run();
	g_App		= nullptr;

	if (mutex)
		::CloseHandle(mutex);
	return ret;
}
