#include "HookTrampoline.h"

#define HOOK_REL32_MIN (-2147483647LL - 1LL)
#define HOOK_REL32_MAX 2147483647LL

#define HOOK_TRAMPOLINE_MAX_ORIGINAL   32u
#define HOOK_TRAMPOLINE_JMP_SIZE       14u
#define HOOK_TRAMPOLINE_ALLOC          (HOOK_TRAMPOLINE_MAX_ORIGINAL + \
                                        HOOK_TRAMPOLINE_JMP_SIZE)

typedef struct _HOOK_DECODER {
    const UCHAR* Start;
    ULONG Offset;
    ULONG Length;
} HOOK_DECODER;

static UCHAR
HookDecPeek(
    _In_ const HOOK_DECODER* Decoder
    )
{
    return Decoder->Start[Decoder->Offset];
}

static UCHAR
HookDecRead(
    _Inout_ HOOK_DECODER* Decoder
    )
{
    UCHAR value = Decoder->Start[Decoder->Offset];

    ++Decoder->Offset;
    return value;
}

static BOOLEAN
HookDecHasBytes(
    _In_ const HOOK_DECODER* Decoder,
    _In_ ULONG Count
    )
{
    return Decoder->Offset <= Decoder->Length &&
           Count <= Decoder->Length - Decoder->Offset;
}

typedef struct _HOOK_INSTR_INFO {
    ULONG Length;
    BOOLEAN RipRelative;
    ULONG RipDisplacementOffset;
} HOOK_INSTR_INFO;

static BOOLEAN
HookDecConsumeModRmTail(
    _Inout_ HOOK_DECODER* Decoder,
    _In_ UCHAR ModRm,
    _In_ ULONG InstructionStartOffset,
    _Out_ BOOLEAN* RipRelative,
    _Out_ ULONG* RipDisplacementOffset
    )
{
    UCHAR mod = (UCHAR)(ModRm >> 6);
    UCHAR rm = (UCHAR)(ModRm & 7);

    *RipRelative = FALSE;
    *RipDisplacementOffset = 0;

    if (mod == 3) {
        return TRUE;
    }

    if (rm == 4) {
        UCHAR sib;
        UCHAR base;
        if (!HookDecHasBytes(Decoder, 1)) {
            return FALSE;
        }
        sib = HookDecRead(Decoder);
        base = (UCHAR)(sib & 7);
        if (mod == 0 && base == 5) {
            /* SIB with base==101 and mod==00 hides a disp32 (SDM Vol 2A). */
            if (!HookDecHasBytes(Decoder, 4)) {
                return FALSE;
            }
            Decoder->Offset += 4;
        }
    } else if (mod == 0 && rm == 5) {
        *RipRelative = TRUE;
        *RipDisplacementOffset =
            Decoder->Offset - InstructionStartOffset;
        if (!HookDecHasBytes(Decoder, 4)) {
            return FALSE;
        }
        Decoder->Offset += 4;
        return TRUE;
    }

    if (mod == 1) {
        if (!HookDecHasBytes(Decoder, 1)) {
            return FALSE;
        }
        Decoder->Offset += 1;
    } else if (mod == 2) {
        if (!HookDecHasBytes(Decoder, 4)) {
            return FALSE;
        }
        Decoder->Offset += 4;
    }

    return TRUE;
}

static ULONG
HookDecImmediateSize(
    _In_ UCHAR Opcode,
    _In_ UCHAR RexW,
    _In_ UCHAR OperandSizeOverride,
    _In_ UCHAR ModRmRegField
    )
{
    switch (Opcode) {
    case 0xB0: case 0xB1: case 0xB2: case 0xB3:
    case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        return 1;                              /* MOV r8, imm8 */
    case 0xB8: case 0xB9: case 0xBA: case 0xBB:
    case 0xBC: case 0xBD: case 0xBE: case 0xBF:
        return RexW ? 8 : (OperandSizeOverride ? 2 : 4); /* MOV r, imm */
    case 0x68:                               /* PUSH imm32 */
        return OperandSizeOverride ? 2 : 4;
    case 0x69:                               /* IMUL r, r/m, imm32 */
        return RexW ? 4 : (OperandSizeOverride ? 2 : 4);
    case 0x6B:                               /* IMUL r, r/m, imm8 */
        return 1;
    case 0xA9:                               /* TEST EAX, imm32 */
        return RexW ? 4 : (OperandSizeOverride ? 2 : 4);
    case 0xC7:                               /* MOV r/m, imm32 */
        return RexW ? 4 : (OperandSizeOverride ? 2 : 4);
    case 0x81:                               /* arithmetic r/m, imm32 */
        return RexW ? 4 : (OperandSizeOverride ? 2 : 4);
    case 0x6A:                               /* PUSH imm8 */
    case 0x80: case 0x82: case 0x83:         /* arithmetic r/m, imm8 */
    case 0xC0: case 0xC1:                    /* shift r/m, imm8 */
    case 0xCD:                               /* INT imm8 */
    case 0xA8:                               /* TEST AL, imm8 */
        return 1;
    case 0xF6:
        return (ModRmRegField == 0) ? 1 : 0; /* TEST r/m8, imm8 */
    case 0xF7:
        return (ModRmRegField == 0) ?
            (RexW ? 4 : (OperandSizeOverride ? 2 : 4)) : 0;
    case 0xC6:                               /* MOV r/m8, imm8 */
        return 1;
    default:
        return 0;
    }
}

