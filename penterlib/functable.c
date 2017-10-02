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

/////////////////
// GLOBAL DATA //
/////////////////


//////////////////////
// MODULE FUNCTIONS //
//////////////////////

VOID
FunctionTableInitialize(
    VOID) 
{

    RtlInitializeGenericTable(&FunctionTable,
                              FunctionTableCompareRoutine,
                              FunctionTableAllocateRoutine,
                              FunctionTableFreeRoutine,
                              NULL);
    return;
}


PFUNCTION_TABLE_ENTRY
FunctionTableLookupEntry(
    ULONGLONG FunctionAddress) 
{

    KIRQL                 oldIrql;
    FUNCTION_TABLE_ENTRY  lookupEntry = {0};
    PFUNCTION_TABLE_ENTRY foundEntry = NULL;
    PFUNC_TRACE           funcTrace = NULL;
    BOOLEAN               createdEntry;
    BOOLEAN               lockedShared;


    lookupEntry.FunctionAddress = FunctionAddress;

    KeRaiseIrql(SynchronizeIrql, &oldIrql);
    ExAcquireSpinLockSharedAtDpcLevel(&FunctionTableLock);
    lockedShared = TRUE;

    foundEntry = 
        (PFUNCTION_TABLE_ENTRY)RtlLookupElementGenericTable(&FunctionTable,
                                                            &lookupEntry);

    if (foundEntry != NULL) {
        // 
        // Already in the table, nothing to do 
        //  
        goto Exit;

    }

    ExReleaseSpinLockSharedFromDpcLevel(&FunctionTableLock);
    lockedShared = FALSE;

    ExAcquireSpinLockExclusiveAtDpcLevel(&FunctionTableLock);

    foundEntry = 
        (PFUNCTION_TABLE_ENTRY)RtlInsertElementGenericTable(
                                                   &FunctionTable,
                                                   &lookupEntry,
                                                   sizeof(FUNCTION_TABLE_ENTRY),
                                                   &createdEntry);

    if (createdEntry == FALSE) {

        // 
        // Someone beat us to, nothing to do 
        //  
        goto Exit;

    }


    if (FuncTracesInUse >= MAX_FUNC_TRACES) {

        // 
        // Destroy the table entry 
        //  
        RtlDeleteElementGenericTable(&FunctionTable,
                                     foundEntry);

        //
        // Only nag the user once.
        //
        if (ErrorReported == FALSE) {

            DbgPrint("***Out Of Static Entries For Functions. "\
                     "No Longer Logging***\n"); 

            ErrorReported = TRUE;

        }

        goto Exit;

    }

    // 
    // Now allocate a slot in the function traces array to store the data... 
    //  
    // Why do it this way? Why not just store the data in the table entry? 
    // Wellll we need to dump this data from the debugger, and I'll be damned if 
    // I write a debugger extension that walks a generic table and dumps out 
    // information about each entry in it. So, we use the array to store the 
    // data, then use the table as a quick lookup to find the entry in the array
    //  
    funcTrace = &FuncTraces[FuncTracesInUse];

    RtlZeroMemory(funcTrace,
                  sizeof(FUNC_TRACE));

    funcTrace->StartAddress = FunctionAddress;

    // 
    // Store the trace entry in the table entry
    //  
    foundEntry->TraceEntry = funcTrace;

    //
    // Update the number of in use entries.
    //
    FuncTracesInUse++;


    //  
    // Done! 
    //  

Exit:
    if (lockedShared) {
        ExReleaseSpinLockSharedFromDpcLevel(&FunctionTableLock);
    } else {
        ExReleaseSpinLockExclusiveFromDpcLevel(&FunctionTableLock);
    }
    
    KeLowerIrql(oldIrql);

    return foundEntry;

}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
FunctionTableCompareRoutine(
    PRTL_GENERIC_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct) 
{

    PFUNCTION_TABLE_ENTRY       firstEntry;
    PFUNCTION_TABLE_ENTRY       secondEntry;
    RTL_GENERIC_COMPARE_RESULTS compareResult;

    UNREFERENCED_PARAMETER(Table);

    firstEntry  = (PFUNCTION_TABLE_ENTRY)FirstStruct;
    secondEntry = (PFUNCTION_TABLE_ENTRY)SecondStruct;

    if (firstEntry->FunctionAddress == secondEntry->FunctionAddress) {

        compareResult = GenericEqual;

    } else if (firstEntry->FunctionAddress > secondEntry->FunctionAddress) {

        compareResult = GenericGreaterThan;

    } else {

        compareResult = GenericLessThan;

    }

    return compareResult;

}

_Use_decl_annotations_
PVOID
FunctionTableAllocateRoutine(
    PRTL_GENERIC_TABLE Table,
    CLONG ByteSize) 
{

    PVOID allocation;

    UNREFERENCED_PARAMETER(Table);

#pragma warning(suppress: 30030)
    allocation =  ExAllocatePoolWithTag(NonPagedPool, ByteSize, 'nFep');

    return allocation;
}


_Use_decl_annotations_
VOID
FunctionTableFreeRoutine(
    PRTL_GENERIC_TABLE Table,
    PVOID Buffer) 
{

    UNREFERENCED_PARAMETER(Table);

    ExFreePoolWithTag(Buffer,
                      'nFep');

    return;
}

