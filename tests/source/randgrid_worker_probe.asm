OPTION CASEMAP:NONE
EXTERN RandgridWorkerAdmission:PROC
EXTERN g_randgrid_worker:BYTE
EXTERN __chkstk:PROC
.code
WorkerProbePositive PROC
    mov eax, 10
    ret
WorkerProbePositive ENDP
WorkerProbeFailure PROC
    mov eax, 20
    ret
WorkerProbeFailure ENDP

; status, original Boolean, enabled, output buffer
WorkerProbeInvoke PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 8
    .allocstack 8
    mov rbx, r9
    mov r12d, ecx
    mov r13d, edx
    mov r14d, r8d
    mov eax, 0D040h
    call __chkstk
    sub rsp, rax
    .allocstack 0D040h
    .endprolog
    mov dword ptr [rsp+0CEF4h], r12d
    mov byte ptr [rsp+9DEh], r13b
    mov dword ptr [g_randgrid_worker+8], r14d
    sub rsp, 8
    call RandgridWorkerAdmission
    add rsp, 8
    movzx edx, byte ptr [rsp+9DEh]
    mov [rbx], edx
    mov edx, dword ptr [rsp+0CEF4h]
    mov [rbx+4], edx
    mov [rbx+8], r12
    mov [rbx+16], r13
    add rsp, 0D040h
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
WorkerProbeInvoke ENDP
END
