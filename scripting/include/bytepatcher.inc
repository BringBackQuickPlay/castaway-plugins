// === BytePatcher Include File ===
// By VerdiusArcana (lol ty chatgpt)
// Drop this into your includes folder as bytepatcher.inc
// Credits: VerdiusArcana (with assistance from ChatGPT)

// desc: Used to manipulate the memory of a char array, 
// typically to intentionally corrupt it in order to
// have it be treated as actual CPU instructions.
// but can also be used to just store data if neccesary.
// Hard Depends on SourceScramble to function because
// it needs the "Address of Natives" functionality.
// to get started with code cave creation.

// Explanation of what this does and why it's neccesary, this is gonna be a wild ride
// and if you don't care and just want to read the script then start reading from line 92.

// So bytepatcher.inc gives you the ability
// to create a artificial codecave of sorts.
// you'd then use SourceScramble to patch 5 bytes somewhere in the function you want to
// change to jump to our codecave, run your logics etc and then jump back from the codecave
// back to somewhere appropiate in the function.

// This comes with some caveats, it's far less stable than sourcescramble itself
// so you absolutely need to know what you are doing before you use this to write patches.
// It was originally written for the purposes of enabling more advanced reverts
// for castaway.tf where it would be impractical or outright impossible
// to make the revert using only SourceScramble.

// The primary example of this being needed that serves well is to revert
// the hp regen rampup on the Amputator in Team Fortress 2, so it always provides 3+hp/second
// even if you have recently taken damage.

// Because the cozy camper and the concheror depends on the same function for thier
// hp regen as the amputator, it's not possible to patch the function with a normal
// SourceScramble patch without also effecting these two items.
// The other option is to detour with dhooks and recreate the functions from scratch, if these
// functions make alot of function calls to other parts of the server or source engine and then they too also make function calls, this
// quickly leads to having to create a massive amount of overhead.

// Thus, making a memorypatch is hundreds of times more effective, performant
// and keeps overhead minimal. Despite how scary HEX looks,
// it's also far more simple to do it this way rather than to deal with the headache of recreating functions with dhooks.
// Much like any tools a programmer has, it's up to you to decide what's best for the job.
// Sometimes dhooks is just the more performant solution.
// Sometimes, you need to be like the medic from TF2, get that scalpel out and create
// your monstrosity. :)

// To solve this issue (and many other future unknown needs), one needs
// to effectively write a custom form of detour that 
// allows the patchmaker to write custom logic in assembly.
// This can also be done with SourceScramble, but it gets annoying
// and cumbersome to do this once a patch goes above maybe 30 bytes.

// This is where bytepatcher.inc comes in, it provides the tools
// to setup a custom codecave that allows each plugin to write 
// some 4000 bytes of instructions (which is way more than actually neccesary)
// by making a initial char byte array that is 8kb.
// the reason this is done, is to get a memorypage aligned codecave, this is 
// important because the code cave will live among what is traditionally the
// data part of the programs memory, thus is will not be executeable by default.
// setting a data page to execute without being sure what it contains
// can be a recipe for disaster if someone accidently jumps to it.

// By ensuring that we are guaranteed to have a memorypage aligned
// space for our codecave, we can set execute on that page only
// meaning instead of possible undefined crashes, it will only crash
// if the patchmaker has made a bad patch, or if the game updates
// and changes something relating to the functions we patch in the codecave,
// instead of random data being set to execute and corrupting stuff.

// The premise is simple: when you create the char array we will use
// for our codecave in Sourcemod, we are virtually guaranteed to be within a 2gb+- jump.
// thus we need to sacrifice merely 5 bytes to jump to our codecave from the function we intend to patch.
// registers etc are kept intact because the CPU really does not give a shit that you
// jumped to another part of memory, it's just as if the function is simply longer.
// And lots of reverts will be possible simply by reading these registers, it's rare that
// you will have to clean up a register by yourself as 99%
// of patches that will use this bytepatcher.inc functionality will just be a bunch of if statements.

// In the codecave, the patchmaker has to ensure that whatever instructions were sacrificed for the jump are recreated
// and compliant with what the original function was doing with the registers and all that funny stuff.
// One could kinda see it as if we took two bricks, moved them aside and inserted
// a empty brick in the middle, allowing us to essentially extend the amount of space we can write instructions into.

