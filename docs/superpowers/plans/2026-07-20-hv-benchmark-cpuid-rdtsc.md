# HV Benchmark CPUID/RDTSC Implementation Plan

**Goal:** Integrate the user-mode CPUID/RDTSC timing routine into the existing benchmark and rename that benchmark target to `hv-benchmark` without moving the other monorepo projects.

**Architecture:** Keep `johnsmithctl` and the renamed benchmark as separate applications under `tools/` in the existing `JohnSmith.sln`. Add one intrinsic-only timing helper and one deterministic arithmetic self-check to the benchmark C++ harness. Preserve all existing MASM probes and driver benchmark controls.

**Tech stack:** C++20, MSVC x64 intrinsics, MASM, Visual Studio/MSBuild, PowerShell.

## File structure

- Modify then rename `tools/vmexit-bench/benchmark.cpp` to `tools/hv-benchmark/benchmark.cpp`: add the timing calculation, measurement helper, self-check command, and output.
- Rename `tools/vmexit-bench/ops.asm` to `tools/hv-benchmark/ops.asm`: path-only move; assembly behavior stays unchanged.
- Rename and modify `tools/vmexit-bench/vmexit-bench.vcxproj` to `tools/hv-benchmark/hv-benchmark.vcxproj`: update project identity strings and output name while retaining its GUID.
- Modify `JohnSmith.sln`: point the existing benchmark project GUID at the renamed project.
- Modify `docs/build-and-test.md`: update executable commands and hashes.
- Modify `docs/implementation-status.md`: update the benchmark path and name.
- Delete local ignored file `build/bin/tools/test.c`: remove the superseded experiment without adding a tracked deletion.

The driver property `JohnSmithVmexitBenchmark` and macro `JOHNSMITH_VMEXIT_BENCHMARK` stay unchanged because they describe the VM-exit-specific driver build behavior, not the application project name.

## Tasks

1. **Define and test adjusted timing arithmetic.** Add `AverageAdjustedTiming` and a `--selfcheck` path that verifies the signed average-adjustment calculation. Build to verify the contract fails, add the helper, then build and run the self-check.

2. **Add the user-mode measurement.** Add `CpuidRdtscTiming` and `MeasureCpuidRdtscTiming`: 100 iterations of `RDTSC; CPUID(1); RDTSC`, 100 back-to-back RDTSC pairs, integer averages, and a signed adjusted result. Print the raw CPUID average, raw RDTSC overhead average, and adjusted average before the existing probe table.

3. **Rename the benchmark project.** Rename the directory, project file, and solution entry. Update root namespace, intermediate directory, and output name. Preserve the project GUID. Verify the normal benchmark output includes the new timing line and the existing probe table.

4. **Update documentation and verify.** Update `docs/build-and-test.md` and `docs/implementation-status.md` with the new executable name and commands. Run `git diff --check`.

## Validation

- `hv-benchmark.exe --selfcheck` exits `0`.
- The normal benchmark prints the new timing line and the existing probe table.
- Debug, Release, and Benchmark x64 build without warnings.
- `git diff --check` passes.
