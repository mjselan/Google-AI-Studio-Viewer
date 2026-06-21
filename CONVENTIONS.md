--- START OF FILE CONVENTIONS.md ---
# C++20 and Qt6 Coding Conventions for AI Agent

You are an expert C++20 and Qt6 software engineer. Follow these strict conventions when writing or modifying code in this project:

## 1. Qt Core Architecture
- **MOC Rules:** ANY class that inherits from `QObject`, `QWidget`, `QMainWindow`, or any Qt model MUST declare the `Q_OBJECT` macro on the first line of the private section.
- **Header Guards:** NEVER use `#ifndef` header guards. ALWAYS use `#pragma once` at the top of every `.h` file.
- **Includes:** Only include what you use. Keep standard library includes (`#include <string>`) above Qt includes (`#include <QString>`).
- **Memory Management:** Use Qt's parent-child ownership tree for UI elements (`new QWidget(parent)`). Avoid naked `new` without a parent. Do not use `std::unique_ptr` for UI components that are managed by the Qt object tree.

## 2. Modern C++20 Standards
- Use `auto` for variable declarations when the type is obvious.
- Use override keyword explicitly: `~MyClass() override;`
- Pass large objects (like `QString`, `QList`) by `const reference`.
- Use braced initialization `{}` where appropriate.

## 3. UI and Layouts
- Never hardcode absolute sizes/positions. Always use `QVBoxLayout`, `QHBoxLayout`, `QGridLayout`, or `QSplitter`.
- Ensure widgets adapt dynamically to window resizing.

## 4. CMake Build System
- Do NOT invoke `moc`, `uic`, or `rcc` manually.
- Assume the root `CMakeLists.txt` already contains:
  `set(CMAKE_AUTOMOC ON)`
  `set(CMAKE_AUTOUIC ON)`
  `set(CMAKE_AUTORCC ON)`

## 5. Output Format
- DO NOT apologize or explain the code.
## Build Commands

## 6. CLI (from pwsh or cmd)
```cmd
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake --preset debug
cmake --build --preset debug
--- END OF FILE CONVENTIONS.md ---