// You will primarily make use of the ability to load a external .txt file 
// to setup the initial patch, use ReplaceWildCards to quickly insert
// addresses to values you need or other stuff,
// and finally make use of the other functions for further fine tuning.

// I hope this proves to be usefull for server owners in general. / VerdiusArcana

// var creation instructions in main plugin (be that reverts.sp or some other plugin)
// When making MemoryPatch vars, Addresses etc for use with bytepatcher, always have a 
// "BP_" to indicate "BytePatcher" to seperate more normal sourcescramble
// patches from the more advanced usage of bytepatcher. 
// This is for visual clarity, and you know then that you can just start typing BP_
// to get a list of all BytePatcher related vars.

// for example: If we need to store the bytes for the offset of a member field for active gun.
// you'd call the int something like "BP_OffsetToActiveWeaponOfPlayer"
// to make it clear that the var is to be used for bytepatcher purposes and not merely
// normal sourcescramble patching.

// Template for codecave (this is if you intend to use bytepatcher.inc in your own plugin)

// enum struct BytePatcher_CodeCave {
//     char g_8kbforcodecave[8192];
//     Address AddressOf_g_8kbforcodecave; // Address of the 8kb, is only used to find where to start looking for codecave.
//     Address AddressOf_codecave; // The actual start of the codecave (memorypage) within that 8kb region.
//     int OffsetOf_codecave_start; // The offset of the start of the codecave (memorypage) relative to the start of the 8kb region
//     Address AddressOf_codecave_startbyte; //Address of first useable byte after the start signature for the codecave
//     int OffsetOf_codecave_startbyte; // The offset of the start of the first useable byte (memorypage) relative to the start of the 8kb region
// }

// BytePatcher_CodeCave myplugin_codecave;

// Change "myplugin" to the name of your plugin file.

// Debug var. if true then print debug lines:
bool doDebug_BytepatcherINC = true;

// To use in a plugin: 

// --- Short Jump Types (rel8) ---
#define JMP_REL8      1   // unconditional
#define JE_REL8       2   // == (ZF=1)
#define JNE_REL8      3   // != (ZF=0)
#define JL_REL8       4   // <  (signed)
#define JLE_REL8      5   // <= (signed)
#define JG_REL8       6   // >  (signed)
#define JGE_REL8      7   // >= (signed)
#define JA_REL8       8   // >  (unsigned)
#define JAE_REL8      9   // >= (unsigned)
#define JB_REL8      10   // <  (unsigned)
#define JBE_REL8     11   // <= (unsigned)

#define JUMP_TYPE_REL8_MIN JMP_REL8
#define JUMP_TYPE_REL8_MAX JBE_REL8

// --- Long Jump/Call Types (rel32) ---
#define JMP_REL32     20  // unconditional near jump
#define CALL_REL32    21  // near relative call
#define JE_REL32      22  // == (ZF=1)
#define JNE_REL32     23  // != (ZF=0)
#define JL_REL32      24  // <  (signed)
#define JLE_REL32     25  // <= (signed)
#define JG_REL32      26  // >  (signed)
#define JGE_REL32     27  // >= (signed)
#define JA_REL32      28  // >  (unsigned)
#define JAE_REL32     29  // >= (unsigned)
#define JB_REL32      30  // <  (unsigned)
#define JBE_REL32     31  // <= (unsigned)

#define JUMP_TYPE_REL32_MIN JMP_REL32
#define JUMP_TYPE_REL32_MAX JBE_REL32

// --- Absolute JMP/CALL (FF /r encoding) ---
#define JMP_ABS32     100 // jmp [absAddr]
#define CALL_ABS32    101 // call [absAddr]

