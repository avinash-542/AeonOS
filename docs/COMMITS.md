# AeonOS Commit Convention

Format: <type>(<scope>): <description>

## Types
- feat     → new feature or capability
- fix      → bug fix
- build    → Makefile, Docker, toolchain
- boot     → bootloader changes
- kernel   → core kernel changes
- driver   → hardware drivers
- docs     → documentation
- chore    → cleanup, no logic change
- test     → tests
- refactor → restructure, no behavior change

## Scopes (examples)
- kernel, boot, driver, mm, scheduler, uart, fs, lib

## Examples
feat(kernel): add basic interrupt descriptor table
fix(boot): align stack to 16-byte boundary
build(docker): pin GCC version to 13.x
docs(readme): add QEMU boot instructions
