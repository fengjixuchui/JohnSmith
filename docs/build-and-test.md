# Build and test

## Toolchain

- Visual Studio 2022 with Desktop C++.
- WDK `10.0.26100`.
- x64 Developer PowerShell or Developer Command Prompt.
- Bare-metal test system with a kernel debugger attached.

## Build configurations

| Configuration | Diagnostics | CPUID handler | VMCALL fast path |
| --- | --- | --- | --- |
| Debug | Enabled | C | Disabled |
| Release | Disabled | C | Disabled |
| Benchmark | Disabled | C | Enabled |

```powershell
msbuild .\JohnSmith.sln /m /p:Configuration=Release /p:Platform=x64
.\build\bin\tools\johnsmithctl.exe selftest
```

## Load the driver

JohnSmith is unsigned. `johnsmithctl` builds KDU when needed, disables Driver Signature Enforcement, creates and starts the service, then restores DSE.

```powershell
.\build\bin\tools\johnsmithctl.exe start --cpu 0
```

Record the printed seed. A later client must use the same seed while that driver instance runs. Stop and remove the service:

```powershell
.\build\bin\tools\johnsmithctl.exe stop
```

Use a disposable test system with Secure Boot, Hyper-V, VBS, and HVCI disabled. Do not terminate the loader while DSE is disabled.

## Verify the loaded artifact

A successful build does not prove that Windows loaded that build:

```powershell
sc.exe qc JohnSmith
Get-FileHash .\build\bin\Release\JohnSmith.sys -Algorithm SHA256
sc.exe query JohnSmith
```

Record the service path and SHA-256 with every runtime log. A stale service path invalidates code-to-log conclusions.

## VM-exit benchmark

```powershell
.\build\bin\tools\hv-benchmark.exe 200000
```

Use one Release artifact set on one bare-metal platform. Confirm JohnSmith is not running before you prepare the artifact set:

```powershell
sc.exe query JohnSmith
```

Do not proceed if the service reports `RUNNING`. Stop it, repeat the query, then copy the intended Release driver beside `johnsmithctl.exe`. Record the driver and benchmark artifacts before the five inactive samples:

```powershell
Copy-Item .\build\bin\Release\JohnSmith.sys .\build\bin\tools\JohnSmith.sys -Force
Get-FileHash .\build\bin\tools\JohnSmith.sys -Algorithm SHA256
Get-FileHash .\build\bin\tools\hv-benchmark.exe -Algorithm SHA256
1..5 | ForEach-Object { .\build\bin\tools\hv-benchmark.exe 200000 }
```

Start JohnSmith without installing or probing hooks, then run five active samples. Record the loaded service path and both hashes first:

```powershell
.\build\bin\tools\johnsmithctl.exe start --cpu 0
sc.exe qc JohnSmith
Get-FileHash .\build\bin\tools\JohnSmith.sys -Algorithm SHA256
Get-FileHash .\build\bin\tools\hv-benchmark.exe -Algorithm SHA256
1..5 | ForEach-Object { .\build\bin\tools\hv-benchmark.exe 200000 }
```

`BINARY_PATH_NAME` must resolve to `build\bin\tools\JohnSmith.sys`, and both hashes must match the artifacts from the inactive runs. Use the same platform for both sets. Any mismatch invalidates the comparison.

The median active CPUID leaf-0 `ratio(trim)` must be 10 or lower. CPUID leaf 16h is informational and is not judged against this gate, because the reference bare-metal leaf-16 ratio already exceeds 10.

## Transition floor

```powershell
msbuild .\JohnSmith.sln /m /p:Configuration=Benchmark /p:Platform=x64
.\build\bin\tools\johnsmithctl.exe start --cpu 0
.\build\bin\tools\hv-benchmark.exe 200000 --vmcall
```

`0xC000001D` is expected on bare metal and with Debug/Release. If it occurs with the intended Benchmark build, verify the service path and hash before you change the handler.

## Verification matrix

| Check | Scope | Build-only acceptable? |
| --- | --- | --- |
| Support probe | Intel and AMD | No |
| All-CPU launch and rollback | Intel and AMD | No |
| Live SLAT permission change | Intel INVEPT, AMD VMMCALL/TLB_CONTROL | No |
| CPUID policy | Intel and AMD | No |
| Execute-hook install/remove/query | Intel | No |
| Teardown and state restoration | Intel and AMD | No |
| Debug/Release/Benchmark compile | Solution | Yes |
| WDK analysis | Driver | Yes |

AMD compilation on an Intel system is not AMD runtime proof. Keep that boundary explicit in reviews and release notes.

## Intel rendezvous policy self-check

```powershell
cl /nologo /std:c17 /W4 /WX /TC /I .\src\intel `
  .\tools\intel-rendezvous-policy-selfcheck.c `
  /Fe:"$env:TEMP\johnsmith-rendezvous-policy-selfcheck.exe"
& "$env:TEMP\johnsmith-rendezvous-policy-selfcheck.exe"
```

The self-check covers policy classification, CPUID conditional behavior inside and outside the eight-exit window, exact budget behavior, excluded-exit budget preservation, ICR encoding, the required NMI-exiting and virtual-NMI pin-control bits, and the required TSC-offset primary-control bit. The hardware-only boundary is LAPIC delivery, NMI callback timing, VMCS writes, timeout release, and resume skew.

## Pull-request checklist

- [ ] Debug, Release, and Benchmark build without warnings.
- [ ] Release WDK analysis passes.
- [ ] Project XML and PowerShell scripts parse.
- [ ] `git diff --check` passes.
- [ ] Enabled intercepts have a complete handler and exception policy.
- [ ] Assembly-visible layouts have compile-time assertions.
- [ ] Manual revision and section are cited.
- [ ] Hardware results identify artifact, platform, and configuration.
