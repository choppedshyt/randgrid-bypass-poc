OPTION CASEMAP:NONE
EXTERN g_randgrid_match:BYTE
.code
; This is the match-start Randgrid open's decision AFTER its native thunk has
; been released and its temporary object name erased. A missing device has
; acquired none of the subsequent IOCTL/decoder/close resources.
;
; Only the exact missing-device case with the native zero seed and no handle
; enters the already existing accepted bookkeeping. That continuation needs
; R13D=1 for its lock; R14 still has its temporary string-length role here.
; No API calls, allocation, stack growth, or SIMD changes occur in this hook.
RandgridMatchAdmission PROC
    cmp dword ptr [g_randgrid_match+8], 1
    jne original_status
    cmp edx, 0C0000034h
    jne original_status
    test r12, r12
    jne original_status
    cmp qword ptr [rbp+438h], 0
    jne original_status
    lock inc qword ptr [g_randgrid_match+16]
    mov dword ptr [g_randgrid_match+56], 0C0000034h
    mov r13d, 1
    test edx, edx
    jmp qword ptr [g_randgrid_match+48]
original_status:
    ; Equivalent to the verified TEST and preserving conditional-RET bridge.
    ; Preserve every GPR, the original frame, DF, and the native TEST flags.
    test edx, edx
    js original_failure
    jmp qword ptr [g_randgrid_match+32]
original_failure:
    jmp qword ptr [g_randgrid_match+40]
RandgridMatchAdmission ENDP
END
