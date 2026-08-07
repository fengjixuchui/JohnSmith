# Modular hv-benchmark Timers Implementation Plan

**Goal:** Turn `hv-benchmark.exe` into three independently selectable timer modules with detector-aligned gates, modular output, continued execution after module-specific setup errors, and no `--selfcheck` command.

**Architecture:** Keep production code in `tools/hv-benchmark/benchmark.cpp`. Add one standalone C++ test translation unit that includes the production source with `main` renamed, allowing direct tests of static CLI, gate, outcome, and panel helpers without adding a test framework or restoring a runtime self-check mode. Preserve `ops.asm` unchanged.

**Tech stack:** C++20, Windows API, MSVC v143, MASM x64 probes, PowerShell smoke checks, assert-based standalone tests.

## File structure

- Modify `tools/hv-benchmark/benchmark.cpp`: shared models, CLI parser, gates, panel formatter, three timer modules, execution orchestration, and exit aggregation.
- Create `tools/hv-benchmark/benchmark-tests.cpp`: runnable assert-based tests for deterministic logic and the two short/non-destructive timer result shapes.
- Modify `tools/hv-benchmark/hv-benchmark.vcxproj`: compile source literals as UTF-8.
- Modify `docs/build-and-test.md`: document modular commands, the `2.5` software-tick gate, exact TSC-exit semantics, and VMCALL usage.
- Verify `tools/hv-benchmark/ops.asm`: assembly probes stay byte-for-byte unchanged.

## Tasks

1. **Add tested shared CLI, gate, outcome, and panel primitives.** Create `benchmark-tests.cpp` with `#define main HvBenchmarkProgramMain` then `#include "benchmark.cpp"`. Write failing tests for CLI parsing, the software-tick ratio gate (`FAIL iff leaf0.trimmedMean / serialize.trimmedMean >= 2.5`), `CombineOutcome` exit-code precedence, and `PrintPanel` row formatting. Add the minimal production declarations to make the tests compile and the cases fail. Then implement each helper to green.

2. **Isolate TSC-CPUID and add exact TSC-exit measurement.** Move `MeasureCpuidRdtscTiming` into `RunTscCpuidTimer`. Add `RunTscExitTimer`: ten samples of `RDTSC; CPUID(0); RDTSC; Sleep(500)`, unsigned 64-bit accumulation, integer division, gate `PASS iff 0 < average < 1000`. No serialization, baseline subtraction, trim, or outlier removal. Write tests for the TSC-exit gate and the TSC-CPUID adjusted-timing shape.

3. **Modularize software-tick and replace main orchestration.** Move the existing two-thread design into `RunSoftwareTickTimer`. Add `ParseOptions` to produce a module bitmask from the CLI. Replace `main` with orchestration that runs selected modules in fixed order (software-tick, TSC-exit, TSC-CPUID), collects outcomes, and returns `CombineOutcome`. A module-specific setup failure records an error outcome and does not stop other selected modules. Remove `--selfcheck`.

4. **Enable UTF-8 compilation and update documentation.** Add the UTF-8 compiler option to the vcxproj. Call `SetConsoleOutputCP(CP_UTF8)` before printing. Update `docs/build-and-test.md` with the new CLI, the `2.5` software-tick gate, exact TSC-exit semantics, and VMCALL usage. Remove documentation that instructs users to run `--selfcheck`.

5. **Full verification and diff audit.** Build Release with warnings as errors. Run `--tsc-cpuid` and confirm no clock thread requirement. Run `--tsc-exit` and confirm ten samples with ten 500 ms sleeps and the `0 < average < 1000` result. Run `10000 --software-tick --plain` and confirm full statistics, ratio gate, and report-only tripwires. Run a combined selection and confirm all panels print even when a gated module fails. Run invalid CLI cases and verify code `2`. On the target host with the Benchmark driver loaded, run software-tick with `--vmcall` and record the VMCALL floor without gating it. Run `git diff --check`.

## Exit codes

| Code | Meaning |
|---:|---|
| `0` | Every gated module that ran passed; no setup errors |
| `1` | At least one gated module failed; no setup errors |
| `2` | Invalid CLI |
| `3` | Insufficient CPU topology |
| `4` | Test-thread affinity or priority setup failed |
| `5` | Software-tick clock-thread affinity or priority setup failed |
| `6` | Required CPU capability unavailable |
| `7` | Requested or required probe raised an exception |

Setup errors override gated failures. If multiple setup errors occur, return the first setup error encountered in the fixed module order.
