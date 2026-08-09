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
```

Inspect the local virtualization and Windows security state without changing
it:

```powershell
.\tools\check-environment.ps1
```

Verify the extracted Intel hypercall protocol IDs from the same shell:

```powershell
cl /nologo /W4 /WX /TC .\tests\HypercallProtocolTests.c `
  /Fe:.\build\bin\HypercallProtocolTests.exe
.\build\bin\HypercallProtocolTests.exe
```

After changing an assembly entry path, inspect the generated object code and
the linked unwind record:

```powershell
dumpbin /symbols .\build\obj\Release\IntelAssembly.obj
dumpbin /disasm .\build\obj\Release\HookDispatch.obj
dumpbin /unwindinfo .\build\bin\Release\JohnSmith.sys
```

Confirm stack offsets, transfer instructions, and the `AsmHookDispatcher`
frame before review. These checks are host-only and must not load the driver.

Run the documentation-link check before review:

```powershell
.\tools\check-docs.ps1
```

`JohnSmith.sln` builds only the driver. The companion `JohnSmithCtl` repository
must be checked out beside this repository when building or loading the control
utility.

## Load the driver

JohnSmith is unsigned. The companion
[JohnSmithCtl](https://github.com/meowdiocre/johnsmithctl) manages the service
and its configured driver-loading workflow. Manual loading requires an
appropriately configured disposable Windows test system.

```powershell
Copy-Item .\build\bin\Release\JohnSmith.sys ..\JohnSmithCtl\build\bin\Release\JohnSmith.sys -Force

johnsmithctl.exe start --cpu 0
```

Record the printed seed. A later client must use the same seed while that
driver instance runs. Stop and remove the service:

```powershell
johnsmithctl.exe stop
```
