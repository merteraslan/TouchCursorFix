# Regression tests

Build and run on Windows with the same MinGW-w64 toolchain as the app:

```powershell
g++ -std=c++11 -O2 -static tests/regression.cpp -o tests/regression.exe -luser32 -lshell32 -ladvapi32
.\tests\regression.exe
```

The console harness includes the application source but never calls its entry
point. It creates a message-only test window, but does not install input hooks
or create a tray icon. Registry tests
redirect HKCU to a temporary test key and remove that key on success; real
preferences and startup entries are not changed.
Input APIs are replaced by spies, so a failing assertion cannot move the real
cursor, release another application's cursor confinement, or steal focus.

Coverage: tray preference changes survive reloading; missing and malformed
preferences preserve the original defaults; saving preferences never opts into
running at startup; disabling cancels pending touch state and posted teleports; queued timers and late
touch triggers remain inert while disabled; re-enabling seeds fresh tracking.
Log path cases cover a short executable name whose longer log name crosses
`MAX_PATH`, truncated module paths, Unicode paths, and module-query failures.
