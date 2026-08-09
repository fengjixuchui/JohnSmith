#include <string.h>

#include "../src/arch/intel/IntelHypercallProtocol.h"

typedef struct _HYPERCALL_COMMAND_CASE {
    INTEL_HYPERCALL_CMD Command;
    const char* Tag;
    unsigned long ExpectedId;
} HYPERCALL_COMMAND_CASE;

int
main(
    void
    )
{
    static const unsigned long seed = 0x12345678u;
    static const HYPERCALL_COMMAND_CASE cases[] = {
        {INTEL_HYPERCALL_CMD_REGISTER, "reg", 0x8A13FEDDu},
        {INTEL_HYPERCALL_CMD_INSTALL, "inst", 0xC6E52D51u},
        {INTEL_HYPERCALL_CMD_REMOVE, "rem", 0x8013EF1Fu},
        {INTEL_HYPERCALL_CMD_READ, "read", 0xFA6AC69Du},
        {INTEL_HYPERCALL_CMD_WRITE, "writ", 0x950016B9u},
        {INTEL_HYPERCALL_CMD_QUERY_HOOK, "quer", 0xA6701220u},
        {INTEL_HYPERCALL_CMD_LIST_HOOKS, "list", 0xFC225B19u},
        {INTEL_HYPERCALL_CMD_PROBE, "prob", 0x407F55B6u}
    };
    size_t index;

    if (IntelHypercallFnv1a(
            seed, INTEL_HYPERCALL_SUBLEAF_TAG) != 0x226DB25Du) {
        return 1;
    }
    if (IntelHypercallFnv1a(
            seed, INTEL_HYPERCALL_REGISTER_SUBLEAF_TAG) != 0x1D5C615Du) {
        return 2;
    }

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const char* tag = IntelHypercallCommandTag(cases[index].Command);

        if (tag == NULL || strcmp(tag, cases[index].Tag) != 0) {
            return 10 + (int)index;
        }
        if (IntelHypercallFnv1a(seed, tag) != cases[index].ExpectedId) {
            return 20 + (int)index;
        }
    }

    if (IntelHypercallCommandTag(INTEL_HYPERCALL_CMD_COUNT) != NULL) {
        return 30;
    }
    return 0;
}
