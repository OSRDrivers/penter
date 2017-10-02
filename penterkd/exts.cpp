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
//      Based on code taken from the dbgexts sample in the DTFW package.
//      Original copyright follows.
//

/*-----------------------------------------------------------------------------
   Copyright (c) 2000  Microsoft Corporation

Module:
  exts.cpp

  Sampe file showing couple of extension examples

-----------------------------------------------------------------------------*/
#include "dbgexts.h"

void DumpSymbol64(ULONG64 Address);


/*
  modulestats <modulename>

  Print out the module function trace data in CSV format.

*/
HRESULT CALLBACK
modulestats(PDEBUG_CLIENT4 Client, PCSTR args)
{

    ULONG64 funcTracesInUsePtr;
    ULONG64 funcTracesInUse;
    ULONG64 funcTracesBase;
    ULONG64 funcTrace;
    ULONG64 funcTraceSize;
    ULONG64 startAddress;
    ULONG64 callTicks;
    ULONG   callCount;
    char    symbolBuffer[512];
    ULONG64 i;
    HRESULT hr;
    ULONG64 ptrSize;

    UNREFERENCED_PARAMETER(Client);


    // 
    // Figure out the pointer size on the target
    // 
    ptrSize = GetExpression("@$ptrsize");

    //
    // Generate module!FuncTracesInUse
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!FuncTracesInUse", 
                        args);
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the pointer value of the FuncTracesInUse global
    //
    funcTracesInUsePtr = GetExpression(symbolBuffer);

    //
    // Read the pointer to get the number of traces in use.
    //
    ReadPointer(funcTracesInUsePtr, &funcTracesInUse);

    //
    // Generate module!FuncTraces
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!FuncTraces", 
                        args);

    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the base address of the func traces array.
    //
    funcTracesBase = GetExpression(symbolBuffer);

    //
    // Generate module!_FUNC_TRACE
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!_FUNC_TRACE", 
                        args);

    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the size of the structure
    //
    funcTraceSize = GetTypeSize(symbolBuffer);
    if (funcTraceSize == 0) {
        dprintf("Error getting type size\n");
        return S_OK;

    }

    //
    // Print out the CSV header.
    //
    dprintf("Function,CallCount,CallTicks,TicksPerCall\n");

    //
    // Loop over all the in use entries and print out the
    // information
    //
    for (i = 0; i < funcTracesInUse; i++) {
        if (CheckControlC()) {
            return S_OK;
        }

        //
        // Calculate the base address of the entry
        //
        funcTrace = funcTracesBase + (i * funcTraceSize);

        //
        // Set up the pointer so that we can do ReadField calls. This is
        // the same as an InitTypeRead except we get to pass a string as
        // the second parameter
        //
        if (GetShortField(funcTrace, symbolBuffer, 1) != 0) {
            dprintf("Error in reading FUNC_TRACE at %p\n", funcTrace);
            return S_OK;
        }

        //
        // Read the start address
        //
        startAddress = ReadField(StartAddress);

        // 
        // If the target is 32-bit, we must sign extend
        // 
        if (ptrSize == 4) {

            startAddress = ((ULONG64)((LONG)(startAddress)));

        }

        //
        // Read the tick count
        //
        callTicks = ReadField(CallTicks);

        //
        // Read the call count
        //
        callCount = (ULONG)ReadField(CallCount);

        if (callCount == 0) {

            // 
            // Trace must have been reset, ignore this entry 
            // 
            continue; 

        }

        //
        // Get the symbol name for the start address and print it out.
        //
        DumpSymbol64(startAddress);

        //
        // And print out the other fields.
        //
        dprintf(",%d,%I64d,%I64d\n", callCount, callTicks, (callTicks/callCount));

    }

    return S_OK;
}


/*
  resettrace <modulename>

  Reset the module function tracing info

*/
HRESULT CALLBACK
resettrace(PDEBUG_CLIENT4 Client, PCSTR args)
{
    ULONG64 funcTracesInUsePtr;
    ULONG64 funcTracesInUse;
    ULONG64 funcTracesBase;
    ULONG64 funcTrace;
    ULONG64 funcTraceSize;
    ULONG   callTicksOffset;
    ULONG   callCountOffset;
    char    symbolBuffer[512];
    ULONG64 i;
    HRESULT hr;
    ULONG64 ptrSize;

    UNREFERENCED_PARAMETER(Client);


    // 
    // Figure out the pointer size on the target
    // 
    ptrSize = GetExpression("@$ptrsize");

    //
    // Generate module!FuncTracesInUse
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!FuncTracesInUse", 
                        args);
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the pointer value of the FuncTracesInUse global
    //
    funcTracesInUsePtr = GetExpression(symbolBuffer);

    //
    // Read the pointer to get the number of traces in use.
    //
    ReadPointer(funcTracesInUsePtr, &funcTracesInUse);

    //
    // Generate module!FuncTraces
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!FuncTraces", 
                        args);

    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the base address of the func traces array.
    //
    funcTracesBase = GetExpression(symbolBuffer);

    //
    // Generate module!_FUNC_TRACE
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!_FUNC_TRACE", 
                        args);

    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the size of the structure
    //
    funcTraceSize = GetTypeSize(symbolBuffer);
    if (funcTraceSize == 0) {
        dprintf("Error getting type size\n");
        return S_OK;

    }

    //
    // Loop over all the in use entries and zero down the
    // call count and tick count
    //
    for (i = 0; i < funcTracesInUse; i++) {

        //
        // Calculate the base address of the entry
        //
        funcTrace = funcTracesBase + (i * funcTraceSize);

        // 
        // And zero out the fields 
        //  
        GetFieldOffset(symbolBuffer, "CallTicks", &callTicksOffset);
        GetFieldOffset(symbolBuffer, "CallCount", &callCountOffset);

        WritePointer((funcTrace + callTicksOffset), 0);
        WritePointer((funcTrace + callCountOffset), 0);

    }

    dprintf("Module tracing reset.\n");
    return S_OK;
}

