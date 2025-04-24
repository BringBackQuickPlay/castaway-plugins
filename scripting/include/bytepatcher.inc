// === BytePatcher Include File ===
// Drop this into your includes folder as bytepatcher.inc
// Credits: VerdiusArcana (with assistance from ChatGPT)

// desc: Used to manipulate the memory of int arrays, 
// typically to intentionally corrupt it in order to
// have it be treated as actual CPU instructions.
// but can also be used to just store larger
// amounts of data.
// Depends on SourceScramble to function because
// it needs the "Address of Natives" functionality.

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
stock int GetJumpOpcode(int type)
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
stock BytePatcher(int offset, int shortjmpOffset, int instrAddr, int targetAddr, int type, int bufferSize, int[] buffer)
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



// Writes a sequence of bytes into a buffer at the specified offset.
// The amount of bytes you want to write to buffer (using customBytes) can be 1 to the size of the buffer.
// buffer[]       The output buffer to write into. Should be a int array of this manner:
//                int patchBytes[4] = {0x90, 0x90, 0x90, 0x90};
// bufferSize     The total size of the buffer. Use sizeof(bufferSize)
// offset         Offset at which to start writing bytes.
// customBytes[]  The array of bytes to write, declare a int array i.e int patchBytes[4] = {0x90, 0x90, 0x90, 0xCC}; 
//                And use that as parameter when you call the function.
// customSize     How many bytes to write from customBytes. Use sizeof(customBytes)

// For code readability, it's suggested you never write more than 8 bytes per line in your editor of choice.
// As a example: if you want to make a large patch of 32 bytes into your 64 byte target, consider doing it like this:

//      int myPatch[32] = {0x90, 0x90, 0x90, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
//                         0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x90, 0x90,
//                         0x00, 0xFF, 0x90, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
//                         0xCC, 0xCC, 0xCC, 0x00, 0x00, 0xFF, 0x00, 0x90};

// And then call BytePatcherCustom like this:

// BytePatcherCustom(myTargetCodeCave, sizeof(myTargetCodeCave), 0, myPatch, sizeof(myPatch));
// And remember the power of two, you may simply NOP the bytes you don't need in your patch.

stock void BytePatcherCustom(int[] buffer, int bufferSize, int offset, int[] customBytes, int customSize)
{
    if (customSize <= 0)
    {
        SetFailState("BytePatcherCustom: custom byte count must be > 0");
    }

    if ((offset < 0) || (offset + customSize > bufferSize))
    {
        SetFailState("BytePatcherCustom: write would exceed buffer bounds (offset = %d, size = %d, max = %d)", offset, customSize, bufferSize);
    }

    for (int i = 0; i < customSize; i++)
    {
        buffer[offset + i] = customBytes[i] & 0xFF; // ensure only the low byte is written
    }
}




// View memory from an address with full hex + ASCII format.
// startAddr  Memory address to begin reading from.
// count      Total number of bytes to display.
// mode       0 = "90", 1 = "0x90", 2 = "\\x90"
// compress   If false, adds space between groups. If true, no spaces at all.
// showchars  If true, displays ASCII only (ignores mode/compress).