// --- Jump Opcode Resolver ---
stock int GetJumpOpcode(int type)
{
    switch (type)
    {
        // --- Short jumps (rel8) ---
        case JMP_REL8:   return 0xEB;
        case JE_REL8:    return 0x74;
        case JNE_REL8:   return 0x75;
        case JL_REL8:    return 0x7C;
        case JLE_REL8:   return 0x7E;
        case JG_REL8:    return 0x7F;
        case JGE_REL8:   return 0x7D;
        case JA_REL8:    return 0x77;
        case JAE_REL8:   return 0x73;
        case JB_REL8:    return 0x72;
        case JBE_REL8:   return 0x76;

        // --- Relative near jumps (rel32) ---
        case JMP_REL32:  return 0xE9;
        case CALL_REL32: return 0xE8;
        case JE_REL32:   return 0x84;
        case JNE_REL32:  return 0x85;
        case JL_REL32:   return 0x8C;
        case JLE_REL32:  return 0x8E;
        case JG_REL32:   return 0x8F;
        case JGE_REL32:  return 0x8D;
        case JA_REL32:   return 0x87;
        case JAE_REL32:  return 0x83;
        case JB_REL32:   return 0x82;
        case JBE_REL32:  return 0x86;

        default: return -1;
    }
}


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

// --- SetupCodeCave ---

// Sets up the codecave within a provided 8192-byte char buffer.
// Populates the given codecave struct with everything needed
// please change startSignature and endSignature to have the name of your 
// plugin file. For example if your .sp is named myplugin then
// call the function and have the parameters be:
// startSignature = MYPLUGIN-ÍÍ-CODE-CAVE-START
// endSignature = MYPLUGIN-ÍÍ-CODE-CAVE-END
// The reason we include a signature is to make the start and end of the codecave easy to identify.
// and debug.
// it also allows other plugins to quickly find your codecave should they ever need to.

stock void SetupCodeCave(char buffer[8192], BytePatcher_CodeCave codecave, const char[] startSignature, const char[] endSignature)
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
        SetFailState("bytepatcher.inc(SetupCodeCave): ERROR: logic error — final size is %d, expected 4096", offset);
    }

    // Populate codecave struct
    codecave.AddressOf_g_8kbforcodecave = baseAddr;
    codecave.AddressOf_codecave = aligned;
    codecave.OffsetOf_codecave_start = offsetToAligned;
    codecave.AddressOf_codecave_startbyte = afterStart;
    codecave.OffsetOf_codecave_startbyte = offsetAfterStart;


    if (!IsValidAddress(codecave.AddressOf_g_8kbforcodecave) &&
        !IsValidAddress(codecave.AddressOf_codecave) && 
        !IsValidAddress(codecave.AddressOf_codecave_startbyte)) 
    {
        SetFailState(
        "Bytepatcher.inc(SetupCodeCave) CRITICALERROR: The addresses in the enum struct were no good! AddressOf_g_8kbforcodecave:0x%08X AddressOf_codecave:0x%08X AddressOf_codecave_startbyte:0x%08X",
        codecave.AddressOf_g_8kbforcodecave,
        codecave.AddressOf_codecave,
        codecave.AddressOf_codecave_startbyte
        );       
    } else {
        PrintToServer("Bytepatcher.inc is done setting up the codecave!");
        if (doDebug_BytepatcherINC) {
            PrintToServer("====== Codecave Debug Info (SetupCodeCave) ======");
            PrintToServer("8KB base address:               0x%08X", codecave.AddressOf_g_8kbforcodecave);
            PrintToServer("Page-aligned codecave address:  0x%08X", codecave.AddressOf_codecave);
            PrintToServer("Offset to codecave start:       %d",     codecave.OffsetOf_codecave_start);
            PrintToServer("Usable byte address:            0x%08X", codecave.AddressOf_codecave_startbyte);
            PrintToServer("Offset to usable byte:          %d",     codecave.OffsetOf_codecave_startbyte);
            PrintToServer("=================================================");
        }
    }



    // Check that codecave is within it's own memorypage:

    if (!VerifyMemoryRegionSinglePage(codecave.AddressOf_codecave,4096)) {
        SetFailState("Bytepatcher.inc(SetupCodeCave) CRITICALERROR: CodeCave did not have it's own memorypage!");
    } else {
        if (doDebug_BytepatcherINC) PrintToServer("Bytepatcher.inc(SetupCodeCave) SUCCESS: CodeCave does have have it's own memorypage!");
    }
}






