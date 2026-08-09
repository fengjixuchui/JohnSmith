# Contributing

Keep changes narrow, architecture-backed, and warning-free.

Follow the repository's [coding style](docs/style/coding.md) and
[Doxygen style](docs/style/doxygen.md). Format only the source involved in a
change; do not combine repository-wide formatting with behavioral work.

## Before review

1. Build Debug, Release, and Benchmark x64.
2. Run WDK C/C++ analysis on Release.
3. Preserve synchronized all-CPU rollback and fail-stop teardown.
4. Validate VMCS/VMCB controls against capability bits.
5. Add complete handling before enabling a new intercept.
6. Add compile-time assertions for assembly-visible layouts.
7. Cite the manual revision and section for architecture changes.
8. Run the relevant bare-metal lifecycle and SLAT tests.

See [Build and test](docs/build-and-test.md) for the checklist and measurement evidence rules.

## Evidence

Hardware results must identify CPU model/stepping, Windows build, firmware, security state, driver configuration, service path, SHA-256, and measurement method. A build on one vendor does not prove runtime correctness on the other.

## Repository hygiene

- Do not commit build output, private signing keys, or crash dumps.
- Preserve unrelated worktree changes and vendored submodule state.
- Store only public, unmodified vendor documents or open-access papers.
- Add every archived source to [Reference catalog](docs/references.md) with revision, provenance, role, and SHA-256.
