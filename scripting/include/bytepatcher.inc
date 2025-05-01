// === BytePatcher Include File ===
// By VerdiusArcana (lol ty chatgpt)
// Drop this into your includes folder as bytepatcher.inc
// Credits: VerdiusArcana (with assistance from ChatGPT)

// desc: Used to manipulate the memory of a char array, 
// typically to intentionally corrupt it in order to
// have it be treated as actual CPU instructions.
// but can also be used to just store data if neccesary.
// Depends on SourceScramble to function because
// it needs the "Address of Natives" functionality.

// Debug var. if true then print debug lines:



bool doDebug_BytepatcherINC = true;

// To use in a plugin: 

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

// --- SetupCodeCave ---
// Like the function is named, sets up the codecave so it has the CC traps and signatures.
// Q: Why CC traps?
// A: To cause a server crash if code tries to enter it. The TF2 server itself will
// never manage to enter the memorysection containing the var, it's more so to ensure 
// a crash if a patch made using bytepatcher gets it wrong (aka the one making the patch fucked up).
// You should ALWAYS have some form of JMP out of the codecave once you are done.

// If you intend to signature search: convert start signature into
// bytes, and combine it with the start trap marker and end trap marker.
// so \xCC\x90\x90\xCC + Bytes of start signature + \xCC\x90\x90\xCC

// For the default startSignature "POOTISPOW-CODE-CAVE-START"
// the total start signature would be:

// \xCC\x90\x90\xCC\x50\x4F\x4F\x54\x49\x53\x50\x4F\x57\x2D\x43\x4F
// \x44\x45\x2D\x43\x41\x56\x45\x2D\x53\x54\x41\x52\x54\xCC\x90\x90\xCC

// Please be aware that signatures are supposed to be one liners, it's
// only split up here in the comments for readability.

// Finds the first 4096-byte page-aligned region within a given 8192-byte char array.
// Returns Address_Null if no valid page-aligned region could be found (extremely unlikely).
// 'baseAddr' should be the address of the 8kb buffer.
// The return value is the aligned address that can be safely used as the start of the codecave.
static Address FindAlignedCodecaveStart(Address baseAddr, int totalSize = 8192)
{
    for (int offset = 0; offset <= (totalSize - 4096); offset++)
    {
        Address candidate = baseAddr + view_as<Address>(offset);
        if ((candidate & view_as<Address>(0xFFF)) == view_as<Address>(0))
        {
            if ((candidate + view_as<Address>(4096)) <= (baseAddr + view_as<Address>(totalSize)))
            {
                return candidate;
            }
        }
    }

    PrintToServer("Bytepatcher.inc(FindAlignedCodecaveStart): Failed to find 4KB page-aligned region in 8KB buffer!");
    return Address_Null;
}

// Sets up the codecave within a provided 8192-byte char buffer.
// Populates the given reverts_codecave struct with everything needed
// please change startSignature and endSignature to have the name of your 
// plugin file. For example if your .sp is named myplugin then
// they should say:
// endSignature = MYPLUGIN-CODE-CAVE-START
// endSignature = MYPLUGIN-CODE-CAVE-END