// ========= Jumps and call writing helpers =============


/**
 * Writes a 2-byte short jump instruction (rel8).
 *
 * @param type              The jump type (e.g. JMP_REL8, JE_REL8)
 * @param rel8              Signed offset (-128 to 127) to the jump target from next instruction
 * @param codecave  The codecave context containing the buffer and offset info
 * @param byteToWriteAt     1-based index into the codecave where the instruction starts
 */
stock void WriteRel8Jump(int type, int rel8, BytePatcher_CodeCave codecave, int byteToWriteAt)
{
    if (byteToWriteAt <= 0)
        SetFailState("bytepatcher.inc(WriteRel8Jump) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");

    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);
    int bufferSize = sizeof(codecave.g_8kbforcodecave);

    if (rel8 < -128 || rel8 > 127)
        SetFailState("bytepatcher.inc(WriteRel8Jump) ERROR: rel8 offset %d out of range", rel8);

    int opcode = GetJumpOpcode(type);
    if (opcode == -1 || type < JUMP_TYPE_REL8_MIN || type > JUMP_TYPE_REL8_MAX)
        SetFailState("bytepatcher.inc(WriteRel8Jump) ERROR: Invalid short jump type: %d", type);

    if ((offset + 2) > bufferSize)
        SetFailState("bytepatcher.inc(WriteRel8Jump) ERROR: Out of bounds (start=%d, need=2, buffer=%d)", offset, bufferSize);

    codecave.g_8kbforcodecave[offset + 0] = opcode & 0xFF;
    codecave.g_8kbforcodecave[offset + 1] = rel8 & 0xFF;
}

/**
 * Writes a 5-byte near JMP (rel32).
 *
 * @param targetAddr        Absolute address to jump to
 * @param codecave  The codecave context containing the buffer and offset info
 * @param byteToWriteAt     1-based index into the codecave where the instruction starts
 */
stock void WriteRel32Jump(int targetAddr, BytePatcher_CodeCave codecave, int byteToWriteAt)
{
    if (byteToWriteAt <= 0)
        SetFailState("bytepatcher.inc(WriteRel32Jump) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");

    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);
    int bufferSize = sizeof(codecave.g_8kbforcodecave);

    if ((offset + 5) > bufferSize)
        SetFailState("bytepatcher.inc(WriteRel32Jump) ERROR: Out of bounds (start=%d, need=5, buffer=%d)", offset, bufferSize);

    Address instrAddr = codecave.AddressOf_codecave_startbyte + view_as<Address>(byteToWriteAt - 1);
    int rel32 = targetAddr - (view_as<int>(instrAddr) + 5);

    codecave.g_8kbforcodecave[offset + 0] = 0xE9;
    codecave.g_8kbforcodecave[offset + 1] = (rel32 >> 0) & 0xFF;
    codecave.g_8kbforcodecave[offset + 2] = (rel32 >> 8) & 0xFF;
    codecave.g_8kbforcodecave[offset + 3] = (rel32 >> 16) & 0xFF;
    codecave.g_8kbforcodecave[offset + 4] = (rel32 >> 24) & 0xFF;
}

/**
 * Writes a 5-byte near CALL (rel32).
 *
 * @param targetAddr        Absolute address to call
 * @param codecave  The codecave context containing the buffer and offset info
 * @param byteToWriteAt     1-based index into the codecave where the instruction starts
 */
stock void WriteRel32Call(int targetAddr, BytePatcher_CodeCave codecave, int byteToWriteAt)
{
    if (byteToWriteAt <= 0)
        SetFailState("bytepatcher.inc(WriteRel32Call) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");

    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);
    int bufferSize = sizeof(codecave.g_8kbforcodecave);

    if ((offset + 5) > bufferSize)
        SetFailState("bytepatcher.inc(WriteRel32Call) ERROR: Out of bounds (start=%d, need=5, buffer=%d)", offset, bufferSize);

    Address instrAddr = codecave.AddressOf_codecave_startbyte + view_as<Address>(byteToWriteAt - 1);
    int rel32 = targetAddr - (view_as<int>(instrAddr) + 5);

    codecave.g_8kbforcodecave[offset + 0] = 0xE8;
    codecave.g_8kbforcodecave[offset + 1] = (rel32 >> 0) & 0xFF;
    codecave.g_8kbforcodecave[offset + 2] = (rel32 >> 8) & 0xFF;
    codecave.g_8kbforcodecave[offset + 3] = (rel32 >> 16) & 0xFF;
    codecave.g_8kbforcodecave[offset + 4] = (rel32 >> 24) & 0xFF;
}

