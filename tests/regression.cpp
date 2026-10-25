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

    Check(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) == ERROR_SUCCESS,
          "restore registry mapping");
    RegCloseKey(sandbox);
    Check(RegDeleteTreeW(HKEY_CURRENT_USER, keyPath) == ERROR_SUCCESS,
          "remove temporary test registry data");
    puts("PASS: preferences");
    return 0;
}
