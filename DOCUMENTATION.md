# Documentation

Snapshot: **2026-07-19**.

## Start here

| Topic | Document |
| --- | --- |
| Code-to-claim status | [Implementation status](docs/implementation-status.md) |
| Intel virtualization | [VMX/EPT architecture](docs/architecture/intel-vmx.md) |
| AMD virtualization | [SVM/NPT architecture](docs/architecture/amd-svm.md) |
| Build, load, verify | [Build and test](docs/build-and-test.md) |
| Source provenance | [Reference catalog](docs/references.md) |
| Contribution rules | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Security reporting | [SECURITY.md](SECURITY.md) |

## Evidence policy

Rank sources in this order:

1. Intel SDM, AMD APM, and WDK headers for architectural or DDI requirements.
2. Vendor optimization manuals for microarchitectural guidance.
3. Hardware measurements from the exact binary, CPU, and Windows build.
4. Research papers for design context only.

Each architectural claim must cite the document revision and section or table title. A clean build does not prove successful VM entry or correct behavior on untested hardware.

## Repository map

```
asm/        VM-entry, VM-exit, and state-transition assembly
include/    Public and assembly-facing contracts
src/common/ Vendor-neutral x86 validation and range helpers
src/intel/  VMCS, VM-exit, and EPT implementation
src/amd/    VMCB, VM-exit, and NPT implementation
src/hv.c    Backend selection and all-CPU lifecycle
tools/      Control client, loader, and benchmark
docs/       Design and operating documentation
static/docs/ Pinned manuals and research papers
```

## Review rules

- Validate optional controls against capability MSRs or CPUID before use.
- Keep C layout assertions for every assembly-visible VMCS/VMCB field.
- Add an exit handler before enabling a new intercept.
- Preserve all-CPU rollback, SLAT invalidation, and fail-stop teardown.
- Separate architectural requirements from project policy and measured facts.
- Record CPU model, firmware, Windows build, configuration, service path, driver hash, sample count, and statistic for every performance result.
