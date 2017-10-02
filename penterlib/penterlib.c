//
// Copyright 2008-2017 OSR Open Systems Resources, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE
// 
#include "penterlib.h"

BOOLEAN    Initialized;
BOOLEAN    Initializing;
FUNC_TRACE FuncTraces[MAX_FUNC_TRACES];
ULONG_PTR  FuncTracesInUse = 0;
BOOLEAN    ErrorReported;

EX_SPIN_LOCK      FunctionTableLock;
RTL_GENERIC_TABLE FunctionTable;

EX_SPIN_LOCK      ThreadTableLock;
RTL_GENERIC_TABLE ThreadTable;

//
// ****NOTE****
//
// This allows you to define the maximum IRQL at which your code
// will be running. The lowest supported IRQL is
// DISPATCH_LEVEL!! You can make it higher if you want (to
// serialize an ISR, for example) but you CANNOT make it lower.
//
// ****ALSO****
//
// This is a COMPILE TIME decision. You CANNOT change it
// runtime! So, if you want to serialize against the ISR you'll
// need to pick something that's at or above your DIRQL.
//
KIRQL      SynchronizeIrql = DISPATCH_LEVEL;

///////////////////////////////////////////////////////////////////////////////
//
//  TracingLibraryInitialize
//
//      Initialize the library. Called by the first invocation of LogFuncEntry,
//      which will be done by the hook in the main entry point of the module.
//      So, no race conditions...
//
//  INPUTS:
//
//      None.
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None
//
//  IRQL:
//
//      IRQL <= SynchronizeIrql
// 
//  NOTES:
//
///////////////////////////////////////////////////////////////////////////////
VOID
TracingLibraryInitialize(
    VOID) 
{

    //
    // We keep two globals - Initialized and Initializing. This
    // protects us from an infinite loop if we make a call to
    // function with a _penter in its prolog while trying to get set
    // up (we'd come back through here and note that Initialized was
    // false, call the function again, come back through here and
    // note that Initialized is false, come back through here, etc).
    //

    //
    // ***NOTE***
    //
    // Do NOT put ANYthing but this check between here and setting
    // Initializing to TRUE! You're just asking for an infinite
    // recursion if you do...
    //
    if (Initializing == TRUE) {
        //
        // Something that we called in the containing IF block was
        // compiled with our _penter code. Ignore it...
        //
        return;
    }
    Initializing = TRUE;
    //
    // ***END NOTE***
    //

    //
    // Set up our globals.
    //

    FunctionTableInitialize();
    ThreadTableInitialize();

    //
    // Print out a message.
    //
    DbgPrint("*****OSR TICK TRACING BUILD*****\n");

    //
    // We're set up!
    //
    Initialized = TRUE;

    return;
}