/**
 * Writes a 6-byte conditional near jump (0F 8x rel32).
 *
 * @param targetAddr        Absolute address to jump to
 * @param type              Conditional jump type (JE_REL32, JNE_REL32, etc.)
 * @param codecave  The codecave context containing the buffer and offset info
 * @param byteToWriteAt     1-based index into the codecave where the instruction starts
 */
stock void WriteRel32CondJump(int targetAddr, int type, BytePatcher_CodeCave codecave, int byteToWriteAt)
{
    if (byteToWriteAt <= 0)
        SetFailState("bytepatcher.inc(WriteRel32CondJump) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");

    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);
    int bufferSize = sizeof(codecave.g_8kbforcodecave);

    int opcode = GetJumpOpcode(type);
    if (opcode == -1 || type < JE_REL32 || type > JBE_REL32)
        SetFailState("bytepatcher.inc(WriteRel32CondJump) ERROR: Invalid conditional jump type: %d", type);

    if ((offset + 6) > bufferSize)
        SetFailState("bytepatcher.inc(WriteRel32CondJump) ERROR: Out of bounds (start=%d, need=6, buffer=%d)", offset, bufferSize);

    Address instrAddr = codecave.AddressOf_codecave_startbyte + view_as<Address>(byteToWriteAt - 1);
    int rel32 = targetAddr - (view_as<int>(instrAddr) + 6);

    codecave.g_8kbforcodecave[offset + 0] = 0x0F;
    codecave.g_8kbforcodecave[offset + 1] = opcode & 0xFF;
    codecave.g_8kbforcodecave[offset + 2] = (rel32 >> 0) & 0xFF;
    codecave.g_8kbforcodecave[offset + 3] = (rel32 >> 8) & 0xFF;
    codecave.g_8kbforcodecave[offset + 4] = (rel32 >> 16) & 0xFF;
    codecave.g_8kbforcodecave[offset + 5] = (rel32 >> 24) & 0xFF;
}

/**
 * Writes a 6-byte absolute JMP [addr].
 *
 * @param targetAddr        Absolute address to jump to (placed into pointer operand)
 * @param codecave  The codecave context containing the buffer and offset info
 * @param byteToWriteAt     1-based index into the codecave where the instruction starts
 */
stock void WriteAbs32Jump(int targetAddr, BytePatcher_CodeCave codecave, int byteToWriteAt)
{
    if (byteToWriteAt <= 0)
        SetFailState("bytepatcher.inc(WriteAbs32Jump) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");

    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);
    int bufferSize = sizeof(codecave.g_8kbforcodecave);

    if ((offset + 6) > bufferSize)
        SetFailState("bytepatcher.inc(WriteAbs32Jump) ERROR: Out of bounds (start=%d, need=6, buffer=%d)", offset, bufferSize);

    codecave.g_8kbforcodecave[offset + 0] = 0xFF;
    codecave.g_8kbforcodecave[offset + 1] = 0x25;
    codecave.g_8kbforcodecave[offset + 2] = (targetAddr >> 0) & 0xFF;
    codecave.g_8kbforcodecave[offset + 3] = (targetAddr >> 8) & 0xFF;
    codecave.g_8kbforcodecave[offset + 4] = (targetAddr >> 16) & 0xFF;
    codecave.g_8kbforcodecave[offset + 5] = (targetAddr >> 24) & 0xFF;
}

/**
 * Writes a 6-byte absolute CALL [addr].
 *
 * @param targetAddr        Absolute address to call (placed into pointer operand)
 * @param codecave  The codecave context containing the buffer and offset info
 * @param byteToWriteAt     1-based index into the codecave where the instruction starts
 */
