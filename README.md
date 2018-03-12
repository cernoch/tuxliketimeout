Linux-like `timeout` for Windows
================================

I needed a tool for a Windows batch script to spawn
a child process and waited _at most X milliseconds_
for its completion. Since the “TIMEOUT + TASKKILL”
solution waits _exactly X milliseconds_, I created
my own, small solution.

Usage
-----

Instead of `ping google.com`, simply run
```
tuxliketimeout ping google.com
```

Compilation
-----------

Assuming you have **Visual Studio** and **CMake**
installed, first open `cmd.exe` and then:
```
cmake -Bbuild -H. -G "Visual Studio 15 2017 Win64"
cmake --build build
```
The binary will live in `build\Debug\tuxliketimeout.exe`.
