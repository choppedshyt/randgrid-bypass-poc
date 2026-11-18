OPTION CASEMAP:NONE
EXTERN g_randgrid_worker:BYTE
.code
; The native worker rechecks this dependency. Keep only this application-level
; missing-device policy active. No API calls, allocation, stack growth, SIMD
; changes, handle fabrication, or kernel-status substitutions occur here.
RandgridWorkerAdmission PROC
    cmp dword ptr [g_randgrid_worker+8], 1
    jne original_status
    cmp dword ptr [rsp+0CF04h], 0C0000034h
    jne original_status
    cmp byte ptr [rsp+9EEh], 0
    jne original_status
    lock inc qword ptr [g_randgrid_worker+16]
    mov dword ptr [g_randgrid_worker+56], 0C0000034h
    mov byte ptr [rsp+9EEh], 1
original_status:
    movzx eax, byte ptr [rsp+9EEh]
    test eax, eax
    jz original_failure
    jmp qword ptr [g_randgrid_worker+32]
original_failure:
    jmp qword ptr [g_randgrid_worker+40]
RandgridWorkerAdmission ENDP
END
