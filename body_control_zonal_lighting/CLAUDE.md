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

## Session restart instructions

When starting a new session, always:
1. Run git status and git log --oneline -5 first
2. Read CHANGES_PHASE2.md and any doc/ files modified recently
3. Report current state before making any changes
4. Never assume the previous session's context — reconstruct from files

## Model selection policy

Switch models automatically based on task type. Do not ask — just switch.

Use Sonnet for:
- Compiling and running build commands
- Fixing compiler errors and warnings
- Renaming fields, updating field references across files
- Writing or updating CMakeLists entries
- Adding files to git, staging, committing
- Running ctest and reporting results
- Updating doc/*.md and CHANGES_*.md files
- Any task that is mechanical, repetitive, or clearly defined

Use Opus for:
- Designing a new interface or abstract class
- Deciding how two components should be wired together
- Reviewing code for architectural correctness
- Any task where the right answer is not obvious
- Catching subtle bugs (wrong predicate, init order, type mismatch)
- Writing a new test that requires understanding the contract being tested

When in doubt, start with Sonnet. If it gets confused or produces
something wrong, switch to Opus for that step only, then switch back.
