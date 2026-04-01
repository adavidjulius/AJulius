# JULIUS OS

JULIUS OS is an advanced-purpose operating system built on the XNU kernel, with a fully custom user-space.

## Goals

* Replace all Apple user-space components
* Build a minimal and controllable system from scratch
* Develop a custom libc and init system
* Support future custom hardware platforms

## Architecture

* Kernel: XNU (modified)
* User-space: Fully custom (no libSystem, no launchd)
* IPC: Mach (wrapped by JULIUS APIs)

## Project Structure

* kernel/   → XNU source and patches
* libc/     → Minimal C standard library
* init/     → First user-space process (PID 1)
* build/    → Build scripts
* tools/    → Debugging and utilities
* docs/     → Design and architecture notes

## Current Status

🚧 Bootstrapping phase — setting up build system and minimal kernel
