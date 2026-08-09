# Doxygen style

JohnSmith uses Javadoc-style Doxygen blocks for public interfaces and important
internal boundaries.

```c
/**
 * @brief Starts the selected virtualization backend on every active processor.
 *
 * @param[out] State Receives the initialized hypervisor state on success.
 * @return `STATUS_SUCCESS` on success; otherwise an initialization failure.
 * @warning Must run at `PASSIVE_LEVEL` with virtualization available.
 */
```

Use these tags when they add information: `@file`, `@brief`, `@details`,
`@param[in]`, `@param[out]`, `@param[in,out]`, `@return`, `@retval`, `@warning`,
and `@see`.

- Document ownership, lifetime, IRQL, synchronization, and partial-failure
  behavior. SAL remains the machine-readable contract; Doxygen explains it.
- Document public structures when their layout crosses C/assembly or
  kernel/user boundaries.
- Do not add author, date, or source-version banners; version control owns that
  history.
- Do not restate a parameter's C type or write empty `@details` sections.
- File comments describe the responsibility and boundary of the file, not its
  filename.

Run `tools/check-docs.ps1`. Pass `-RequireDoxygen` in CI or on machines where
Doxygen is part of the documented toolchain.
