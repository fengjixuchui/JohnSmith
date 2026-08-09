#pragma once

#include <stddef.h>

#define INTEL_HYPERCALL_SUBLEAF_TAG          "sub"
#define INTEL_HYPERCALL_REGISTER_SUBLEAF_TAG "srg"

#define INTEL_FNV1A_OFFSET 0x811c9dc5u
#define INTEL_FNV1A_PRIME  0x01000193u

typedef enum _INTEL_HYPERCALL_CMD {
    INTEL_HYPERCALL_CMD_REGISTER = 0,
    INTEL_HYPERCALL_CMD_INSTALL,
    INTEL_HYPERCALL_CMD_REMOVE,
    INTEL_HYPERCALL_CMD_READ,
    INTEL_HYPERCALL_CMD_WRITE,
    INTEL_HYPERCALL_CMD_QUERY_HOOK,
    INTEL_HYPERCALL_CMD_LIST_HOOKS,
    INTEL_HYPERCALL_CMD_PROBE,
    INTEL_HYPERCALL_CMD_COUNT
} INTEL_HYPERCALL_CMD;

static inline unsigned long
IntelHypercallFnv1a(
    unsigned long Seed,
    const char* Tag
    )
{
    unsigned long hash = Seed ^ INTEL_FNV1A_OFFSET;

    while (*Tag != '\0') {
        hash ^= (unsigned char)*Tag++;
        hash *= INTEL_FNV1A_PRIME;
    }
    return hash;
}

static inline const char*
IntelHypercallCommandTag(
    INTEL_HYPERCALL_CMD Command
    )
{
    switch (Command) {
    case INTEL_HYPERCALL_CMD_REGISTER:
        return "reg";
    case INTEL_HYPERCALL_CMD_INSTALL:
        return "inst";
    case INTEL_HYPERCALL_CMD_REMOVE:
        return "rem";
    case INTEL_HYPERCALL_CMD_READ:
        return "read";
    case INTEL_HYPERCALL_CMD_WRITE:
        return "writ";
    case INTEL_HYPERCALL_CMD_QUERY_HOOK:
        return "quer";
    case INTEL_HYPERCALL_CMD_LIST_HOOKS:
        return "list";
    case INTEL_HYPERCALL_CMD_PROBE:
        return "prob";
    default:
        return NULL;
    }
}
