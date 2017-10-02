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

#ifndef __PENTER_H__
#define __PENTER_H__

#define RTL_USE_AVL_TABLES 0

#include <ntddk.h>

//
// 64bit headers and compiler insist on inlining the IRQL functions with penter
// tracing enabled. This causes an infinite loop for us, so we provide our
// own hacky versions
//
#ifdef AMD64

#undef KeGetCurrentIrql
#define KeGetCurrentIrql (KIRQL)ReadCR8

_IRQL_requires_max_(HIGH_LEVEL)
CFORCEINLINE
VOID
Osr64LowerIrql (
    _In_ _Notliteral_ _IRQL_restores_ KIRQL NewIrql
   )
{
    WriteCR8(NewIrql);
    return;
}

_IRQL_requires_max_(HIGH_LEVEL)
_IRQL_raises_(NewIrql)
_IRQL_saves_
CFORCEINLINE
KIRQL
Osr64RaiseIrql (
    _In_ KIRQL NewIrql
    )
{

    KIRQL OldIrql;

    OldIrql = KeGetCurrentIrql();

    WriteCR8(NewIrql);
    return OldIrql;
}

#undef KfRaiseIrql
#define KfRaiseIrql Osr64RaiseIrql

#undef KeLowerIrql
#define KeLowerIrql Osr64LowerIrql

#endif

#include "func_trace.h"

extern
NTSYSAPI
USHORT
NTAPI
RtlCaptureStackBackTrace(
    __in ULONG FramesToSkip,
    __in ULONG FramesToCapture,
    __out_ecount(FramesToCapture) PVOID *BackTrace,
    __out_opt PULONG BackTraceHash
);


// 
// Loop to avoid:
// 4127 -- Conditional Expression is Constant warning
// 
#define WHILE_CONSTANT(constant) \
__pragma(warning(disable: 4127)) while(constant); __pragma(warning(default: 4127))

#ifdef _X86_

typedef struct _ENTER_REGISTERS {
    ULONG Edi;
    ULONG Esi;
    ULONG Ebp;
    ULONG Esp;
    ULONG Ebx;
    ULONG Edx;
    ULONG Ecx;
    ULONG Eax;
    ULONG EbpFrame;
    ULONG ReturnEip;
    ULONG CalleeEip;
}ENTER_REGISTERS, *PENTER_REGISTERS;

typedef struct _EXIT_REGISTERS {
    ULONG Edi;
    ULONG Esi;
    ULONG Ebp;
    ULONG Esp;
    ULONG Ebx;
    ULONG Edx;
    ULONG Ecx;
    ULONG Eax;
    ULONG EbpFrame;
    ULONG ReturnEip;
    ULONG CalleeReturnEip;
}EXIT_REGISTERS, *PEXIT_REGISTERS;

#else

typedef struct _ENTER_REGISTERS {
    ULONGLONG R11;
    ULONGLONG R10;
    ULONGLONG R9;
    ULONGLONG R8;
    ULONGLONG Rdx;
    ULONGLONG Rcx;
    ULONGLONG Rax;
    ULONGLONG Rsp;
    
    ULONGLONG ReturnRip;

}ENTER_REGISTERS, *PENTER_REGISTERS;

//
// Assembly code has baked in assumption
//
C_ASSERT(sizeof(ENTER_REGISTERS) <= 0x60);

typedef struct _EXIT_REGISTERS {
    ULONGLONG R11;
    ULONGLONG R10;
    ULONGLONG R9;
    ULONGLONG R8;
    ULONGLONG Rdx;
    ULONGLONG Rcx;
    ULONGLONG Rax;
    ULONGLONG Rsp;
    ULONGLONG ReturnRip;    
}EXIT_REGISTERS, *PEXIT_REGISTERS;

//
// Assembly code has baked in assumption
//
C_ASSERT(sizeof(EXIT_REGISTERS) <= 0x60);

#endif

typedef struct _FUNCTION_TABLE_ENTRY {

    ULONGLONG     FunctionAddress;

    PFUNC_TRACE   TraceEntry;

}FUNCTION_TABLE_ENTRY, *PFUNCTION_TABLE_ENTRY;

typedef struct _THREAD_TABLE_ENTRY {

    ULONGLONG         ThreadId;

    volatile LONG     RefCount;

    SINGLE_LIST_ENTRY CallList;

}THREAD_TABLE_ENTRY, *PTHREAD_TABLE_ENTRY;

typedef struct _TIME_LOGGER {

    SINGLE_LIST_ENTRY     ListEntry;
    PFUNCTION_TABLE_ENTRY FunctionEntry;
    PTHREAD_TABLE_ENTRY   ThreadEntry;
    LARGE_INTEGER         StartTicks;

}TIME_LOGGER, *PTIME_LOGGER;

#define TIME_LOGGER_TAG 'LTsO'

extern EX_SPIN_LOCK      FunctionTableLock;
extern RTL_GENERIC_TABLE FunctionTable;

extern EX_SPIN_LOCK      ThreadTableLock;
extern RTL_GENERIC_TABLE ThreadTable;

extern KIRQL             SynchronizeIrql;

extern FUNC_TRACE FuncTraces[MAX_FUNC_TRACES];
extern ULONG_PTR  FuncTracesInUse;
extern BOOLEAN    ErrorReported;


typedef enum _LOOKUP_ACTION {
    LookupActionFailIfNotFound,
    LookupActionCreateIfNotFound,
    LookupActionMaximum
}LOOKUP_ACTION, *PLOOKUP_ACTION;


RTL_GENERIC_COMPARE_ROUTINE  FunctionTableCompareRoutine;
RTL_GENERIC_ALLOCATE_ROUTINE FunctionTableAllocateRoutine;
RTL_GENERIC_FREE_ROUTINE     FunctionTableFreeRoutine;

RTL_GENERIC_COMPARE_ROUTINE  ThreadTableCompareRoutine;
RTL_GENERIC_ALLOCATE_ROUTINE ThreadTableAllocateRoutine;
RTL_GENERIC_FREE_ROUTINE     ThreadTableFreeRoutine;

VOID LogFuncEntry(PENTER_REGISTERS Registers);
VOID LogFuncExit(PEXIT_REGISTERS Registers);

VOID
FunctionTableInitialize(
    VOID
    );


PFUNCTION_TABLE_ENTRY
FunctionTableLookupEntry(
    ULONGLONG FunctionAddress
    );

VOID
ThreadTableInitialize(
    VOID
    );

LONG
ThreadTableEntryReference(
    PTHREAD_TABLE_ENTRY Entry
    );

LONG
ThreadTableEntryDereference(
    PTHREAD_TABLE_ENTRY Entry
    );


PTHREAD_TABLE_ENTRY
ThreadTableLookupEntry(
    HANDLE ThreadId,
    LOOKUP_ACTION LookupAction
    );

#endif __PENTER_H__