HRESULT CALLBACK
callstacks(PDEBUG_CLIENT4 Client, PCSTR args)
{

    ULONG64 funcTracesInUsePtr;
    ULONG64 funcTracesInUse;
    ULONG64 funcTracesBase;
    ULONG64 funcTrace;
    ULONG64 funcTraceSize;
    ULONG64 startAddress;
    char    symbolBuffer[512];
    char    funcTraceSymName[512];
    char    callHistorySymName[512];
    ULONG64 i;
    ULONG64 j;
    ULONG64 callIndex;
    ULONG64 callTotal;
    ULONG64 callHistoryBase;
    ULONG64 callHistory;
    ULONG64 callHistorySize;
    ULONG64 frames;
    ULONG64 framesCount;
    ULONG64 frameAddress;
    ULONG64 framesIndex;
    ULONG64 stackSeenCount;
    ULONG64 totalInvocations;
    HRESULT hr;
    ULONG64 currentEpochPtr;
    LONG    currentEpochVal = 0;
    LONG    epoch;
    ULONG   ptrSize;

    UNREFERENCED_PARAMETER(Client);

    // 
    // Figure out the pointer size on the target
    // 
    if (IsPtr64()) {
        ptrSize = 8;
    } else {
        ptrSize = 4;
    }   

    //
    // Generate module!FuncTracesInUse
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!FuncTracesInUse", 
                        args); 
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }


    //
    // Get the pointer value of the FuncTracesInUse global
    //
    funcTracesInUsePtr = GetExpression(symbolBuffer);

    //
    // Read the pointer to get the number of traces in use.
    //
    ReadPointer(funcTracesInUsePtr, &funcTracesInUse);

    //
    // Generate module!CurrentEpoch
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!CurrentEpoch", 
                        args);
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }

    //
    // Get the pointer value of the CurrentEpoch global
    //
    currentEpochPtr = GetExpression(symbolBuffer);

    if (currentEpochPtr != 0) {
        //
        // Read the pointer to get the current epoch
        //
        ReadMemory(currentEpochPtr, 
                   &currentEpochVal,
                   sizeof(LONG),
                   NULL);
    }


    //
    // Generate module!FuncTraces
    //
    memset(symbolBuffer, 0, sizeof(symbolBuffer));
    hr = StringCbPrintf(symbolBuffer, 
                        sizeof(symbolBuffer)-1, 
                        "%s!FuncTraces", 
                        args); 
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }


    //
    // Get the base address of the func traces array.
    //
    funcTracesBase = GetExpression(symbolBuffer);

    //
    // Generate module!_FUNC_TRACE
    //
    memset(funcTraceSymName, 0, sizeof(funcTraceSymName));
    hr = StringCbPrintf(funcTraceSymName, 
                        sizeof(funcTraceSymName)-1, 
                        "%s!_FUNC_TRACE", 
                        args);
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }


    //
    // Get the size of the structure
    //
    funcTraceSize = GetTypeSize(funcTraceSymName);
    if (funcTraceSize == 0) {
        dprintf("Error getting type size\n");
        return S_OK;

    }

    //
    // Generate module!_CALL_HISTORY
    //
    memset(callHistorySymName, 0, sizeof(callHistorySymName));
    hr = StringCbPrintf(callHistorySymName, 
                        sizeof(callHistorySymName)-1, 
                        "%s!_CALL_HISTORY", 
                        args);
    if (hr != S_OK) {
        dprintf("String error (0x%x)\n", hr);
        return hr;
    }


    //
    // Get the size of the structure
    //
    callHistorySize = GetTypeSize(callHistorySymName);
    if (callHistorySize == 0) {
        dprintf("Error getting type size\n");
        return S_OK;

    }

    //
    // Loop over all the in use entries and print out the
    // information
    //
    for (i = 0; i < funcTracesInUse; i++) {
        if (CheckControlC()) {
            return S_OK;
        }

        //
        // Calculate the base address of the entry
        //
        funcTrace = funcTracesBase + (i * funcTraceSize);

        //
        // Set up the pointer so that we can do ReadField calls. This is
        // the same as an InitTypeRead except we get to pass a string as
        // the second parameter
        //
        if (GetShortField(funcTrace, funcTraceSymName, 1) != 0) {
            dprintf("Error in reading FUNC_TRACE at %p\n", funcTrace);
            return S_OK;
        }

        //
        // Read the start address
        //
        startAddress = ReadField(StartAddress);

        // 
        // Current index in the history
        // 
        callIndex = ReadField(CallHistoryIndex);

        // 
        // Total history buffers
        // 
        callTotal = ReadField(CallHistoryTotal);

        // 
        // And the address of the structures...
        // 
        callHistoryBase = ReadField(CallHistory);

        //
        // Get the symbol name for the start address and print it out.
        //
        DumpSymbol64(startAddress);

        dprintf("\n");

        totalInvocations = 0;

        if (callTotal > callIndex) {
            //
            // We've wrapped around our circular buffer...So, first we'll
            // print index->max, then do 0->(index-1)
            //
            for (j = callIndex; j < MAX_CALL_HISTORY; j++) {

                if (CheckControlC()) {
                    return S_OK;
                }

                callHistory = callHistoryBase + (j * callHistorySize);

                if (GetShortField(callHistory, callHistorySymName, 1) != 0) {
                    dprintf("Error in reading _CALL_HISTORY at %p\n", 
                            callHistory);
                    return S_OK;
                }

                if (currentEpochPtr != 0) {
                    // 
                    // Get the epoch
                    // 
                    epoch = (ULONG)ReadField(Epoch);
    
                    // 
                    // If this isn't a valid entry (i.e. taken before the last
                    // reset), then skip it
                    // 
                    if (epoch != currentEpochVal) {
                        continue;
                    }
                }

                frames = ReadField(Frames);

                framesCount = ReadField(FramesCount);

                stackSeenCount = ReadField(StackSeenCount);
                
                totalInvocations += stackSeenCount;

                dprintf("Stack - Occurred %d times:\n", stackSeenCount);
                for (framesIndex = 0; framesIndex < framesCount; framesIndex++) {

                    ReadPointer((frames + (ptrSize * framesIndex)),
                                &frameAddress);

                    dprintf("\t");
                    DumpSymbol64(frameAddress);
                    dprintf("\n");

                }
                dprintf("\n");

            }
        }

        for (j = 0; j < callIndex; j++) {
            if (CheckControlC()) {
                return S_OK;
            }

            callHistory = callHistoryBase + (j * callHistorySize);

            if (GetShortField(callHistory, callHistorySymName, 1) != 0) {
                dprintf("Error in reading _CALL_HISTORY at %p\n", 
                        callHistory);
                return S_OK;
            }

            if (currentEpochPtr != 0) {
                // 
                // Get the epoch
                // 
                epoch = (ULONG)ReadField(Epoch);
    
                // 
                // If this isn't a valid entry (i.e. taken before the last
                // reset), then skip it
                // 
                if (epoch != currentEpochVal) {
                    continue;
                }
            }

            frames = ReadField(Frames);

            framesCount = ReadField(FramesCount);

            stackSeenCount = ReadField(StackSeenCount);

            totalInvocations += stackSeenCount;

            // 
            // And the epoch
            // 
            epoch = (ULONG)ReadField(Epoch);

            // 
            // If this isn't a valid entry (i.e. taken before the last
            // reset), then skip it
            // 
            if (epoch != currentEpochVal) {
                continue;
            }

            dprintf("Stack - Occurred %d times:\n", stackSeenCount);

            for (framesIndex = 0; framesIndex < framesCount; framesIndex++) {

                ReadPointer((frames + (ptrSize * framesIndex)),
                            &frameAddress);

                dprintf("\t");
                DumpSymbol64(frameAddress);
                dprintf("\n");

            }
            dprintf("\n");
        }

        dprintf("%d invocations total\n", totalInvocations);
        dprintf("*****************************************************\n");
        dprintf("-----------------------------------------------------\n");
        dprintf("\n\n");

    }

    return S_OK;
}


/*
  A built-in help for the extension dll
*/
HRESULT CALLBACK
help(PDEBUG_CLIENT4 Client, PCSTR args)
{
    INIT_API();

    UNREFERENCED_PARAMETER(args);

    dprintf("Help for penterexts.dll\n"
            "  modulestats <module> - Display the function stats for module\n"
            "  callstacks  <module> - Display the call stack stats\n"
            "  resettrace  <module> - Reset the function stats for module\n"
            "  help                 - Shows this help\n"
            );
    EXIT_API();

    return S_OK;
}

//
// DumpSymbol
//
//  Find the symbol closest to Address and 
//   display it.
//
void DumpSymbol64(ULONG64 Address) {

    char    symbolBuffer[512];
    ULONG64 offset;

    GetSymbol(Address, symbolBuffer, &offset);
    if (symbolBuffer[0] != '\0') {
        
        dprintf("%s", symbolBuffer);
        
    } else {

        dprintf("%p", Address);

    }
   
    return;   
    
}
