// RewindTime ASI plugin shell (Windows).
//
// STATUS: SOURCE ONLY - never run inside the game.
// The game-access hooks listed in sheets/hooks.json are still unresolved, so
// this plugin stays idle: it reads its settings, writes a log line and returns.
// It touches nothing in the game until every hook is marked "verified" in the
// sheet and the headers are regenerated.
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <fstream>
#include <string>

#include "core.h"
#include "generated/hooks.h"

namespace {

std::ofstream g_log;
timerewind::Config g_cfg;
timerewind::Controller* g_ctl = nullptr;

std::string dir_of(HMODULE module) {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(module, path, MAX_PATH);
    const std::string p(path);
    return p.substr(0, p.find_last_of("\\/") + 1);
}

// To be called once per frame on the game's script thread (hook game.frame_tick).
// Planned body, once the hooks exist:
//   if (game.is_online() || game.is_paused()) { g_ctl->set_enabled(false); return; }
//   g_ctl->set_enabled(true);
//   read a Snapshot, call g_ctl->step(dt, key_held, current, out), and write `out`
//   back to Arthur and the horse when step() returns true.
[[maybe_unused]] void on_frame() {}

DWORD WINAPI init_thread(LPVOID param) {
    const std::string dir = dir_of(static_cast<HMODULE>(param));
    g_log.open(dir + "RewindTime.log", std::ios::trunc);

    std::ifstream ini(dir + "RewindTime.ini");
    if (ini) timerewind::load_ini(ini, g_cfg);

    if (!timerewind::all_hooks_verified()) {
        g_log << "Idle: the game hooks are not verified for this build, nothing is touched.\n";
        return 0;
    }
    g_ctl = new timerewind::Controller(g_cfg);
    g_log << "Ready, waiting for the frame hook.\n";
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        // File work happens on a separate thread, not under the loader lock.
        if (HANDLE h = CreateThread(nullptr, 0, init_thread, module, 0, nullptr)) CloseHandle(h);
    }
    return TRUE;
}

#endif  // _WIN32
