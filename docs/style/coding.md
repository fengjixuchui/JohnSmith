# Coding style

JohnSmith uses a small, explicit C style derived from the Windows Driver Kit
and the useful parts of HyperDbg's conventions. The checked-in
`.clang-format` file is authoritative for whitespace.

## Naming

- Use `PascalCase` for functions and WDK-style types.
- Use `camelCase` for local variables and parameters.
- Prefix writable process-wide globals with `g_`.
- Use uppercase words separated by underscores for C enums, structures, and
  compile-time macros when they form part of a kernel interface.
- Keep Intel and AMD architecture terminology identical to the vendor manuals.
- Use `PascalCase` for kernel `.c`, `.h`, and `.asm` filenames, lowercase names
  for ownership directories, and lowercase hyphenated names for documentation
  and PowerShell tools.

Do not rename existing interfaces solely to satisfy a naming preference. Keep
mechanical renames separate from behavioral changes.

## Layout

- Place code under the narrowest owning subsystem. Public interfaces belong in
  `include/johnsmith`; backend-private code belongs under `src/arch`; hooks do
  not belong in the common lifecycle layer.
- Public headers expose only cross-module contracts. Keep implementation
  records, assembly glue, and backend helpers in private headers beside their
  owning source files.
- Split a function when it mixes policy, lifecycle, I/O, or architecture
  responsibilities. File length alone is not a reason to separate cohesive
  hardware logic.
- Use four spaces and no tabs.
- Keep lines at or below 100 columns when practical.
- Put top-level C return types on their own line, matching the existing driver.
- Use braces for every multi-line control statement. Do not place an `if`, loop,
  or its body on one line.
- Keep SAL annotations on exported and cross-module kernel interfaces.
- Sort neither platform nor project includes automatically; dependency order
  can be significant in WDK builds.

Format only files or lines involved in a change. Repository-wide formatting is
a dedicated mechanical change with its own review.

## Comments and errors

Comments explain invariants, architecture requirements, ownership, IRQL, and
why a non-obvious decision is safe. Do not narrate the statement immediately
below the comment, preserve disabled code in comments, speculate, or use
informal nicknames. Prefer a precise name or a small helper when the code can
explain itself.

Use Doxygen on public headers and cross-module contracts. Keep implementation
comments local and short unless they document a hardware rule or concurrency
invariant that cannot be expressed in code.

Kernel errors must preserve the original `NTSTATUS`. Logs identify the
subsystem and operation; they do not encode the source through capitalization.

## Assembly

- Begin MASM sources with `option casemap:none` and group constants by the
  hardware or frame contract they describe.
- Name stack-frame offsets and shared sizes. Use the common x64 entry macros
  when Intel and AMD require byte-for-byte identical save sequences.
- Keep simple hardware helpers explicit. A macro must remove meaningful
  duplication without hiding instruction ordering that reviewers need to
  audit.
- Comments document ABI obligations, register ownership, hardware flags, and
  fault or unwind behavior. They do not translate each instruction into prose.
- Back every assembly-visible C structure with compile-time size and offset
  assertions. Record cross-file stack contracts in the
  [assembly ABI contracts](../architecture/assembly-abi.md).

## Verification

Every change must build Debug, Release, and Benchmark x64 without warnings.
Architecture changes also follow the evidence requirements in
`docs/build-and-test.md` and `CONTRIBUTING.md`.
