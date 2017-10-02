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
#ifndef __FUNC_TRACE_H__
#define __FUNC_TRACE_H__

#define PENTER_STACK_WALK_ON 1

//
// Cap the max number of functions that we'll monitor to
// something insane (like 2,000). This makes everything
// easier...
//
#define MAX_FUNC_TRACES 2000

#define MAX_CALL_FRAMES    5
#define MAX_CALL_HISTORY   50

typedef struct _CALL_HISTORY {
    PVOID  Frames[MAX_CALL_HISTORY];
    USHORT FramesCount;
    LONG   StackSeenCount;
    volatile LONG  Epoch;
}CALL_HISTORY, *PCALL_HISTORY;

// 
// Entries are invalidated by updating the global epoch
// 
extern volatile LONG CurrentEpoch;

//
// Tracking structure for each function.
//
typedef struct _FUNC_TRACE {
    //
    // Starting address of the function.
    //
    ULONGLONG      StartAddress;

    //
    // Number of clock ticks spent in the function
    //
    LARGE_INTEGER  CallTicks;

    //
    // Number of times the function has been called.
    //
    ULONG          CallCount;

#ifdef PENTER_STACK_WALK_ON
    CALL_HISTORY   CallHistory[MAX_CALL_HISTORY];
    volatile LONG  CallHistoryIndex;
    volatile LONG  CallHistoryTotal;
#endif

}FUNC_TRACE, *PFUNC_TRACE;

#endif __FUNC_TRACE_H__


