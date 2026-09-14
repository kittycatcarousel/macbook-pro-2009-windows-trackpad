; SPDX-License-Identifier: MIT
; The entry points use the stack frame of the Apple driver.
; tools/build-patch.c links all addresses and makes the PE base relocation records.
.386
.model flat
extern wdf_globals:dword, wdf_input:dword, wdf_output:dword
extern old_dispatch:near, ioctl_done:near, tap_continue:near
extern gap_continue:near, motion_continue:near, queue_packet:near, init_continue:near
extern reset_continue:near
extern init_failed:near
public runtime_dispatch, runtime_tap, runtime_gap, runtime_motion, runtime_init, runtime_reset
.code
runtime_init:
    mov dword ptr [esi+18b4h],500
    mov dword ptr [esi+18d0h],3 ; Interface version.
    mov dword ptr [esi+18d4h],250
    mov dword ptr [esi+18d8h],300
    mov dword ptr [esi+18dch],8
    mov dword ptr [esi+18f0h],50
    pushad
    push dword ptr [ebp-4]
    call click_create
    mov [esp+28],eax
    popad
    test eax,eax
    js init_error
    jmp init_continue
init_error:
    add esp,12 ; Remove the original semaphore arguments.
    jmp init_failed
runtime_reset:
    and dword ptr [esi+18b8h],0
    and dword ptr [esi+18e0h],0
    and dword ptr [esi+18e4h],0
    and dword ptr [esi+18e8h],0
    and dword ptr [esi+18ech],0
    jmp reset_continue
runtime_tap:
    mov edx,[esi+18d4h]
    jmp tap_continue
runtime_gap:
    pushad
    call click_cancel
    popad
    push edx
    mov edx,[esi+18d8h]
    cmp eax,edx
    pop edx
    jmp gap_continue
runtime_motion:
    lea edi,[esi+18e0h]
    movsx eax,byte ptr [ebp-0ch]
    cdq
    add [edi],eax
    adc [edi+4],edx
    movsx eax,byte ptr [ebp-0ah]
    cdq
    add [edi+8],eax
    adc [edi+12],edx
    mov ecx,[esi+18dch]
    mov eax,[edi]
    mov edx,[edi+4]
    test edx,edx
    jns x_absolute
    neg eax
    adc edx,0
    neg edx
x_absolute:
    test edx,edx
    jnz begin_drag
    cmp eax,ecx
    jae begin_drag
    mov eax,[edi+8]
    mov edx,[edi+12]
    test edx,edx
    jns y_absolute
    neg eax
    adc edx,0
    neg edx
y_absolute:
    test edx,edx
    jnz begin_drag
    cmp eax,ecx
    jae begin_drag
    mov byte ptr [ebp-3],0
    jmp motion_continue
begin_drag:
    mov byte ptr [ebp-3],0
    lea eax,[ebp-0ch]
    push eax
    lea eax,[esi+14b4h]
    push eax
    call queue_packet
    and dword ptr [ebp-0ch],0
    mov byte ptr [ebp-3],1
    mov byte ptr [esi+18a6h],0
    jmp motion_continue
runtime_dispatch:
    mov eax,[ebp+18h]
    cmp eax,0f2018h
    je get_config
    cmp eax,0f201ch
    je set_config
    cmp eax,0f2010h
    je get_config
    cmp eax,0f2014h
    je set_config
    cmp eax,0f2008h
    je old_config
    cmp eax,0f200ch
    je old_config
    sub eax,0f2000h
    jmp old_dispatch
get_config:
    mov edi,16
    cmp dword ptr [ebp+18h],0f2018h
    jne get_size
    mov edi,20
get_size:
    cmp [ebp+10h],edi
    jb invalid_config
    push 0
    lea eax,[ebp-4]
    push eax
    push edi
    push dword ptr [ebp+0ch]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_output]
    mov edi,eax
    test eax,eax
    js finish_config
    mov ecx,[ebp-4]
    mov dword ptr [ecx],2
    mov eax,[esi+18d4h]
    mov [ecx+4],eax
    mov eax,[esi+18d8h]
    mov [ecx+8],eax
    mov eax,[esi+18dch]
    mov [ecx+12],eax
    mov dword ptr [ebp-8],16
    cmp dword ptr [ebp+18h],0f2018h
    jne finish_config
    mov dword ptr [ecx],3
    mov eax,[esi+18f0h]
    mov [ecx+16],eax
    mov dword ptr [ebp-8],20
    jmp finish_config
set_config:
    mov edi,16
    cmp dword ptr [ebp+18h],0f201ch
    jne set_size
    mov edi,20
set_size:
    cmp [ebp+14h],edi
    jne invalid_config
    push 0
    lea eax,[ebp-4]
    push eax
    push edi
    push dword ptr [ebp+0ch]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_input]
    mov edi,eax
    test eax,eax
    js finish_config
    mov ecx,[ebp-4]
    mov eax,2
    cmp dword ptr [ebp+18h],0f201ch
    jne set_version
    inc eax
set_version:
    cmp [ecx],eax
    jne invalid_config
    ; Each setting uses an aligned DWORD and accepts the full unsigned 32-bit range.
    mov eax,[ecx+4]
    mov [esi+18d4h],eax
    mov eax,[ecx+8]
    mov [esi+18d8h],eax
    mov eax,[ecx+12]
    mov [esi+18dch],eax
    cmp dword ptr [ebp+18h],0f201ch
    jne set_done
    mov eax,[ecx+16]
    mov [esi+18f0h],eax
    pushad
    call click_cancel
    popad
set_done:
    xor edi,edi
    jmp finish_config
old_config:
    mov edi,0c0000059h ; STATUS_REVISION_MISMATCH. Reject the previous packed interface.
    jmp finish_config
invalid_config:
    mov edi,0c000000dh
finish_config:
    jmp ioctl_done
include click.inc
end