///////////////////////////////////////////////////////////////////////////////
//
//  _penter
//
//      This is our penter routine on x86. It's placed at the beginning
//      of all subroutines compiled with the /Gh switch.
//
//
//  INPUTS:
//
//      None.
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None
//
//  IRQL:
//
//      IRQL <= SynchronizeIrql
// 
//  NOTES:
//
///////////////////////////////////////////////////////////////////////////////
#ifdef _X86_
VOID __declspec(naked) _cdecl _penter(VOID) {
    _asm {
        push ebp
        mov  ebp, esp
        pushad            ; Push all of the general purpose registers
                          ; onto the stack

        push esp          ; Push the stack pointer, which now becomes the 
                          ; PENTER_REGISTERS paramter to the C function.

        call LogFuncEntry ; Call our C code that does the logging.

        popad             ; Restore the general purpose registers.

        pop ebp
        ret               ; And get out!
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
//  LogFuncEntry
//
//      In the interest of doing as much in C as possible, the
//      _penter routine calls into this function. This is the
//      code that will record the starting time of the
//      function.
//
//  INPUTS:
//
//      Registers - The register info for the called function.
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None
//
//  IRQL:
//
//      IRQL <= SynchronizeIrql
//
//  NOTES:
//
///////////////////////////////////////////////////////////////////////////////
VOID 
LogFuncEntry(
    PENTER_REGISTERS Registers) 
{

    ULONGLONG             functionAddress;
    PTIME_LOGGER          timeLogger;
    NTSTATUS              status;
    PFUNCTION_TABLE_ENTRY funcTableEntry = NULL;
    PTHREAD_TABLE_ENTRY   threadTableEntry = NULL;
    
    if (Initialized == FALSE) {

        TracingLibraryInitialize();

    }

    //
    // The address that we have is the address where the _penter
    // function will return to. Subtract off 5
    // bytes (on the x86) to get the base address.
    //
#ifdef _X86_
    functionAddress = (Registers->ReturnEip - 5);
#elif _AMD64_

    //
    // A call on the x64 also just so happens to be 5 bytes long
    //
    functionAddress = (Registers->ReturnRip - 5);
#else
    #error "Unsupported architecture"
#endif

    //
    // Each invocation of a subroutine requires a unique tracking
    // structure to track the start and end time. Allocate one from
    // pool.
    //

    // 
    // 6014 - We free this in LogFuncExit
    // 
    // 30030 - Bad form to suppress the executable, but we're a static lib
    // and can't enforce such things
    //
#pragma warning(suppress: 6014)
#pragma warning(suppress: 30030)
    timeLogger = (PTIME_LOGGER)ExAllocatePoolWithTag(NonPagedPool, 
                                                     sizeof(TIME_LOGGER), 
                                                     TIME_LOGGER_TAG);
    if (timeLogger == NULL) {

        //
        // Oops! Not much we can do here...
        //
        DbgPrint("OSRPENTER: Memory allocation failed. Not tracking call\n");

        status = STATUS_INSUFFICIENT_RESOURCES;

        goto Exit;

    }


    // 
    // Function table entries are NEVER FREED. This is by design! They live 
    // until the driver unloads or the system reboots 
    //  
    funcTableEntry = FunctionTableLookupEntry(functionAddress);

    if (funcTableEntry == NULL) {

        DbgPrint("OSRPENTER: Memory allocation failed. Not tracking call\n");

        status = STATUS_INSUFFICIENT_RESOURCES;

        goto Exit;

    }
    
    // 
    // Thread table entries are transient and reference counted. They go away 
    // when we have no further calls outstanding from the thread 
    //  
    threadTableEntry = ThreadTableLookupEntry(PsGetCurrentThreadId(),
                                              LookupActionCreateIfNotFound);

    if (threadTableEntry == NULL) {

        DbgPrint("OSRPENTER: Memory allocation failed. Not tracking call\n");

        status = STATUS_INSUFFICIENT_RESOURCES;

        goto Exit;

    }

    //
    // Store the referenced function table entry
    //
    timeLogger->FunctionEntry = funcTableEntry;

    // 
    // And the referenced thread table entry 
    // 
    timeLogger->ThreadEntry   = threadTableEntry; 

    //
    // Capture the performance counter. 
    //
    timeLogger->StartTicks = KeQueryPerformanceCounter(NULL);

    // 
    // Push onto the thread call list 
    //  
    PushEntryList(&threadTableEntry->CallList,
                  &timeLogger->ListEntry);

    // 
    // Success! 
    //  
    status = STATUS_SUCCESS;

Exit:

    if (!NT_SUCCESS(status)) {

        if (timeLogger != NULL) {

            ExFreePool(timeLogger);

        }

        if (threadTableEntry != NULL) {

            ThreadTableEntryDereference(threadTableEntry);

        }

    }

    return;
}


///////////////////////////////////////////////////////////////////////////////
//
//  _pexit
//
//      This is our peit routine. It's placed at the end
//      of all subroutines compiled with the /GH switch.
//
//
//  INPUTS:
//
//      None.
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None
//
//  IRQL:
//
//      IRQL <= SynchronizeIrql
//
//  NOTES:
//
///////////////////////////////////////////////////////////////////////////////
#ifdef _X86_
VOID __declspec(naked) _cdecl _pexit(VOID) {
    _asm {
        push ebp
        mov  ebp, esp

        pushad            ; Push all of the general purpose registers
                          ; onto the stack

        push esp          ; Push the stack pointer, which now becomes the 
                          ; PENTER_REGISTERS paramter to the C function.

        call LogFuncExit  ; Call our C code that does the logging.

        popad             ; Restore the general purpose registers.
        
        pop ebp
        ret               ; And get out!
    }
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
//  LogFuncExit
//
//      In the interest of doing as much in C as possible, the
//      _pexit routine calls into this function. This is the
//      code that will record the ending time of the function
//      and add the delta to the global table
//
//  INPUTS:
//
//      Registers - The register info for the returning
//      function
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None
//
//  IRQL:
//
//      IRQL <= SynchronizeIrql
//
//  NOTES:
//
///////////////////////////////////////////////////////////////////////////////
VOID 
LogFuncExit(
    PEXIT_REGISTERS Registers) 
{

    LARGE_INTEGER         endTicks;
    PTIME_LOGGER          timeLogger = NULL;
    PTHREAD_TABLE_ENTRY   threadTableEntry = NULL;
    PFUNC_TRACE           funcTrace = NULL;
    PFUNCTION_TABLE_ENTRY funcTableEntry = NULL;

    UNREFERENCED_PARAMETER(Registers);

    //
    // Bail if we're not set up.
    //
    if (Initialized == FALSE) {

        goto Exit;

    }

    //
    // Capture the performance counter at the end of the subroutine
    // call.
    //
    endTicks = KeQueryPerformanceCounter(NULL);

    // 
    // Get the thread table entry for the current thread 
    //  
    threadTableEntry = ThreadTableLookupEntry(PsGetCurrentThreadId(),
                                              LookupActionFailIfNotFound);

    if (threadTableEntry == NULL) {

        DbgPrint("OSRPENTER: Thread table entry not found??\n");

        goto Exit;

    }

    // 
    // Drop the reference that we just received to the table entry from the 
    // lookup...We don't need it, we have a reference to the entry dangling 
    // from the function entry logging routine (we acquire a reference there 
    // and leave it so that the thread tracking structure won't go 
    // away...unfortunately we can't pass that as a context here, so we end 
    // up with this double reference situation) 
    //  
    ThreadTableEntryDereference(threadTableEntry);

    //
    // Get the time logger by popping the call stack queue
    //
    timeLogger = (PTIME_LOGGER)PopEntryList(&threadTableEntry->CallList); 

    // 
    // Get the function table information 
    // 
    funcTableEntry = timeLogger->FunctionEntry; 

    // 
    // And the trace info 
    // 
    funcTrace = funcTableEntry->TraceEntry; 

    //
    // Add the delta in.
    //
    InterlockedExchangeAdd64(
            &funcTrace->CallTicks.QuadPart,
            (endTicks.QuadPart - timeLogger->StartTicks.QuadPart));

    // 
    // Bump the call count 
    //  
    InterlockedIncrement((volatile LONG *)&funcTrace->CallCount);

    //
    // Done!
    //

Exit:

    if (timeLogger != NULL) {

        ExFreePool(timeLogger);

    }

    if (threadTableEntry != NULL) {

        ThreadTableEntryDereference(threadTableEntry);

    }

    return;
}




