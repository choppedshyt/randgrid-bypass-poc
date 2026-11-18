OPTION CASEMAP:NONE
EXTERN g_randgrid_offline:BYTE
EXTERN RandgridRestoreNativePredicate:PROC
.code

; Replaces only the audited, post-cleanup signed-status branch. No kernel API
; result, handle, stack pointer, exception disposition, or termination API is
; fabricated. The native successful-open and other-error paths remain intact.
RandgridOfflineAdmission PROC
    cmp dword ptr [g_randgrid_offline+8], 1
    jne original_status
    cmp r8d, 0C0000034h
    jne original_status
    test r13, r13
    jne original_status
    ; This JMP target inherits the native body's 16-byte-aligned RSP, not
    ; the usual function-entry RSP+8. Preserve every volatile input around
    ; the one-time restoration helper and retain the allocated relay.
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, 220h
    vmovdqu ymmword ptr [rsp+20h], ymm0
    vmovdqu ymmword ptr [rsp+40h], ymm1
    vmovdqu ymmword ptr [rsp+60h], ymm2
    vmovdqu ymmword ptr [rsp+80h], ymm3
    vmovdqu ymmword ptr [rsp+0A0h], ymm4
    vmovdqu ymmword ptr [rsp+0C0h], ymm5
    vmovdqu ymmword ptr [rsp+0E0h], ymm6
    vmovdqu ymmword ptr [rsp+100h], ymm7
    vmovdqu ymmword ptr [rsp+120h], ymm8
    vmovdqu ymmword ptr [rsp+140h], ymm9
    vmovdqu ymmword ptr [rsp+160h], ymm10
    vmovdqu ymmword ptr [rsp+180h], ymm11
    vmovdqu ymmword ptr [rsp+1A0h], ymm12
    vmovdqu ymmword ptr [rsp+1C0h], ymm13
    vmovdqu ymmword ptr [rsp+1E0h], ymm14
    vmovdqu ymmword ptr [rsp+200h], ymm15
    call RandgridRestoreNativePredicate
    movzx eax, al
    mov [rsp], eax
    vmovdqu ymm0, ymmword ptr [rsp+20h]
    vmovdqu ymm1, ymmword ptr [rsp+40h]
    vmovdqu ymm2, ymmword ptr [rsp+60h]
    vmovdqu ymm3, ymmword ptr [rsp+80h]
    vmovdqu ymm4, ymmword ptr [rsp+0A0h]
    vmovdqu ymm5, ymmword ptr [rsp+0C0h]
    vmovdqu ymm6, ymmword ptr [rsp+0E0h]
    vmovdqu ymm7, ymmword ptr [rsp+100h]
    vmovdqu ymm8, ymmword ptr [rsp+120h]
    vmovdqu ymm9, ymmword ptr [rsp+140h]
    vmovdqu ymm10, ymmword ptr [rsp+160h]
    vmovdqu ymm11, ymmword ptr [rsp+180h]
    vmovdqu ymm12, ymmword ptr [rsp+1A0h]
    vmovdqu ymm13, ymmword ptr [rsp+1C0h]
    vmovdqu ymm14, ymmword ptr [rsp+1E0h]
    vmovdqu ymm15, ymmword ptr [rsp+200h]
    cmp dword ptr [rsp], 0
    je restoration_failed
    add rsp, 220h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    lock inc qword ptr [g_randgrid_offline+16]
    mov dword ptr [g_randgrid_offline+56], r8d
    mov r12d, 1
    jmp qword ptr [g_randgrid_offline+48]
restoration_failed:
    add rsp, 220h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
original_status:
    test r8d, r8d
    js original_failure
    jmp qword ptr [g_randgrid_offline+32]
original_failure:
    jmp qword ptr [g_randgrid_offline+40]
RandgridOfflineAdmission ENDP
END
