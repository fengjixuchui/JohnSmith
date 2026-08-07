# Security policy

JohnSmith is experimental kernel and virtualization code. It is not hardened for hostile guests or production deployment.

## Reporting

Report vulnerabilities privately through GitHub Security Advisories. Include:

- CPU vendor/model and Windows build.
- Firmware, Hyper-V, VBS, HVCI, and CET state.
- Driver configuration, service path, and SHA-256.
- Reproduction steps and expected vs. actual result.
- VM-exit reason, qualification, and bugcheck parameters when available.

Do not publish working kernel exploitation details before a fix exists. Do not attach private signing keys or crash dumps that contain secrets.

## Supported boundary

The project targets controlled research. It does not claim isolation against a malicious guest, side-channel resistance, nested-virtualization security, supervisor-CET support, or production-grade device isolation.
