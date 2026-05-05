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
- Before every commit: run git status, confirm staged files match only
  the current task. If unrelated files are staged (e.g. platform files
  mixed with HMI files), unstage them with git restore --staged before
  committing. Never mix changes from different layers or components in
  one commit.

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

## Advisor strategy — mandatory for all remaining work

You are the Executor running on Sonnet. You run every turn.

You have access to an Advisor (Opus) via the Task tool.
Call the Advisor ONLY for:
- Designing a new interface or abstract class
- Deciding how two components should be wired together  
- Architectural decisions with non-obvious tradeoffs
- Catching subtle bugs where the root cause is unclear
- Any decision where you are not confident in the correct answer

Do NOT call the Advisor for:
- Building, compiling, running tests
- Renaming fields or updating references
- Writing CMakeLists entries
- Git operations
- Updating documentation
- Any task with a clear mechanical answer

When calling the Advisor:
- Pass only the minimal context needed — not the full conversation
- Ask one specific question — not "what should I do generally"
- Apply the advice and continue without asking again

This saves tokens and gets better results on hard problems.

## Known-good baseline tags

**zephyr-fully-working-v1** — All Zephyr lamp behavior
verified on hardware. If any future change breaks
Zephyr functionality, restore from this tag:
  git checkout zephyr-fully-working-v1 -- \
    app/zephyr_nucleo_h753zi/ \
    src/platform/zephyr/

Always verify against this tag before commits that
touch Zephyr files.

**stm32-fully-working-v1** — All STM32 bare-metal lamp
behavior verified on hardware with new pin map matching
Zephyr devicetree. If any future change breaks STM32
functionality, restore from this tag:
  git checkout stm32-fully-working-v1 -- \
    app/stm32_nucleo_h753zi/ \
    src/platform/stm32/ \
    include/body_control/lighting/platform/stm32/

Always verify against this tag before commits that
touch STM32 files.

**mcuboot-ota-zephyr-fully-working-v1** — MCUboot OTA
on Zephyr verified end-to-end on hardware (Phase 13).
Covers: ECDSA-P256 signing, dual-bank flash partitions,
swap-using-move, UDS/DoIP transfer, stream_flash write,
boot_request_upgrade, and boot_write_img_confirmed.
Pre-OTA: BCL-EXTERIOR-NODE 1.0.0 → Post-OTA: 1.0.1.
If any future change breaks OTA functionality, restore:
  git checkout mcuboot-ota-zephyr-fully-working-v1 -- \
    app/zephyr_nucleo_h753zi/ \
    src/application/ota_session_manager_zephyr.cpp \
    src/application/uds_request_handler.cpp \
    src/application/ota_handler.cpp

Always verify against this tag before commits that
touch OTA or MCUboot files.

**exterior-rename-fully-working-v1** — Exterior_lighting
rename fully verified on STM32 hardware (Phase 14).
OEM-accurate naming: ExteriorLightingService,
exterior_lighting_node, BCL-EXTERIOR-NODE v1.0.2.
Wire protocol unchanged from prior tags.
If any future change introduces old rear_lighting names,
restore:
  git checkout exterior-rename-fully-working-v1 -- \
    app/ src/ include/ test/ arxml/ config/ doc/

Always verify against this tag after any broad rename
or refactor touching service/class names.
