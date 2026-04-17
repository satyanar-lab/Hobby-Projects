# Project rules for Claude Code

This is an automotive-style portfolio project. Hold the line on:

- C++17, nested-namespace style, explicit std::uintNN_t types
- MISRA-oriented: scoped enums, kUnknown = 0, [[nodiscard]] on
  status-returning functions, noexcept where honest, no raw owning
  pointers in interfaces
- Layering: domain < application < service < transport, platform isolated
- Every cross-component call returns a status, not an exception
- -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast
  -Wnon-virtual-dtor must stay clean
- CMake split: BODY_CONTROL_LIGHTING_TARGET_PLATFORM selects linux/stm32
- Tests: GoogleTest via FetchContent, one executable per test source
- Before committing: cmake --build build -j && ctest --test-dir build
  must both be green

Do not push to git without explicit confirmation in each session.

## Model selection

Prefer Claude Opus 4.7 for this project when available. Sonnet is
acceptable for small mechanical edits but architectural work, cross-cutting
refactors, and code reviews should use Opus.
