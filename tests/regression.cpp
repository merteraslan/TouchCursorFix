// Console regression tests; the app entry point is not called, so no input
// hooks or tray icons are installed. HKCU is redirected to a temporary key.
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define UNICODE
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

// Spy on input APIs so a failing regression cannot move the real mouse,
// release another app's cursor confinement, or change the active window.
static unsigned inputCalls;
static BOOL WINAPI TestSetCursorPos(int, int) { ++inputCalls; return TRUE; }
static UINT WINAPI TestSendInput(UINT count, LPINPUT, int) { ++inputCalls; return count; }
static BOOL WINAPI TestClipCursor(const RECT*) { ++inputCalls; return TRUE; }
static BOOL WINAPI TestSetForegroundWindow(HWND) { ++inputCalls; return TRUE; }
static BOOL WINAPI TestBringWindowToTop(HWND) { ++inputCalls; return TRUE; }
static BOOL WINAPI TestAttachThreadInput(DWORD, DWORD, BOOL) { ++inputCalls; return TRUE; }
#define SetCursorPos TestSetCursorPos
#define SendInput TestSendInput
#define ClipCursor TestClipCursor
#define SetForegroundWindow TestSetForegroundWindow
#define BringWindowToTop TestBringWindowToTop
#define AttachThreadInput TestAttachThreadInput
#define wWinMain TouchCursorFixEntryPoint
#include "../main.cpp"
#undef wWinMain

static void Check(bool condition, const char* description) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(1);
    }
}

static void TestPreferences() {
    LoadSettings();
    Check(g_enabled && g_restoreFocus, "missing preferences retain defaults");

    // Exercise the same command handlers as the tray menu, then simulate restart.
    WndProc(nullptr, WM_COMMAND, IDM_ENABLED, 0);
    WndProc(nullptr, WM_COMMAND, IDM_FOCUS, 0);
    g_enabled = g_restoreFocus = TRUE;
    LoadSettings();
    Check(!g_enabled && !g_restoreFocus, "disabled preferences survive reload");

    WndProc(nullptr, WM_COMMAND, IDM_ENABLED, 0);
    WndProc(nullptr, WM_COMMAND, IDM_FOCUS, 0);
    g_enabled = g_restoreFocus = FALSE;
    LoadSettings();
    Check(g_enabled && g_restoreFocus, "enabled preferences survive reload");

    HKEY key;
    Check(RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_SET_VALUE, &key)
          == ERROR_SUCCESS, "open isolated preferences");
    DWORD invalid = 2;
    RegSetValueExW(key, L"Enabled", 0, REG_DWORD,
                   (const BYTE*)&invalid, sizeof(invalid));
    const wchar_t text[] = L"false";
    RegSetValueExW(key, L"RestoreFocus", 0, REG_SZ,
                   (const BYTE*)text, sizeof(text));
    RegCloseKey(key);
    LoadSettings();
    Check(g_enabled && g_restoreFocus, "invalid values retain defaults");

    HKEY keyToRemove;
    Check(RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &keyToRemove)
          == ERROR_SUCCESS, "preferences were created");
    RegCloseKey(keyToRemove);
    HKEY run;
    Check(RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &run)
          == ERROR_FILE_NOT_FOUND, "saving preferences does not enable startup");
}

static void TestDisableCancelsQueuedWork() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TouchCursorFixRegressionWindow";
    Check(RegisterClassW(&wc) != 0, "register hidden test window");
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    Check(g_hwnd != nullptr, "create message-only test window");
    Check(PostMessageW(g_hwnd, WM_TELEPORT, 0, 0) != 0,
          "queue a teleport before disabling");
    g_enabled = TRUE;
    g_pending = g_parked = g_warped = g_timerOn = 1;
    g_snapUntil = g_guardUntil = g_suppressFgUntil = GetTickCount() + 3000;
    g_haveAnchor = g_snapDone = g_ungrabSent = TRUE;
    g_restoreFg = (HWND)1;
    WndProc(nullptr, WM_COMMAND, IDM_ENABLED, 0);
    Check(!g_enabled && !g_pending && !g_parked && !g_warped && !g_timerOn,
          "disable clears every pending input state");
    Check(!g_snapUntil && !g_guardUntil && !g_suppressFgUntil && !g_haveAnchor
          && !g_restoreFg, "disable discards stale anchors and foreground state");
    MSG queued;
    Check(!PeekMessageW(&queued, g_hwnd, WM_TELEPORT, WM_TELEPORT, PM_REMOVE),
          "disable removes posted teleports before they can survive re-enable");

    // A timer may already be queued when KillTimer is called. Even if stale
    // state is present, it must not restore focus, move the mouse or re-arm.
    g_pending = g_parked = g_warped = 1;
    g_haveAnchor = TRUE;
    g_lastTouchTick = GetTickCount() - RESTORE_MS - 1;
    const unsigned before = inputCalls;
    WndProc(nullptr, WM_TIMER, TIMER_ID, 0);
    Check(inputCalls == before && !g_pending && !g_parked && !g_warped
          && !g_haveAnchor && !g_snapUntil, "queued timer is inert while disabled");
    WndProc(nullptr, WM_TELEPORT, 0, 0);
    TouchSeen(GetTickCount(), "test");
    Check(!g_pending && !g_timerOn, "late touch triggers are inert while disabled");

    WndProc(nullptr, WM_COMMAND, IDM_ENABLED, 0);
    Check(g_enabled && g_hidx == 1 && !g_haveAnchor && !g_restoreFg,
          "re-enable seeds fresh tracking without reviving a touch sequence");
    DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

int main() {
    wchar_t keyPath[128];
    swprintf(keyPath, 128, L"Software\\TouchCursorFixRegression_%lu", GetCurrentProcessId());
    HKEY sandbox;
    Check(RegCreateKeyExW(HKEY_CURRENT_USER, keyPath, 0, nullptr, 0,
                          KEY_ALL_ACCESS, nullptr, &sandbox, nullptr)
          == ERROR_SUCCESS, "create temporary registry root");
    Check(RegOverridePredefKey(HKEY_CURRENT_USER, sandbox) == ERROR_SUCCESS,
          "isolate registry writes from user preferences");

    TestPreferences();
    TestDisableCancelsQueuedWork();

    Check(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) == ERROR_SUCCESS,
          "restore registry mapping");
    RegCloseKey(sandbox);
    Check(RegDeleteTreeW(HKEY_CURRENT_USER, keyPath) == ERROR_SUCCESS,
          "remove temporary test registry data");
    puts("PASS: preferences, disabled input state");
    return 0;
}
