# JohnSmith

Windows x64 research hypervisor. Intel VT-x/EPT and AMD-V/SVM/NPT backends for isolated red-team experiments.

![JohnSmith banner](static/img/main.png)

> [!WARNING]
> Experimental. Not for production use.

## Capabilities

| Area | What it does |
| --- | --- |
| Lifecycle | All-CPU launch, rollback, and teardown |
| Intel | VMX, EPT, VPID, VMCS control validation, CPUID policy |
| Intel hooks | Per-vCPU dual-EPT execute hooks with observation counters |
| AMD | SVM/NPT and masked CPUID policy |
| Control | CPUID/shared-page hypercall transport and `johnsmithctl` |
| Memory | 512 GiB identity-SLAT ceiling, runtime 4 KiB permission changes |
| State | CR0/CR3/CR4, debug state, PAT/EFER, MSR bitmaps |
| Diagnostics | Fail-stop bugchecks, Debug-only VM-exit history |
| Measurement | Cross-core software-clock VM-exit benchmark |

Intel and AMD hide vendor-virtualization features from the guest. Execute hooks and `johnsmithctl` control are Intel-only.

## Documentation

| Document | Covers |
| :--- | :--- |
| [Implementation status](docs/implementation-status.md) | Code-to-claim matrix and known blockers |
| [Intel VMX/EPT](docs/architecture/intel-vmx.md) | VMCS, exits, EPT, VPID, CET |
| [AMD SVM/NPT](docs/architecture/amd-svm.md) | VMCB, exits, NPT, ASIDs |
| [Build and test](docs/build-and-test.md) | Build, load, and verify the driver |
| [References](docs/references.md) | Manuals, papers, revisions, hashes |

## License

[MIT](LICENSE)
