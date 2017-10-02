; 
;  Copyright 2008-2017 OSR Open Systems Resources, Inc.
;  All rights reserved.
; 
;  Redistribution and use in source and binary forms, with or without
;  modification, are permitted provided that the following conditions are met:
; 
;  1. Redistributions of source code must retain the above copyright notice,
;     this list of conditions and the following disclaimer.
;  
;  2. Redistributions in binary form must reproduce the above copyright notice,
;     this list of conditions and the following disclaimer in the documentation
;     and/or other materials provided with the distribution.
;  
;  3. Neither the name of the copyright holder nor the names of its
;     contributors may be used to endorse or promote products derived from this
;     software without specific prior written permission.
; 
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
; ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
; LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
; CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
; SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
; INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
; CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
; ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
; POSSIBILITY OF SUCH DAMAGE
; 
;
; This module provides the PENTER hooks for x64. I tried to make it
; as readable as possible, but it turned out to be way more complicated
; than expected. So, change at your own peril...
;

;
; External C functions that do the real work
;
EXTERN LogFuncEntry:PROC
EXTERN LogFuncExit:PROC

; typedef struct _ENTER_REGISTERS {
;     ULONGLONG R11;
;     ULONGLONG R10;
;     ULONGLONG R9;
;     ULONGLONG R8;
;     ULONGLONG Rdx;
;     ULONGLONG Rcx;
;     ULONGLONG Rax;
;     ULONGLONG Rsp;
;     ULONGLONG ReturnRip;    
; }ENTER_REGISTERS, *PENTER_REGISTERS;

;
; We save our registers after the home/alignment space that 
; we place on the stack for the logging functions.
;
; !OFFSETS MUST MATCH START OF ENTER/EXIT_REGISTERS STRUCTURE!
;
R11Save EQU 20h     ; Offset 0x00
R10Save EQU 28h     ; Offset 0x08
R9Save  EQU 30h     ; Offset 0x10
R8Save  EQU 38h     ; Offset 0x18
RDXSave EQU 40h     ; Offset 0x20
RCXSave EQU 48h     ; Offset 0x28
RAXSave EQU 50h     ; Offset 0x30
RSPSave EQU 58h     ; Offset 0x38
RIPSave EQU 60h     ; Offset 0x40

;
; VOID
; SAVE_VOLATILE(
;   VOID
; );
;
;   Saves all volatile registers on the stack
;
SAVE_VOLATILE macro

    ;
    ; Store all of the VOLATILE registers on the stack
    ;

    ;
    ; Note that we don't need .SAVEREG declarations for these,
    ; we're saving VOLATILE registers (.SAVEREG exists to unwind
    ; non-volatile registers)
    ;    
    mov R11Save[rsp], r11
    mov R10Save[rsp], r10
    mov R9Save[rsp],  r9
    mov R8Save[rsp],  r8
    mov RDXSave[rsp], rdx
    mov RCXSave[rsp], rcx
    mov RAXSave[rsp], rax

endm


;
; VOID
; RESTORE_VOLATILE(
;   VOID
; );
;
;   Restore all volatile registers from the stack
;
RESTORE_VOLATILE macro

    mov r11, R11Save[rsp]
    mov r10, R10Save[rsp]
    mov r9,  R9Save[rsp]
    mov r8,  R8Save[rsp]
    mov rdx, RDXSave[rsp]
    mov rcx, RCXSave[rsp]
    mov rax, RAXSave[rsp]

endm


