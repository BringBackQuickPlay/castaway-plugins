// === BytePatcher Include File ===
// Drop this into your includes folder as bytepatcher.inc
// Credits: VerdiusArcana (with assistance from ChatGPT)
// UNTESTED, NOT DONE, DON'T EVEN KNOW IF A CODECAVE
// EVEN CAN BE CREATED SO THIS IS JUST A CONCEPT.
// IT HAS NEVER BEEN COMPILED AND TESTED DO NOT USE

// --- Jump Type Defines ---
#define JMP_REL8     1
#define JE_REL8      2
#define JNE_REL8     3
#define JL_REL8      4
#define JLE_REL8     5
#define JG_REL8      6
#define JGE_REL8     7

#define JMP_REL32    10
#define CALL_REL32   11
#define JE_REL32     12
#define JNE_REL32    13
#define JL_REL32     14
#define JLE_REL32    15
#define JG_REL32     16
#define JGE_REL32    17

#define JMP_ABS32    100  // JMP [absAddr] (FF 25 [addr])
#define CALL_ABS32   101  // CALL [absAddr] (FF 15 [addr])

// --- Jump Opcode Resolver ---
int GetJumpOpcode(int type)
{
    switch (type)
    {
        case JMP_REL8:  return 0xEB;
        case JE_REL8:   return 0x74;
        case JNE_REL8:  return 0x75;
        case JL_REL8:   return 0x7C;
        case JLE_REL8:  return 0x7E;
        case JG_REL8:   return 0x7F;
        case JGE_REL8:  return 0x7D;

        case JMP_REL32: return 0xE9;
        case CALL_REL32:return 0xE8;
        case JE_REL32:  return 0x84;
        case JNE_REL32: return 0x85;
        case JL_REL32:  return 0x8C;
        case JLE_REL32: return 0x8E;
        case JG_REL32:  return 0x8F;
        case JGE_REL32: return 0x8D;

        default: return -1;
    }
}

// --- BytePatcher Main Function ---
void BytePatcher(int buffer[], int offset, int shortjmpOffset, int instrAddr, int targetAddr, int type, int bufferSize)
{
    int opcode = GetJumpOpcode(type);
    if (opcode == -1 && type != JMP_ABS32 && type != CALL_ABS32)
    {
        SetFailState("BytePatcher.inc: Invalid jump/call type: %d", type);
    }

    int requiredBytesBefore = (type >= JE_REL32 && type <= JGE_REL32) ? 2 : 1;
    int requiredBytesAfter = (type == JMP_REL32 || type == CALL_REL32 || (type >= JE_REL32 && type <= JGE_REL32)) ? 4 : 1;

    if (offset < requiredBytesBefore)
    {
        SetFailState("BytePatcher.inc: offset %d too small for instruction type %d (needs at least %d)", offset, type, requiredBytesBefore);
    }

    if ((offset + requiredBytesAfter) > bufferSize)
    {
        SetFailState("BytePatcher.inc: offset %d would write beyond end of buffer (offset + size = %d > bufferSize = %d)", offset, offset + requiredBytesAfter, bufferSize);
    }

    if (type >= JMP_REL8 && type <= JGE_REL8)
    {
        if (!(instrAddr == 0 && targetAddr == 0))
        {
            SetFailState("BytePatcher.inc used incorrectly. For short jumps, instrAddr AND targetAddr must be 0!");
        }

        if ((offset + 1) > bufferSize || offset < 1)
        {
            SetFailState("BytePatcher.inc would write beyond end of buffer or offset too small for short jump");
        }

        buffer[offset - 1] = opcode;
        buffer[offset + 0] = shortjmpOffset;
    }
    else if (type == JMP_REL32 || type == CALL_REL32)
    {
        if (instrAddr == 0 || targetAddr == 0)
        {
            SetFailState("BytePatcher.inc: instrAddr or targetAddr is 0 for rel32 jump/call.");
        }

        if (shortjmpOffset != 0)
        {
            SetFailState("BytePatcher.inc: shortjmpOffset must be 0 for long jumps/calls.");
        }

        if (targetAddr >= instrAddr && targetAddr < (instrAddr + 5))
        {
            SetFailState("BytePatcher.inc: JMP/CALL cannot jump into its own opcode/offset bytes.");
        }

        buffer[offset - 1] = opcode;

        int rel32 = targetAddr - (instrAddr + 5);
        buffer[offset + 0] = rel32 & 0xFF;
        buffer[offset + 1] = (rel32 >> 8) & 0xFF;
        buffer[offset + 2] = (rel32 >> 16) & 0xFF;
        buffer[offset + 3] = (rel32 >> 24) & 0xFF;
    }
    else if (type >= JE_REL32 && type <= JGE_REL32)
    {
        if (instrAddr == 0 || targetAddr == 0)
        {
            SetFailState("BytePatcher.inc: instrAddr or targetAddr is 0 for long conditional jump.");
        }

        if (targetAddr >= instrAddr && targetAddr < (instrAddr + 6))
        {
            SetFailState("BytePatcher.inc: Conditional jump cannot target its own bytes.");
        }

        buffer[offset - 2] = 0x0F;
        buffer[offset - 1] = opcode;

        int rel32 = targetAddr - (instrAddr + 6);
        buffer[offset + 0] = rel32 & 0xFF;
        buffer[offset + 1] = (rel32 >> 8) & 0xFF;
        buffer[offset + 2] = (rel32 >> 16) & 0xFF;
        buffer[offset + 3] = (rel32 >> 24) & 0xFF;
    }
    else if (type == JMP_ABS32)
    {
        if (targetAddr == 0)
        {
            SetFailState("BytePatcher.inc: JMP_ABS32 requires targetAddr to be non-zero (absolute address). instrAddr must be 0.");
        }
        if (instrAddr != 0)
        {
            SetFailState("BytePatcher.inc: JMP_ABS32 expects instrAddr to be 0 (not used in absolute mode). Use 0 explicitly.");
        }

        if (offset < 2 || (offset + 4) > bufferSize)
        {
            SetFailState("BytePatcher.inc bounds error for JMP_ABS32");
        }

        buffer[offset - 2] = 0xFF;
        buffer[offset - 1] = 0x25;

        buffer[offset + 0] = targetAddr & 0xFF;
        buffer[offset + 1] = (targetAddr >> 8) & 0xFF;
        buffer[offset + 2] = (targetAddr >> 16) & 0xFF;
        buffer[offset + 3] = (targetAddr >> 24) & 0xFF;
    }
    else if (type == CALL_ABS32)
    {
        if (targetAddr == 0)
    {
        SetFailState("BytePatcher.inc: CALL_ABS32 requires targetAddr to be non-zero (absolute address). instrAddr must be 0.");
    }
    if (instrAddr != 0)
    {
        SetFailState("BytePatcher.inc: CALL_ABS32 expects instrAddr to be 0 (not used in absolute mode). Use 0 explicitly.");
    }

        if (offset < 2 || (offset + 4) > bufferSize)
        {
            SetFailState("BytePatcher.inc bounds error for CALL_ABS32");
        }

        buffer[offset - 2] = 0xFF;
        buffer[offset - 1] = 0x15;

        buffer[offset + 0] = targetAddr & 0xFF;
        buffer[offset + 1] = (targetAddr >> 8) & 0xFF;
        buffer[offset + 2] = (targetAddr >> 16) & 0xFF;
        buffer[offset + 3] = (targetAddr >> 24) & 0xFF;
    }
    else
    {
        SetFailState("BytePatcher.inc: Unhandled jump type %d", type);
    }
}