stock void ViewMemoryBytes(Address startAddr, int count, int mode = 0, bool compress = false, bool showchars = false)
{
    int bytes[16];
    char hexLine[256];
    char asciiLine[17];

    Address endAddr = startAddr + view_as<Address>(count) - view_as<Address>(1);
    PrintToServer("=== Memory View: 0x%08X → 0x%08X ===", startAddr, endAddr);

    for (int offset = 0; offset < count; offset += 16)
    {
        int lineCount = (offset + 16 <= count) ? 16 : (count - offset);

        hexLine[0] = '\0';
        asciiLine[0] = '\0';

        for (int i = 0; i < lineCount; i++)
        {
            int byteVal = LoadFromAddress(startAddr + view_as<Address>(offset) + view_as<Address>(i), NumberType_Int8) & 0xFF;
            bytes[i] = byteVal;

            // Build hex part
            char temp[16];
            switch (mode)
            {
                case 0: {
                    Format(temp, sizeof(temp), "%02X", byteVal);
                    break;
                }
                case 1: {
                    Format(temp, sizeof(temp), "0x%02X", byteVal);
                    break;
                }
                case 2: {
                    Format(temp, sizeof(temp), "\\x%02X", byteVal);
                    break;
                }
            }

            StrCat(hexLine, sizeof(hexLine), temp);

            bool isLast = (i == lineCount - 1);
            bool atGroupEnd = ((i + 1) % 8 == 0);

            if (!isLast && !compress && mode == 0)
            {
                StrCat(hexLine, sizeof(hexLine), atGroupEnd ? "  " : " ");
            }

            // Build ASCII part (printable chars only)
            asciiLine[i] = (byteVal >= 32 && byteVal <= 126) ? byteVal : '.';
        }

        asciiLine[lineCount] = '\0';  // Null-terminate

        if (showchars)
        {
            PrintToServer("[0x%08X] %s", startAddr + view_as<Address>(offset), asciiLine);
        }
        else
        {
            PrintToServer("[0x%08X] %-50s | %s", startAddr + view_as<Address>(offset), hexLine, asciiLine);
        }
    }
}

// Converts an array of bytes into a printable ASCII string.
// Replaces non-printable characters with '.'.
// The result is written to the provided output string buffer.
// Warns if output buffer is to small.
stock void BytesToAscii(int[] buffer, int count, char[] out, int outSize)
{
    if (outSize <= 1)
    {
        PrintToServer("BytePatcher.inc(BytesToAscii): output buffer too small");
    }

    int limit = (count < (outSize - 1)) ? count : (outSize - 1);

    for (int i = 0; i < limit; i++)
    {
        out[i] = (buffer[i] >= 32 && buffer[i] <= 126) ? buffer[i] : '.';
    }

    out[limit] = '\0';

    PrintToServer("BytePatcher.inc(BytesToAscii): converted %d bytes to ASCII string", limit);
}

// Checks if a given address is likely valid.
// Rejects null and very low addresses (e.g., page zero).
// Returns true if the address is above 0x1000.
// Warns (and returns false) if the address is likely invalid.
stock bool IsValidAddress(Address addr)
{
    if (addr <= view_as<Address>(0x1000))
    {
        PrintToServer("BytePatcher.inc(IsValidAddress): address 0x%08X is too low and likely invalid", addr);
        return false;
    }

    PrintToServer("BytePatcher.inc(IsValidAddress): 0x%08X is likely valid", addr);
    return true;
}

// Copies bytes from a source memory address to a destination.
// Validates both source and destination addresses first.
// Useful for copying memory regions or replicating patch contents.
// Warns (and returns false) on invalid addresses or if the byte count is zero or negative.
stock bool CopyBytes(Address src, Address dst, int count)
{
    if (count <= 0)
    {
        PrintToServer("BytePatcher.inc(CopyBytes): byte count must be > 0");
        return false;
    }

    if (!IsValidAddress(src))
    {
        PrintToServer("BytePatcher.inc(CopyBytes): source address 0x%08X is invalid", src);
        return false;
    }

    if (!IsValidAddress(dst))
    {
        PrintToServer("BytePatcher.inc(CopyBytes): destination address 0x%08X is invalid", dst);
        return false;
    }

    for (int i = 0; i < count; i++)
    {
        int val = LoadFromAddress(src + view_as<Address>(i), NumberType_Int8);
        StoreToAddress(dst + view_as<Address>(i), val, NumberType_Int8);
    }

    PrintToServer("BytePatcher.inc(CopyBytes): copied %d bytes from 0x%08X to 0x%08X", count, src, dst);
    return true;
}