;
; The prolog of every non-leaf function aligns itself on a 16 byte boundary. 
;
; The prolog of a leaf function does not need to align itself on a 16 byte 
; boundary
;
; Accesses to non 16 byte aligned stack pointers after a function's prolog 
; result in an access violation
;
; So, it goes like this:
;
;   Stack starts out as a 16 byte aligned value
;
;   Non-Leaf function is called
;       -> On entry, stack is NOT aligned! "CALL FOO" pushed the retun value on
;          the stack, thus unaligning the stack
;
;       -> Because the non-leaf function will access the stack, it subtracts 
;          8 to align it
;
;       -> If the non-leaf function calls another function, it subtraces 20h for
;          the home space
;
;   Leaf function is called
;       -> Leaf functions don't access the stack, so the stack stays unaligned 
;          (caller pushed the return address on the stack)
; 
; Why is that so important? Well, our _penter hook has to make sure that the stack 
; is 16 byte aligned before we go using it. When you compile with penter hooks, 
; leaf functions become non-leaf functions. You would think that the compiler would 
; "know" this and make every function have a non-leaf prolog, but it doesn't. Therefore,
; either our stack is unaligned on entry (non-leaf caller aligned its stack, called us
; and unaligned it) or it's aligned (leaf caller had an unaligned stack, called our 
; _penter which aligned it))
;
; The trick to this is that we also want to preserve stack walking and 
; we can't dynamically generate stack walk data. In other words, we can't say, "hey,
; for this invocation of _penter the stack walk code needs to subtract another 8 bytes".
; .xdata/.pdata is static, so we can only describe how to unwind a particular function
; at compile time
; 
; This leads us to having two stubs: one that does the work if called from a 
; non-leaf and another for one called from leaf functions. We always register
; the non-leaf one (_penter) and call _penter_leaf if necessary
;
_text SEGMENT

