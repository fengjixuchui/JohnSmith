# JohnSmith

Windows x64 research hypervisor. Intel VT-x/EPT and AMD-V/SVM/NPT backends for isolated red-team experiments.

![JohnSmith banner](docs/images/main.png)

> [!WARNING]
> Experimental. Not for production use.

## Capabilities

| Area | What it does |
| --- | --- |
| Lifecycle | All-CPU launch, rollback, and teardown |
| Intel | VMX, EPT, VPID, VMCS control validation, CPUID policy |
| Intel hooks | Per-vCPU dual-EPT execute hooks with observation counters |
| AMD | SVM/NPT and masked CPUID policy |
| Control | CPUID/shared-page hypercall transport for the companion [JohnSmithCtl](https://github.com/meowdiocre/johnsmithctl) client |
| Memory | 512 GiB identity-SLAT ceiling, runtime 4 KiB permission changes |
| State | CR0/CR3/CR4, debug state, PAT/EFER, MSR bitmaps |
| Diagnostics | Fail-stop bugchecks, Debug-only VM-exit history |
| Measurement | Cross-core software-clock VM-exit benchmark |

Intel and AMD hide vendor-virtualization features from the guest. Execute hooks and `JohnSmithCtl` control are Intel-only.

## Repository layout

| Path | Ownership |
| --- | --- |
| `include/johnsmith/` | Public driver interfaces |
| `src/core/` | Driver lifecycle, common orchestration, logging, and introspection |
| `src/arch/x86/` | Vendor-neutral x86 helpers |
| `src/arch/intel/` | Intel VMX/EPT backend and assembly |
| `src/arch/amd/` | AMD SVM/NPT backend and assembly |
| `src/hooks/` | Hook observation, thunks, trampolines, and assembly |
| `tests/` | Host-only regression and baseline checks |
| `docs/` | Architecture notes, style rules, images, and source references |
| `tools/` | Repository and environment checks |

## Documentation

| Document | Covers |
| :--- | :--- |
| [Intel VMX/EPT](docs/architecture/intel-vmx.md) | VMCS, exits, EPT, VPID, CET |
| [AMD SVM/NPT](docs/architecture/amd-svm.md) | VMCB, exits, NPT, ASIDs |
| [Assembly ABI](docs/architecture/assembly-abi.md) | Entry frames, register ownership, unwind contracts |
| [Build and test](docs/build-and-test.md) | Build, load, and verify the driver |
| [References](docs/references.md) | Manuals, papers, revisions, hashes |
| [Coding style](docs/style/coding.md) | Naming, formatting, comments, and verification |
| [Doxygen style](docs/style/doxygen.md) | Source documentation conventions |

## License

[MIT](LICENSE)

## Related research

- [johnsmithctl](https://github.com/meowdiocre/johnsmithctl): companion user-mode client and loader.
- [HvD](https://github.com/meowdiocre/HvD): user-mode and kernel measurement harness.
- [vmw](https://github.com/meowdiocre/vmw): KVM/Windows research workspace.