static BOOLEAN
HookDecOpcodeUsesModRm(
    _In_ UCHAR Opcode
    )
{
    switch (Opcode) {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x08: case 0x09:
    case 0x0A: case 0x0B: case 0x10: case 0x11: case 0x12: case 0x13:
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x20: case 0x21:
    case 0x22: case 0x23: case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x38: case 0x39:
    case 0x3A: case 0x3B:
    case 0x62: case 0x63:
    case 0x69: case 0x6B:
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87: case 0x88: case 0x89:
    case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
    case 0xC0: case 0xC1: case 0xC6: case 0xC7:
    case 0xD0: case 0xD1: case 0xD2: case 0xD3:
    case 0xF6: case 0xF7: case 0xFE: case 0xFF:
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOLEAN
HookDecOpcodeWithoutModRmSupported(
    _In_ UCHAR Opcode
    )
{
    if ((Opcode >= 0x50 && Opcode <= 0x5F) ||
        (Opcode >= 0xB0 && Opcode <= 0xBF)) {
        return TRUE;
    }
    switch (Opcode) {
    case 0x68: case 0x6A:
    case 0x90: case 0x98: case 0x99:
    case 0x9C: case 0x9D:
    case 0xF8: case 0xF9: case 0xFA:
    case 0xFB: case 0xFC: case 0xFD:
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOLEAN
HookDecInstruction(
    _Inout_ HOOK_DECODER* Decoder,
    _Out_ HOOK_INSTR_INFO* Info
    )
{
    ULONG startOffset = Decoder->Offset;
    UCHAR opcode;
    UCHAR rexPrefix = 0;
    UCHAR rexW = 0;
    UCHAR operandSizeOverride = 0;
    BOOLEAN ripRelative = FALSE;
    ULONG ripDisplacementOffset = 0;

    RtlZeroMemory(Info, sizeof(*Info));
    if (!HookDecHasBytes(Decoder, 1)) {
        return FALSE;
    }

    /* Legacy prefixes (at most one per group in practice). */
    for (;;) {
        if (!HookDecHasBytes(Decoder, 1)) {
            return FALSE;
        }
        opcode = HookDecPeek(Decoder);
        if (opcode == 0xF0 || opcode == 0xF2 || opcode == 0xF3 ||
            opcode == 0x2E || opcode == 0x36 || opcode == 0x3E ||
            opcode == 0x26 || opcode == 0x64 || opcode == 0x65 ||
            opcode == 0x66 || opcode == 0x67) {
            if (opcode == 0x67) {
                /* Address-size override changes RIP-relative rules. */
                return FALSE;
            }
            if (opcode == 0x66) {
                operandSizeOverride = 1;
            }
            HookDecRead(Decoder);
            continue;
        }
        break;
    }

    /* REX prefix (0x40-0x4F). */
    if (!HookDecHasBytes(Decoder, 1)) {
        return FALSE;
    }
    opcode = HookDecPeek(Decoder);
    if (opcode >= 0x40 && opcode <= 0x4F) {
        rexPrefix = HookDecRead(Decoder);
        rexW = (UCHAR)((rexPrefix >> 3) & 1);
        if (!HookDecHasBytes(Decoder, 1)) {
            return FALSE;
        }
        opcode = HookDecPeek(Decoder);
    }

    if (opcode == 0x0F || opcode == 0x62 ||
        opcode == 0xC4 || opcode == 0xC5) {
        /* Multi-byte, EVEX, and VEX opcode maps are not decoded. */
        return FALSE;
    }

    if (opcode == 0xCD || opcode == 0xE8 || opcode == 0xE9 ||
        opcode == 0xEB || (opcode >= 0x70 && opcode <= 0x7F) ||
        (opcode >= 0xE0 && opcode <= 0xE3)) {
        /* Relative control flow needs relocation, which is not implemented. */
        return FALSE;
    }

    HookDecRead(Decoder);

    if (HookDecOpcodeUsesModRm(opcode)) {
        UCHAR modRm;
        UCHAR regField;
        BOOLEAN wasRipRelative = FALSE;
        ULONG displacementOffset = 0;
        if (!HookDecHasBytes(Decoder, 1)) {
            return FALSE;
        }
        modRm = HookDecRead(Decoder);
        regField = (UCHAR)((modRm >> 3) & 7);
        if ((opcode == 0x8F && regField != 0) ||
            ((opcode == 0xC6 || opcode == 0xC7) && regField != 0) ||
            (opcode == 0xFE && regField > 1) ||
            (opcode == 0xFF && regField >= 2 && regField <= 5)) {
            return FALSE;
        }
        if (!HookDecConsumeModRmTail(
                Decoder, modRm, startOffset,
                &wasRipRelative, &displacementOffset)) {
            return FALSE;
        }
        ripRelative = wasRipRelative;
        ripDisplacementOffset = displacementOffset;

        {
            ULONG immediateSize = HookDecImmediateSize(
                opcode, rexW, operandSizeOverride, regField);
            if (immediateSize != 0) {
                if (!HookDecHasBytes(Decoder, immediateSize)) {
                    return FALSE;
                }
                Decoder->Offset += immediateSize;
            }
        }
    } else {
        ULONG immediateSize = HookDecImmediateSize(
            opcode, rexW, operandSizeOverride, 0);
        if (immediateSize == 0 &&
            !HookDecOpcodeWithoutModRmSupported(opcode)) {
            return FALSE;
        }
        if (immediateSize != 0) {
            if (!HookDecHasBytes(Decoder, immediateSize)) {
                return FALSE;
            }
            Decoder->Offset += immediateSize;
        }
    }

    Info->Length = Decoder->Offset - startOffset;
    Info->RipRelative = ripRelative;
    Info->RipDisplacementOffset =
        ripRelative ? ripDisplacementOffset : 0;
    return Info->Length != 0;
}

static BOOLEAN
HookTrampolineRel32Fits(
    _In_ LONG64 Displacement
    )
{
    return Displacement >= HOOK_REL32_MIN &&
           Displacement <= HOOK_REL32_MAX;
}

static VOID
HookTrampolineEncodeAbsoluteJmp(
    _Out_writes_bytes_(HOOK_TRAMPOLINE_JMP_SIZE) PUCHAR Out,
    _In_ ULONG64 TargetVa
    )
{
    Out[0] = 0xFF;
    Out[1] = 0x25;
    Out[2] = 0;
    Out[3] = 0;
    Out[4] = 0;
    Out[5] = 0;
    Out[6] = (UCHAR)(TargetVa & 0xFF);
    Out[7] = (UCHAR)((TargetVa >> 8) & 0xFF);
    Out[8] = (UCHAR)((TargetVa >> 16) & 0xFF);
    Out[9] = (UCHAR)((TargetVa >> 24) & 0xFF);
    Out[10] = (UCHAR)((TargetVa >> 32) & 0xFF);
    Out[11] = (UCHAR)((TargetVa >> 40) & 0xFF);
    Out[12] = (UCHAR)((TargetVa >> 48) & 0xFF);
    Out[13] = (UCHAR)((TargetVa >> 56) & 0xFF);
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
HookTrampolineBuild(
    _In_ PUCHAR OriginalVa,
    _In_ ULONG MinBytes,
    _Out_ PVOID* TrampolineVirtual,
    _Out_ ULONG* BytesCopied
    )
{
    UCHAR original[HOOK_TRAMPOLINE_MAX_ORIGINAL];
    MM_COPY_ADDRESS source;
    SIZE_T originalBytes = 0;
    PUCHAR buffer;
    PUCHAR destination;
    ULONG copiedLength;
    HOOK_DECODER decoder;

    *TrampolineVirtual = NULL;
    *BytesCopied = 0;

    if (OriginalVa == NULL || MinBytes == 0 ||
        MinBytes > HOOK_TRAMPOLINE_MAX_ORIGINAL) {
        return STATUS_INVALID_PARAMETER;
    }

    source.VirtualAddress = OriginalVa;
    (VOID)MmCopyMemory(
        original, source, sizeof(original),
        MM_COPY_MEMORY_VIRTUAL, &originalBytes);
    if (originalBytes < MinBytes) {
        return STATUS_PARTIAL_COPY;
    }

    buffer = (PUCHAR)ExAllocatePool2(
        POOL_FLAG_NON_PAGED_EXECUTE | POOL_FLAG_UNINITIALIZED,
        HOOK_TRAMPOLINE_ALLOC, HV_POOL_TAG_HOOK_CODE);
    if (buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    decoder.Start = original;
    decoder.Offset = 0;
    decoder.Length = (ULONG)originalBytes;
    destination = buffer;
    copiedLength = 0;

    NT_ASSERT(!HookTrampolineRel32Fits(HOOK_REL32_MIN - 1));
    NT_ASSERT(HookTrampolineRel32Fits(HOOK_REL32_MIN));
    NT_ASSERT(HookTrampolineRel32Fits(HOOK_REL32_MAX));
    NT_ASSERT(!HookTrampolineRel32Fits(HOOK_REL32_MAX + 1));

    while (copiedLength < MinBytes) {
        HOOK_INSTR_INFO info;
        ULONG instructionOffset = decoder.Offset;
        ULONG64 instructionVa;
        ULONG64 targetVa;

        if (!HookDecInstruction(&decoder, &info)) {
            ExFreePoolWithTag(buffer, HV_POOL_TAG_HOOK_CODE);
            return STATUS_ILLEGAL_INSTRUCTION;
        }
        if (copiedLength + info.Length > HOOK_TRAMPOLINE_MAX_ORIGINAL) {
            ExFreePoolWithTag(buffer, HV_POOL_TAG_HOOK_CODE);
            return STATUS_BUFFER_TOO_SMALL;
        }

        RtlCopyMemory(
            destination, original + instructionOffset, info.Length);

        if (info.RipRelative) {
            LONG originalDisplacement;
            LONG relocatedDisplacement32;
            LONG64 relocatedDisplacement;

            originalDisplacement = (LONG)(
                (ULONG)original[
                    instructionOffset + info.RipDisplacementOffset] |
                ((ULONG)original[
                    instructionOffset +
                    info.RipDisplacementOffset + 1] << 8) |
                ((ULONG)original[
                    instructionOffset +
                    info.RipDisplacementOffset + 2] << 16) |
                ((ULONG)original[
                    instructionOffset +
                    info.RipDisplacementOffset + 3] << 24));
            instructionVa = (ULONG64)OriginalVa + instructionOffset;
            targetVa = instructionVa + info.Length +
                       (ULONG64)(LONG64)originalDisplacement;
            relocatedDisplacement = (LONG64)targetVa -
                (LONG64)((ULONG64)destination + info.Length);
            if (!HookTrampolineRel32Fits(relocatedDisplacement)) {
                ExFreePoolWithTag(buffer, HV_POOL_TAG_HOOK_CODE);
                return STATUS_NOT_SUPPORTED;
            }
            relocatedDisplacement32 = (LONG)relocatedDisplacement;
            destination[info.RipDisplacementOffset] =
                (UCHAR)((ULONG)relocatedDisplacement32 & 0xFF);
            destination[info.RipDisplacementOffset + 1] =
                (UCHAR)(((ULONG)relocatedDisplacement32 >> 8) & 0xFF);
            destination[info.RipDisplacementOffset + 2] =
                (UCHAR)(((ULONG)relocatedDisplacement32 >> 16) & 0xFF);
            destination[info.RipDisplacementOffset + 3] =
                (UCHAR)(((ULONG)relocatedDisplacement32 >> 24) & 0xFF);
        }

        destination += info.Length;
        copiedLength += info.Length;
    }

    HookTrampolineEncodeAbsoluteJmp(
        destination, (ULONG64)OriginalVa + copiedLength);

    *TrampolineVirtual = buffer;
    *BytesCopied = copiedLength;
    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
HookTrampolineFree(
    _In_ PVOID TrampolineVirtual
    )
{
    if (TrampolineVirtual != NULL) {
        ExFreePoolWithTag(TrampolineVirtual, HV_POOL_TAG_HOOK_CODE);
    }
}