stock void WriteAbs32Call(int targetAddr, BytePatcher_CodeCave codecave, int byteToWriteAt)
{
    if (byteToWriteAt <= 0)
        SetFailState("bytepatcher.inc(WriteAbs32Call) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");

    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);
    int bufferSize = sizeof(codecave.g_8kbforcodecave);

    if ((offset + 6) > bufferSize)
        SetFailState("bytepatcher.inc(WriteAbs32Call) ERROR: Out of bounds (start=%d, need=6, buffer=%d)", offset, bufferSize);

    codecave.g_8kbforcodecave[offset + 0] = 0xFF;
    codecave.g_8kbforcodecave[offset + 1] = 0x15;
    codecave.g_8kbforcodecave[offset + 2] = (targetAddr >> 0) & 0xFF;
    codecave.g_8kbforcodecave[offset + 3] = (targetAddr >> 8) & 0xFF;
    codecave.g_8kbforcodecave[offset + 4] = (targetAddr >> 16) & 0xFF;
    codecave.g_8kbforcodecave[offset + 5] = (targetAddr >> 24) & 0xFF;
}

// Methods to get the values as a int, used to 

/**
 * Calculates a rel8 offset for a short jump.
 *
 * @param fromAddr   Absolute address of the jump instruction.
 * @param targetAddr Absolute address of the jump destination.
 * @return Signed rel8 offset (-128 to 127) as a integer. 
 */
stock int GetRel8JumpValue(int fromAddr, int targetAddr)
{
    int rel8 = targetAddr - (fromAddr + 2);
    if (rel8 < -128 || rel8 > 127)
        SetFailState("bytepatcher.inc(GetRel8JumpValue) ERROR: rel8 offset %d out of range", rel8);
    return rel8;
}


/**
 * Calculates a rel32 offset for any 5-byte rel32 instruction (JMP, CALL, or similar).
 *
 * @param fromAddr   Absolute address of the instruction.
 * @param targetAddr Absolute address of the destination.
 * @return Signed rel32 offset as a integer.
 */
stock int GetRel32InstructionValue(int fromAddr, int targetAddr)
{
    return targetAddr - (fromAddr + 5);
}



/**
 * Calculates a rel32 offset for a conditional near jump (0F 8x rel32).
 *
 * @param fromAddr   Absolute address of the jump instruction.
 * @param targetAddr Absolute address of the jump destination.
 * @return Signed rel32 offset as a integer.
 */
stock int GetRel32CondJumpValue(int fromAddr, int targetAddr)
{
    return targetAddr - (fromAddr + 6);
}



// ======================================================


/**
 * Writes a sequence of bytes into the codecave starting at a specific byte offset.
 *
 * @param codecave  The codecave context with buffer and offset info.
 * @param customBytes[]     The array of bytes to write.
 * @param customSize        Number of bytes to write from customBytes (use sizeof(patch))
 * @param byteToWriteAt     1-based offset into the codecave (e.g., 1 = first usable byte)
 */