// This is to avoid conflicts with other plugins who might want their own codecave.
// Furthermore, if plugin collaborators co-operate (say 4 plugin authors split up the remaining
// 4000 bytes after signatures or what have you) they can save on RAM.
// Just make sure your section starts with CC 90 and ends with 90 CC
// so if one author messes up, the server crashes instead of trying to read
// the other authors instructions.
stock void SetupCodeCave(char buffer[8192], Reverts_CodeCave reverts_codecave)
{
    Address baseAddr = GetAddressOfString(buffer);
    Address aligned = FindAlignedCodecaveStart(baseAddr);

    if (aligned == Address_Null)
    {
        SetFailState("Bytepatcher.inc(SetupCodeCave) CRITICALERROR: Failed to locate page-aligned 4KB region inside buffer!");
        return;
    }

    // Fill 8KB with NOPs and trap markers at beginning/end
    for (int i = 0; i < 8192; i++)
    {
        buffer[i] = 0x90;
    }
    buffer[0] = 0xCC;
    buffer[8191] = 0xCC;

    // Offset into buffer where the aligned 4KB page starts
    int offsetToAligned = aligned - baseAddr;
    int offset = 0;

    // Start trap: CC 90 90 CC
    buffer[offsetToAligned + offset++] = 0xCC;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0xCC;

    // Signature
    static const char startSignature[] = "POOTISPOW-CODE-CAVE-START";
    int startSigLen = strlen(startSignature);
    for (int i = 0; i < startSigLen; i++)
    {
        buffer[offsetToAligned + offset++] = startSignature[i];
    }

    // End of start trap
    buffer[offsetToAligned + offset++] = 0xCC;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0xCC;

    // Address and offset of first usable byte
    Address afterStart = aligned + view_as<Address>(offset);
    int offsetAfterStart = offsetToAligned + offset;

    // Footer setup
    static const char endSignature[] = "POOTISPOW-CODE-CAVE-END";
    int endSigLen = strlen(endSignature);
    int footerSize = 4 + endSigLen + 4;

    while (offset < (4096 - footerSize))
    {
        buffer[offsetToAligned + offset++] = 0x90;
    }

    // Footer trap
    buffer[offsetToAligned + offset++] = 0xCC;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0xCC;

    for (int i = 0; i < endSigLen; i++)
    {
        buffer[offsetToAligned + offset++] = endSignature[i];
    }

    buffer[offsetToAligned + offset++] = 0xCC;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0x90;
    buffer[offsetToAligned + offset++] = 0xCC;

    if (offset != 4096)
    {
        SetFailState("SetupCodeCave: Internal logic error — final size is %d, expected 4096", offset);
    }

    // Populate codecave struct
    reverts_codecave.AddressOf_g_8kbforcodecave = baseAddr;
    reverts_codecave.AddressOf_reverts_codecave = aligned;
    reverts_codecave.OffsetOf_reverts_codecave_start = offsetToAligned;
    reverts_codecave.AddressOf_reverts_codecave_startbyte = afterStart;
    reverts_codecave.OffsetOf_reverts_codecave_startbyte = offsetAfterStart;

    // Sanitycheck (part 1, part 2 needs to be done directly after SetupCodeCave call in your plugin)
    // check that addresses are valid.

    if (!IsValidAddress(reverts_codecave.AddressOf_g_8kbforcodecave) &&
        !IsValidAddress(reverts_codecave.AddressOf_reverts_codecave) && 
        !IsValidAddress(reverts_codecave.AddressOf_reverts_codecave_startbyte)) 
    {
        SetFailState(
        "Bytepatcher.inc(SetupCodeCave) CRITICALERROR: The addresses in the enum struct were no good! AddressOf_g_8kbforcodecave:0x%08X AddressOf_reverts_codecave:0x%08X AddressOf_reverts_codecave_startbyte:0x%08X",
        reverts_codecave.AddressOf_g_8kbforcodecave,
        reverts_codecave.AddressOf_reverts_codecave,
        reverts_codecave.AddressOf_reverts_codecave_startbyte
        );       
    } else {
        if (doDebug_BytepatcherINC) {
            PrintToServer("====== Codecave Debug Info (SetupCodeCave) ======");
            PrintToServer("8KB base address:               0x%08X", reverts_codecave.AddressOf_g_8kbforcodecave);
            PrintToServer("Page-aligned codecave address:  0x%08X", reverts_codecave.AddressOf_reverts_codecave);
            PrintToServer("Offset to codecave start:       %d",     reverts_codecave.OffsetOf_reverts_codecave_start);
            PrintToServer("Usable byte address:            0x%08X", reverts_codecave.AddressOf_reverts_codecave_startbyte);
            PrintToServer("Offset to usable byte:          %d",     reverts_codecave.OffsetOf_reverts_codecave_startbyte);
            PrintToServer("=================================================");
        }
    }



    // Check that codecave is within it's own memorypage:

    if (!VerifyMemoryRegionSinglePage(reverts_codecave.AddressOf_reverts_codecave,4096)) {
        SetFailState("Bytepatcher.inc(SetupCodeCave) CRITICALERROR: CodeCave did not have it's own memorypage!");
    } else {
        if (doDebug_BytepatcherINC) PrintToServer("Bytepatcher.inc(SetupCodeCave) SUCCESS: CodeCave does have have it's own memorypage!");
    }
}






