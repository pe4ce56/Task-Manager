# Task Manager

A small single-user Windows task manager built with C++17 and Qt Widgets. Tasks are kept on this PC in a JSON file; the app does not need an account or network connection.

## Requirements

- Windows 10 or 11, 64-bit
- Qt 6.5 or newer, with the **MinGW 64-bit** desktop kit and Qt Test component
- CMake 3.21 or newer
- The MinGW compiler shipped for the selected Qt kit

The commands below use Qt 6.11.2 and MinGW 13.1 as an example. In the Qt installer, select the matching Qt version's `MinGW 64-bit` component and `Qt Test`, plus the CMake and Ninja tools. Replace the paths with the locations chosen in the installer.

## Configure and build

Open PowerShell in the repository root and set the tool paths:

```powershell
$env:QtRoot = 'C:\Qt\6.11.2\mingw_64'
$env:MinGWRoot = 'C:\Qt\Tools\mingw1310_64'
$env:Path = "$env:MinGWRoot\bin;$env:QtRoot\bin;$env:Path"
```

Configure and compile both the app and tests:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="$env:QtRoot" -DCMAKE_MAKE_PROGRAM="$env:MinGWRoot\bin\mingw32-make.exe" -DBUILD_TESTING=ON
cmake --build build --parallel
```

## Run

```powershell
./build/TaskManager.exe
```

Use **Add task**, double-click a row to edit it, click its checkbox to complete or reopen it, and select a row before choosing **Delete**. `Ctrl+N` adds a task, `Enter` edits the selected task, and `Delete` removes it after confirmation. Search checks titles and notes as you type. The status menu filters all, active, or completed tasks.

Tasks are stored in `%LOCALAPPDATA%\TaskManager\tasks.json`. The file is written atomically. If it cannot be read or written, the app reports the problem and preserves the existing data. For a portable copy, close the app and back up that file.

## Test

```powershell
ctest --test-dir build --output-on-failure
```

Tests cover task operations, blank-title validation, search and status filtering, persistence after reopening the store, preservation of data when a task file is malformed or cannot be written, and a Qt window workflow through its add, edit, completion, filtering, and delete controls.