EntryLocalsSize EQU 78h   ; -20h for the Home Space of the logging function
                          ; -50h for the ENTER_REGISTERS structure (rounded
                          ;      up to 16 byte alignment
                          ; -08h for the return address placed by the caller
                          ;
                          ; For a non-leaf caller, this should ALIGN the
                          ; call stack (the stack was aligned, then the call
                          ; to this function unaligned it)

LeafLocalsSize EQU  70h   ; -20h for the Home Space of the logging function
                          ; -50h for the ENTER_REGISTERS structure (rounded
                          ;      up to 16 byte alignment
                          ;
                          ; NO -08h to align, the stack is aligned on entry

;
; VOID
; LEAF_ENTER(
;   VOID
; );
;
;   Called by _penter_leaf/_pexit_leaf for common processing
;
LEAF_ENTER macro

    ; We don't try to be tricky and use the local space set up by _penter,
    ; remember that we're trying to preserve stack walk ability 

    sub rsp, LeafLocalsSize

    .ALLOCSTACK LeafLocalsSize  ; Generate unwind data
   
    .ENDPROLOG                  ; Done with the prolog

    SAVE_VOLATILE           ; Store all of the VOLATILE registers on the stack

    ;
    ; We want the caller's RSP and RIP. This stuff is above our local space
    ; AND above the _penter home space
    ;

    lea rcx, [rsp+LeafLocalsSize] ; Start by getting rid of the leaf locals

    add rcx, EntryLocalsSize      ; Then account for _penter locals

    add rcx, 08h            ; And another 8 to account for the return address

    mov RSPSave[rsp], rcx   ; Store the result as the caller's RSP

    mov rcx, [rcx]          ; Deref to get the caller's RIP

    mov RIPSave[rsp], rcx   ; Store the result as the caller's RIP

    lea rcx, R11Save[rsp]   ; Set up the parameter to the logging function

ENDM

;
; VOID
; LEAF_EXIT(
;   VOID
; );
;
;   Called by _penter_leaf/_pexit_leaf for common processing
;
LEAF_EXIT macro

    RESTORE_VOLATILE        ; Restore all VOLATILE registers

    add rsp, LeafLocalsSize ; Clean up our stack space

ENDM

;
; VOID
; _penter_leaf(
;   VOID
; );
;
;   Called by the _penter hook if it was called by a leaf function.
;
;   1. Leaf Function was called on an aligned stack
;   2. Leaf Function called _penter, which unaligned the stack
;   3. _penter called _penter_leaf, which aligned the stack
;
;   ** STACK IS ALIGNED ON ENTRY **
;
ALIGN   16              ; Align on a 16 byte boundary
PUBLIC _penter_leaf
_penter_leaf PROC FRAME

    LEAF_ENTER         ; Macro all the prolog stuff, it's the same
                       ; for entry/exit

    call LogFuncEntry  ; Log the function entry

    LEAF_EXIT          ; Macro all the epilog stuff, it's the same
                       ; for entry/exit

    ret                ; Return
_penter_leaf ENDP

;
; VOID
; _pexit_leaf(
;   VOID
; );
;
;   Called by the _pexit hook if it was called by a leaf function.
;
;   1. Leaf Function was called on an aligned stack
;   2. Leaf Function called _pexit, which unaligned the stack
;   3. _pexit called _pexit_leaf, which aligned the stack
;
;   ** STACK IS ALIGNED ON ENTRY **
;
ALIGN   16              ; Align on a 16 byte boundary
PUBLIC _pexit_leaf
_pexit_leaf PROC FRAME

    LEAF_ENTER         ; Macro all the prolog stuff, it's the same
                       ; for entry/exit

    call LogFuncExit   ; Log the function entry

    LEAF_EXIT          ; Macro all the epilog stuff, it's the same
                       ; for entry/exit
    
    ret                ; Return

_pexit_leaf ENDP

;
; VOID
; HOOK_ENTER(
;   LeafProcessingFunction
; );
;
;   Called by _penter/_pexit for common processing
;
HOOK_ENTER macro LeafFunction

    sub rsp, EntryLocalsSize 

    .ALLOCSTACK EntryLocalsSize ; Generate unwind data
    
    .ENDPROLOG                  ; Done with the prolog
    
    test rsp, 8                 ; Stack aligned on an 8 byte boundary? This would mean
                                ; a leaf function called us

    je Stack16AlignedProlog     ; Not 8 byte aligned, jump!
        
    ; We can't conditionally generate .pdata, so we call a helper function
    ; to result in an aligned stack. This is just to make stack walking
    ; work properly
            
    call LeafFunction           ; Leaf caller. Call our alternate entry point

    add rsp, EntryLocalsSize    ; Clean up our stack space

    ret

Stack16AlignedProlog:

    SAVE_VOLATILE               ; Store all of the VOLATILE registers on the stack

    ;
    ; Lastly we want the caller's RSP and RIP. How far this stuff
    ; is away depends on if the stack was aligned or not
    ;

    lea rcx, [rsp+EntryLocalsSize] ; Undo our prolog

    mov RSPSave[rsp], rcx          ; Store the result as the caller's RSP

    mov rcx, [rcx]                 ; Deref to get the caller's RIP

    mov RIPSave[rsp], rcx          ; Store the result as the caller's RIP

    lea rcx, [rsp+20h]             ; Set up the parameter to the logging function

ENDM

;
; VOID
; HOOK_EXIT(
;   VOID
; );
;
;   Called by _penter/_pexit for common processing
;
HOOK_EXIT macro

    RESTORE_VOLATILE               ; Restore all VOLATILE registers

    add rsp, EntryLocalsSize       ; Clean up our stack space
    
    ret                            ; Return

ENDM

;
; VOID
; _penter(
;   VOID
; );
;
;   Our function entry sub. Sets up the call stack and calls 
;   LogFuncEntry
;
ALIGN   16              ; Align on a 16 byte boundary
PUBLIC _penter
_penter PROC FRAME

    HOOK_ENTER _penter_leaf ; Common prolog code
    
    call LogFuncEntry       ; Log the function entry

    HOOK_EXIT 

    ret                     ; Return
_penter ENDP

;
; VOID
; _pexit(
;   VOID
; );
;
;   Our function exit sub. Sets up the call stack and calls 
;   LogFuncExit
;
ALIGN   16              ; Align on a 16 byte boundary
PUBLIC _pexit
_pexit PROC FRAME

    HOOK_ENTER _pexit_leaf  ; Common prolog code
    
    call LogFuncExit        ; Log the function exit

    HOOK_EXIT 

    ret                     ; Return

_pexit ENDP


_text ENDS

END
