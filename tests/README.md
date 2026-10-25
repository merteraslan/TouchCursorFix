# Regression tests

Build and run on Windows with the same MinGW-w64 toolchain as the app:

```powershell
g++ -std=c++11 -O2 tests/regression.cpp -o tests/regression.exe -luser32 -lshell32 -ladvapi32
.\tests\regression.exe
```

The console harness includes the application source but never calls its entry
point. It does not install input hooks or create a tray icon. Registry tests
redirect HKCU to a temporary test key and remove that key on success; real
preferences and startup entries are not changed.

Coverage: tray preference changes survive reloading; missing and malformed
preferences preserve the original defaults; saving preferences never opts into
running at startup.