stock void WriteBytesToCodecave(BytePatcher_CodeCave codecave, int[] customBytes, int customSize, int byteToWriteAt)
{
    if (customSize <= 0)
    {
        SetFailState("bytepatcher.inc(BytePatcherCustomToCodecave) ERROR: custom byte count must be > 0");
    }

    if (byteToWriteAt <= 0)
    {
        SetFailState("bytepatcher.inc(BytePatcherCustomToCodecave) ERROR: byteToWriteAt is 0 — that would then become -1 and that's no good!");
    }

    int bufferSize = sizeof(codecave.g_8kbforcodecave);
    int offset = codecave.OffsetOf_codecave_startbyte + (byteToWriteAt - 1);

    if ((offset + customSize) > bufferSize)
    {
        SetFailState("bytepatcher.inc(BytePatcherCustomToCodecave) ERROR: write would exceed buffer bounds (offset = %d, size = %d, buffer = %d)",
            offset, customSize, bufferSize);
    }

    for (int i = 0; i < customSize; i++)
    {
        codecave.g_8kbforcodecave[offset + i] = customBytes[i] & 0xFF;
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


/**
 * Loads ALL bytes from a patch file into a char array.
 * Ignores comments (//), headers (==), and separators (###).
 *
 * @param filename   Path to file (relative to game folder).
 * @param outBytes[] Array to store bytes into.
 * @param maxBytes   Maximum size of outBytes[] array.
 * @return           Number of bytes read, or -1 on error.
 */
stock int LoadAllBytesFromPatchFile(const char[] filename, int[] outBytes, int maxBytes)
{
    Handle file = OpenFile(filename, "r");
    if (file == INVALID_HANDLE)
    {
        PrintToServer("bytepatcher.inc(LoadAllBytesFromPatchFile) ERROR: Could not open file %s", filename);
        return -1;
    }

    char line[512];
    int byteCount = 0;

    while (!IsEndOfFile(file))
    {
        ReadFileLine(file, line, sizeof(line));
        TrimString(line);

        if (line[0] == '\0' || line[0] == '/' || line[0] == '=' || line[0] == '#')
            continue; // skip empty, comments, header lines, separators

        int len = strlen(line);
        int idx = 0;

        while (idx < len)
        {
            // skip spaces
            while (idx < len && (line[idx] == ' ' || line[idx] == '\t'))
                idx++;

            if (idx + 2 > len)
                break;

            char byteStr[3];
            byteStr[0] = line[idx];
            byteStr[1] = line[idx + 1];
            byteStr[2] = '\0';

            int value = StringToInt(byteStr, 16);

            if (byteCount < maxBytes)
            {
                outBytes[byteCount++] = value & 0xFF;
            }
            else
            {
                PrintToServer("bytepatcher.inc(LoadAllBytesFromPatchFile) ERROR: Patch too large for buffer (max %d)", maxBytes);
                break;
            }

            idx += 2;
        }
    }

    CloseHandle(file);

    if (doDebug_BytepatcherINC)
    {
        PrintToServer("bytepatcher.inc(LoadAllBytesFromPatchFile) Loaded %d bytes from %s", byteCount, filename);
    }

    return byteCount;
}

// ====================================================================================
// ReplaceWildcards (improved version)
//
// Replaces all 0x2A wildcards in patchArray with supplied values.
// If 4 consecutive 0x2A are found, it uses full 4-byte int (little endian).
// Otherwise it replaces single 0x2A with 1-byte value.
//
// Example usage:
// ReplaceWildcards(patch, size,
//      view_as<int>(myAddr),     // replaces 4x 0x2A
//      0x90,                     // replaces 1x 0x2A
//      view_as<int>(myOtherAddr) // replaces 4x 0x2A
// );
// ====================================================================================
stock void ReplaceWildcards(int[] patchArray, int size, int[] args, int argCount)
{
    int argIndex = 0;
    int wildcardCount = 0;

    for (int i = 0; i < size; i++)
    {
        if (patchArray[i] != 0x2A)
            continue;

        wildcardCount++;

        // Check for 4 consecutive wildcards (address placeholder)
        if (i + 3 < size &&
            patchArray[i + 0] == 0x2A &&
            patchArray[i + 1] == 0x2A &&
            patchArray[i + 2] == 0x2A &&
            patchArray[i + 3] == 0x2A)
        {
            if (argIndex >= argCount)
            {
                SetFailState("bytepatcher.inc(ReplaceWildcards) ERROR: not enough args for 4-byte address replacement");
                return;
            }

            int address = args[argIndex++];
            patchArray[i + 0] = (address >> 0) & 0xFF;
            patchArray[i + 1] = (address >> 8) & 0xFF;
            patchArray[i + 2] = (address >> 16) & 0xFF;
            patchArray[i + 3] = (address >> 24) & 0xFF;

            i += 3; // Skip next 3 positions
        }
        else
        {
            if (argIndex >= argCount)
            {
                SetFailState("bytepatcher.inc(ReplaceWildcards) ERROR: not enough args for single-byte replacement");
                return;
            }

            int byteVal = args[argIndex++];
            patchArray[i] = byteVal & 0xFF;
        }
    }

    if (doDebug_BytepatcherINC)
        PrintToServer("bytepatcher.inc(ReplaceWildcards): replaced %d wildcards", wildcardCount);
}

