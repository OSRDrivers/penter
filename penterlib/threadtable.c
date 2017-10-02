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
ThreadTableInitialize(
    VOID) 
{
    RtlInitializeGenericTable(&ThreadTable,
                              ThreadTableCompareRoutine,
                              ThreadTableAllocateRoutine,
                              ThreadTableFreeRoutine,
                              NULL);
    return;
}

PTHREAD_TABLE_ENTRY
ThreadTableLookupEntry(
    HANDLE ThreadId,
    LOOKUP_ACTION LookupAction) 
{

    KIRQL               oldIrql;
    THREAD_TABLE_ENTRY  lookupEntry;
    PTHREAD_TABLE_ENTRY foundEntry = NULL;
    BOOLEAN             createdEntry;
    BOOLEAN             lockedShared;

    lookupEntry.ThreadId = (ULONGLONG)ThreadId; 

    KeRaiseIrql(SynchronizeIrql, &oldIrql);
    ExAcquireSpinLockSharedAtDpcLevel(&ThreadTableLock);
    lockedShared = TRUE;

    foundEntry = 
        (PTHREAD_TABLE_ENTRY)RtlLookupElementGenericTable(&ThreadTable,
                                                          &lookupEntry);

    if (foundEntry != NULL) {

        ThreadTableEntryReference(foundEntry);

        goto Exit;

    }

    if (LookupAction == LookupActionFailIfNotFound) {

        goto Exit;

    }
    
    RtlZeroMemory(&lookupEntry,
                  sizeof(THREAD_TABLE_ENTRY));

    //
    // Drop shared and reacquire
    //
    ExReleaseSpinLockSharedFromDpcLevel(&ThreadTableLock);
    lockedShared = FALSE;

    ExAcquireSpinLockExclusiveAtDpcLevel(&ThreadTableLock);

    // 
    // Thread will only have ONE reference count when we return. We don't want 
    // to be in the business of tracking the lifetime of a thread, so we'll only 
    // keep an entry in the thread table for as long as we have outstanding 
    // calls from the thread 
    //  
    // The alternative is to register a thread notify callback and tear it down 
    // there. However, a goal of this project is to only require BUILD changes 
    // and not any CODE changes to the driver (and we won't know to 
    // unregister the thread callback unless they tell us) 
    //  
    lookupEntry.RefCount = 1;
    lookupEntry.ThreadId = (ULONGLONG)ThreadId; 

    foundEntry = 
        (PTHREAD_TABLE_ENTRY)RtlInsertElementGenericTable(
                                                   &ThreadTable,
                                                   &lookupEntry,
                                                   sizeof(THREAD_TABLE_ENTRY),
                                                   &createdEntry);

    if (createdEntry == FALSE) {

        // 
        // Someone came in and created the entry before we could (we drop the 
        // lock shared and pick it up Ex above, so it could happen...). 
        //  
        // Make sure we reference this entry before returning it. 
        //  
        ThreadTableEntryReference(foundEntry);

    }

    //  
    // Done! 
    //  

Exit:

    if (lockedShared) {
        ExReleaseSpinLockSharedFromDpcLevel(&ThreadTableLock);
    } else {
        ExReleaseSpinLockExclusiveFromDpcLevel(&ThreadTableLock);
    }

    KeLowerIrql(oldIrql);

    return foundEntry;

}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
ThreadTableCompareRoutine(
    PRTL_GENERIC_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct) 
{

    PTHREAD_TABLE_ENTRY         firstEntry;
    PTHREAD_TABLE_ENTRY         secondEntry;
    RTL_GENERIC_COMPARE_RESULTS compareResult;

    UNREFERENCED_PARAMETER(Table);

    firstEntry  = (PTHREAD_TABLE_ENTRY)FirstStruct;
    secondEntry = (PTHREAD_TABLE_ENTRY)SecondStruct;

    if (firstEntry->ThreadId == secondEntry->ThreadId) {

        compareResult = GenericEqual;

    } else if (firstEntry->ThreadId > secondEntry->ThreadId) {

        compareResult = GenericGreaterThan;

    } else {

        compareResult = GenericLessThan;

    }

    return compareResult;

}

_Use_decl_annotations_
PVOID
ThreadTableAllocateRoutine(
    PRTL_GENERIC_TABLE Table,
    CLONG ByteSize) 
{

    PVOID allocation;

    UNREFERENCED_PARAMETER(Table);

#pragma warning(suppress: 30030)
    allocation = ExAllocatePoolWithTag(NonPagedPool, ByteSize, 'hTep');

    return allocation;
}


_Use_decl_annotations_
VOID
ThreadTableFreeRoutine(
    PRTL_GENERIC_TABLE Table,
    PVOID Buffer) 
{

    UNREFERENCED_PARAMETER(Table);

    ExFreePoolWithTag(Buffer,
                      'hTep');

    return;
}

LONG
ThreadTableEntryReference(
    PTHREAD_TABLE_ENTRY Entry) 
{

    LONG refCount;

    refCount = InterlockedIncrement(&Entry->RefCount);

    ASSERT(refCount > 0);

    return refCount;

}

LONG
ThreadTableEntryDereference(
    PTHREAD_TABLE_ENTRY Entry) 
{

    LONG refCount;
    BOOLEAN deleted;
    KIRQL oldIrql;

    KeRaiseIrql(SynchronizeIrql, &oldIrql);
    ExAcquireSpinLockExclusiveAtDpcLevel(&ThreadTableLock);

    refCount = InterlockedDecrement(&Entry->RefCount);

    ASSERT(refCount >= 0);

    if (refCount != 0) {

        goto Exit;

    }

    deleted = RtlDeleteElementGenericTable(&ThreadTable,
                                           Entry);

    ASSERT(deleted == TRUE);

Exit:

    ExReleaseSpinLockExclusiveFromDpcLevel(&ThreadTableLock);
    KeLowerIrql(oldIrql);

    return refCount;

}