// --- WriteJMPorCALLInstruction ---
// Writes a JMP or CALL instruction (short, relative, absolute) into a byte buffer.
// Used to insert control-flow redirection into codecaves or calling other functions.
// Typically the most common reason to insert a CALL instruction might be to replace
// the one that was sacrificed in the original assembly in the function in order to jump into the code cave.
// Should be combined with BytePatcherCustom. This is just to save a bit of brainpower.
//
// Parameters:
//   offset         → Index into the buffer where the patch should be written.
//   shortjmpOffset → Offset used for short jumps (JMP_REL8, JE_REL8, etc). Ignored for other types. -+127
//   instrAddr      → Actual memory address where the instruction will be placed (used to compute rel32 offsets).
//   targetAddr     → Destination address the JMP/CALL will redirect to. Use 0 for short jumps.
//   type           → The jump type constant (JMP_REL8, CALL_REL32, etc). See the top of Bytepatcher.inc for types you can use.
//   bufferSize     → Total number of bytes available in the buffer. ALWAYS call sizeof(buffer) for this.
//   buffer[]       → The int array representing the byte buffer to write into.
//
stock void WriteJMPorCALLInstruction(int offset, int shortjmpOffset, int instrAddr, int targetAddr, int type, int bufferSize, char[] buffer)
{
    int opcode = GetJumpOpcode(type);
    if (opcode == -1 && type != JMP_ABS32 && type != CALL_ABS32)
    {
        SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: Invalid jump/call type: %d", type);
    }

    int requiredBytesBefore = (type >= JE_REL32 && type <= JGE_REL32) ? 2 : 1;
    int requiredBytesAfter = (type == JMP_REL32 || type == CALL_REL32 || (type >= JE_REL32 && type <= JGE_REL32)) ? 4 : 1;

    if (offset < requiredBytesBefore)
    {
        SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: offset %d too small for instruction type %d (needs at least %d)", offset, type, requiredBytesBefore);
    }

    if ((offset + requiredBytesAfter) > bufferSize)
    {
        SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: offset %d would write beyond end of buffer (offset + size = %d > bufferSize = %d)", offset, offset + requiredBytesAfter, bufferSize);
    }

    if (type >= JMP_REL8 && type <= JGE_REL8)
    {
        if (!(instrAddr == 0 && targetAddr == 0))
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: used incorrectly. For short jumps, instrAddr AND targetAddr must be 0!");
        }

        if ((offset + 1) > bufferSize || offset < 1)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: would write beyond end of buffer or offset too small for short jump");
        }

        buffer[offset - 1] = opcode & 0xFF;
        buffer[offset + 0] = shortjmpOffset & 0xFF;
    }
    else if (type == JMP_REL32 || type == CALL_REL32)
    {
        if (instrAddr == 0 || targetAddr == 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: instrAddr or targetAddr is 0 for rel32 jump/call.");
        }

        if (shortjmpOffset != 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: shortjmpOffset must be 0 for long jumps/calls.");
        }

        if (targetAddr >= instrAddr && targetAddr < (instrAddr + 5))
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: JMP/CALL cannot jump into its own opcode/offset bytes.");
        }

        buffer[offset - 1] = opcode & 0xFF;

        int rel32 = targetAddr - (instrAddr + 5);
        buffer[offset + 0] = (rel32 >> 0) & 0xFF;
        buffer[offset + 1] = (rel32 >> 8) & 0xFF;
        buffer[offset + 2] = (rel32 >> 16) & 0xFF;
        buffer[offset + 3] = (rel32 >> 24) & 0xFF;
    }
    else if (type >= JE_REL32 && type <= JGE_REL32)
    {
        if (instrAddr == 0 || targetAddr == 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: instrAddr or targetAddr is 0 for long conditional jump.");
        }

        if (targetAddr >= instrAddr && targetAddr < (instrAddr + 6))
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: Conditional jump cannot target its own bytes.");
        }

        buffer[offset - 2] = 0x0F;
        buffer[offset - 1] = opcode & 0xFF;

        int rel32 = targetAddr - (instrAddr + 6);
        buffer[offset + 0] = (rel32 >> 0) & 0xFF;
        buffer[offset + 1] = (rel32 >> 8) & 0xFF;
        buffer[offset + 2] = (rel32 >> 16) & 0xFF;
        buffer[offset + 3] = (rel32 >> 24) & 0xFF;
    }
    else if (type == JMP_ABS32)
    {
        if (targetAddr == 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: JMP_ABS32 requires targetAddr to be non-zero (absolute address). instrAddr must be 0.");
        }
        if (instrAddr != 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: JMP_ABS32 expects instrAddr to be 0 (not used in absolute mode). Use 0 explicitly.");
        }

        if (offset < 2 || (offset + 4) > bufferSize)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: bounds error for JMP_ABS32");
        }

        buffer[offset - 2] = 0xFF;
        buffer[offset - 1] = 0x25;

        buffer[offset + 0] = (targetAddr >> 0) & 0xFF;
        buffer[offset + 1] = (targetAddr >> 8) & 0xFF;
        buffer[offset + 2] = (targetAddr >> 16) & 0xFF;
        buffer[offset + 3] = (targetAddr >> 24) & 0xFF;
    }
    else if (type == CALL_ABS32)
    {
        if (targetAddr == 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: CALL_ABS32 requires targetAddr to be non-zero (absolute address). instrAddr must be 0.");
        }
        if (instrAddr != 0)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: CALL_ABS32 expects instrAddr to be 0 (not used in absolute mode). Use 0 explicitly.");
        }

        if (offset < 2 || (offset + 4) > bufferSize)
        {
            SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: bounds error for CALL_ABS32");
        }

        buffer[offset - 2] = 0xFF;
        buffer[offset - 1] = 0x15;

        buffer[offset + 0] = (targetAddr >> 0) & 0xFF;
        buffer[offset + 1] = (targetAddr >> 8) & 0xFF;
        buffer[offset + 2] = (targetAddr >> 16) & 0xFF;
        buffer[offset + 3] = (targetAddr >> 24) & 0xFF;
    }
    else
    {
        SetFailState("BytePatcher.inc(WriteJMPorCALLInstruction) ERROR: Unhandled jump type %d", type);
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

stock void BytePatcherCustom(int[] buffer, int bufferSize, int offset, int[] customBytes, int customSize)
{
    if (customSize <= 0)
    {
        SetFailState("Bytepatcher.inc(BytePatcherCustom) CRITICALERROR: custom byte count must be > 0");
    }

    if ((offset < 0) || (offset + customSize > bufferSize))
    {
        SetFailState("Bytepatcher.inc(BytePatcherCustom) CRITICALERROR: write would exceed buffer bounds (offset = %d, size = %d, max = %d)", offset, customSize, bufferSize);
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
    PrintToServer("===============================================");
    PrintToServer("==== Called Bytepatcher.inc(ViewMemoryBytes) ==");
    PrintToServer("===============================================");
    PrintToServer("=== Memory View: 0x%08X → 0x%08X ===", startAddr, endAddr);

    for (int offset = 0; offset < count; offset += 16)
    {
        int lineCount = (offset + 16 <= count) ? 16 : (count - offset);

        hexLine[0] = '\0';
        asciiLine[0] = '\0';

        for (int i = 0; i < lineCount; i++)
        {
            int byteVal = LoadFromAddress(startAddr + view_as<Address>(offset + i), NumberType_Int8) & 0xFF;
            bytes[i] = byteVal;

            // Build hex part
            char temp[16];

            if (mode == 1)
            {
                Format(temp, sizeof(temp), "0x%02X", byteVal);
            }
            else if (mode == 2)
            {
                Format(temp, sizeof(temp), "\\x%02X", byteVal);
            }
            else
            {
                Format(temp, sizeof(temp), "%02X", byteVal);
            }

            StrCat(hexLine, sizeof(hexLine), temp);

            bool isLast = (i == lineCount - 1);
            bool atGroupEnd = ((i + 1) % 8 == 0);

            if (!isLast && !compress)
            {
                StrCat(hexLine, sizeof(hexLine), (atGroupEnd ? "  " : " "));
            }

            // Build ASCII part
            asciiLine[i] = (byteVal >= 32 && byteVal <= 126) ? byteVal : '.';
        }

        asciiLine[lineCount] = '\0';

        if (showchars)
        {
            PrintToServer("[0x%08X] %s", startAddr + view_as<Address>(offset), asciiLine);
        }
        else
        {
            PrintToServer("[0x%08X] %-64s | %s", startAddr + view_as<Address>(offset), hexLine, asciiLine);
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
        PrintToServer("BytePatcher.inc(BytesToAscii): ERROR: output buffer too small");
    }

    int limit = (count < (outSize - 1)) ? count : (outSize - 1);

    for (int i = 0; i < limit; i++)
    {
        out[i] = (buffer[i] >= 32 && buffer[i] <= 126) ? buffer[i] : '.';
    }

    out[limit] = '\0';

    if (doDebug_BytepatcherINC) PrintToServer("BytePatcher.inc(BytesToAscii): DEBUG: converted %d bytes to ASCII string", limit);
}

// Checks if a given address is likely valid.
// Rejects null and very low addresses (e.g., page zero).
// Returns true if the address is above 0x1000.
// Warns (and returns false) if the address is likely invalid.
stock bool IsValidAddress(Address addr)
{
    if (addr <= view_as<Address>(0x1000))
    {
        return false;
    }

    if (doDebug_BytepatcherINC) PrintToServer("BytePatcher.inc(IsValidAddress): 0x%08X is likely valid", addr);
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
        PrintToServer("BytePatcher.inc(CopyBytes) ERROR: byte count must be > 0");
        return false;
    }

    if (!IsValidAddress(src))
    {
        PrintToServer("BytePatcher.inc(CopyBytes) ERROR: source address 0x%08X is invalid", src);
        return false;
    }

    if (!IsValidAddress(dst))
    {
        PrintToServer("BytePatcher.inc(CopyBytes) ERROR: destination address 0x%08X is invalid", dst);
        return false;
    }

    for (int i = 0; i < count; i++)
    {
        int val = LoadFromAddress(src + view_as<Address>(i), NumberType_Int8);
        StoreToAddress(dst + view_as<Address>(i), val, NumberType_Int8);
    }

    if (doDebug_BytepatcherINC) PrintToServer("BytePatcher.inc(CopyBytes): copied %d bytes from 0x%08X to 0x%08X", count, src, dst);
    return true;
}

// Gets the starting address of the memory page containing 'addr'.
stock Address GetMemoryPageStart(Address addr)
{
    return addr & view_as<Address>(~0xFFF);
}

// Gets the ending address of the memory page containing 'addr'.
stock Address GetMemoryPageEnd(Address addr)
{
    return (addr & view_as<Address>(~0xFFF)) + view_as<Address>(0xFFF);
}

// Gets the offset of 'addr' relative to a memory page.
// If reverse is false: offset from page start.
// If reverse is true: offset from page end (may be negative).
stock int GetMemoryPageByteOffset(Address addr, Address page, bool reverse = false)
{
    if (!reverse)
    {
        return addr - page;
    }
    else
    {
        return addr - (page + view_as<Address>(0xFFF));
    }
}

// Verifies that a provided address region stays entirely within one memory page.
// Typically you would get the address of the first byte of the codecave and tack on
// the number of bytes the codecave has as length.
stock bool VerifyMemoryRegionSinglePage(Address addr, int length)
{
    if (length <= 0)
    {
        return false;
    }

    Address startPage = GetMemoryPageStart(addr);
    Address endPage = GetMemoryPageStart(addr + (view_as<Address>(length) - view_as<Address>(1)));

    return (startPage == endPage);
}
