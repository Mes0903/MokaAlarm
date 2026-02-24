#include "Application.h"

// Defined in Application.cpp alongside WndProcDispatch.
extern Application *g_App;

// ─── Entry point ─────────────────────────────────────────────────────────────
// Using int main() with /ENTRY:mainCRTStartup (Release) keeps a single entry
// point for both Debug (console subsystem) and Release (Windows subsystem).
int main()
{
	Application app;
	g_App		= &app;
	int ret = app.run();
	g_App		= nullptr;
	return ret;
}
