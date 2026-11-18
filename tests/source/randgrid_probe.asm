OPTION CASEMAP:NONE
EXTERN RandgridOfflineAdmission:PROC
EXTERN g_randgrid_offline:BYTE
.code
RandgridProbePositive PROC
    mov eax, 10
    ret
RandgridProbePositive ENDP
RandgridProbeFailure PROC
    mov eax, 20
    ret
RandgridProbeFailure ENDP
RandgridProbeBookkeeping PROC
    mov eax, 30
    ret
RandgridProbeBookkeeping ENDP

; status, enabled, expected-zero-register, result buffer
RandgridProbeInvoke PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 32
    .allocstack 32
    .endprolog
    mov rbx, r9
    mov r13, r8
    mov r8d, ecx
    mov dword ptr [g_randgrid_offline+8], edx
    mov r12, 1122334455667788h
    ; Native mid-function JMP entry is 16-byte aligned, unlike a CALL entry.
    sub rsp, 8
    call RandgridOfflineAdmission
    add rsp, 8
    mov [rbx], r12
    mov [rbx+8], r13
    mov [rbx+16], r8
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
RandgridProbeInvoke ENDP
END
