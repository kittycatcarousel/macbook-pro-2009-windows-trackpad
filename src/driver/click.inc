; SPDX-License-Identifier: MIT
; A timer releases tap clicks. The click lock protects the timer and input state.
extern wdf_lock_create:dword, wdf_lock_acquire:dword, wdf_lock_release:dword
extern wdf_timer_create:dword, wdf_timer_start:dword, wdf_timer_stop:dword
extern wdf_timer_parent:dword, wdf_context:dword, context_type:dword
extern queue_continue:near, cleanup_continue:near
public runtime_queue, runtime_click, runtime_cleanup
extern dequeue_packet:near, read_continue:near, read_forward:near, read_send:near
extern usb_complete:near
extern wdf_format:dword, wdf_completion:dword, wdf_target:dword
extern wdf_irp:dword, wdf_complete:dword
public runtime_idle, runtime_read

CLICK_MS equ 18f0h
CLICK_TIMER equ 18f4h
CLICK_LOCK equ 18f8h
CLICK_ACTIVE equ 18fch
CLICK_BUTTONS equ 1900h
CLICK_FINGERS equ 1904h
CLICK_DUE equ 1908h

; Use the input button state when Apple repeats an idle report.
runtime_idle:
    mov al,byte ptr [esi+CLICK_BUTTONS]
    mov [ebp-3],al
    jmp motion_continue

; Set a USB read deadline while a tap press is active.
; EDI is the context. EBX is the URB. The original frame has 16 free bytes.
runtime_read:
    push esi
    mov esi,edi
    call click_acquire
    lea eax,[esi+14b4h]
    push eax
    call dequeue_packet
    test eax,eax
    jnz read_packet
    cmp dword ptr [esi+CLICK_ACTIVE],0
    je read_ordinary
    call click_clock
    sub eax,[esi+CLICK_DUE]
    sbb edx,[esi+CLICK_DUE+4]
    js read_deadline
    mov eax,-1
    mov edx,-1
read_deadline:
    mov dword ptr [ebp-16],16
    mov dword ptr [ebp-12],1 ; WDF_REQUEST_SEND_OPTION_TIMEOUT.
    mov [ebp-8],eax
    mov [ebp-4],edx
    call click_unlock
    pop esi
    mov esi,[ebp+12]
    push esi
    push dword ptr [wdf_globals]
    call dword ptr [wdf_format]
    push dword ptr [ebp+8]
    push offset click_read_complete
    push esi
    push dword ptr [wdf_globals]
    call dword ptr [wdf_completion]
    push dword ptr [ebp+8]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_target]
    lea ecx,[ebp-16]
    push ecx
    jmp read_send
read_packet:
    push eax
    call click_unlock
    pop eax
    pop esi
    jmp read_continue
read_ordinary:
    call click_unlock
    pop esi
    jmp read_forward

; Only reads with our deadline use this callback. Keep other completion results.
click_read_complete:
    mov eax,[esp+12]
    cmp dword ptr [eax+8],0c00000b5h ; STATUS_IO_TIMEOUT.
    jne usb_complete
    push ebp
    mov ebp,esp
    sub esp,12
    push ebx
    push esi
    push edi
    mov edx,[ebp+20]
    mov ecx,[wdf_globals]
    push dword ptr [context_type]
    call dword ptr [wdf_context]
    mov esi,eax
    push dword ptr [ebp+8]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_irp]
    mov eax,[eax+60h]
    mov ebx,[eax+4]
    mov edi,[ebx+1ch]
    call click_acquire
    cmp dword ptr [esi+CLICK_ACTIVE],0
    je read_take
    call click_clock
    sub eax,[esi+CLICK_DUE]
    sbb edx,[esi+CLICK_DUE+4]
    js read_take
    call click_release
read_take:
    lea eax,[esi+14b4h]
    push eax
    call dequeue_packet
    test eax,eax
    jz read_empty
    mov edx,[eax]
    mov [ebp-12],edx
    mov edx,[eax+4]
    mov [ebp-8],edx
    mov dx,[eax+8]
    mov [ebp-4],dx
    jmp read_report
read_empty:
    mov dword ptr [ebp-12],0
    mov dword ptr [ebp-8],0
    mov eax,[esi+CLICK_BUTTONS]
    or eax,[esi+CLICK_ACTIVE]
    mov [ebp-3],al
read_report:
    mov al,[ebp-3]
    mov [esi+18a9h],al
    call click_unlock
    mov byte ptr [edi],2
    mov al,[ebp-3]
    mov [edi+1],al
    mov al,[ebp-12]
    mov [edi+2],al
    mov al,[ebp-10]
    neg al
    mov [edi+3],al
    movsx eax,word ptr [ebp-6]
    call click_wheel
    mov [edi+4],al
    movsx eax,word ptr [ebp-8]
    call click_wheel
    mov [edi+5],al
    mov dword ptr [ebx+4],0 ; USBD_STATUS_SUCCESS.
    mov dword ptr [ebx+18h],6
    push 0
    push dword ptr [ebp+8]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_complete]
    pop edi
    pop esi
    pop ebx
    leave
    ret 16
click_wheel:
    test eax,eax
    jz wheel_done
    cmp eax,8
    jge wheel_divide
    cmp eax,-8
    jle wheel_divide
    sar eax,31
    or eax,1
    ret
wheel_divide:
    movsx eax,al
    cdq
    and edx,7
    add eax,edx
    sar eax,3
wheel_done:
    ret

; ESI is the device context. The argument is its WDF device handle.
click_create:
    push ebp
    mov ebp,esp
    sub esp,48
    push edi
    lea edi,[ebp-48]
    xor eax,eax
    mov ecx,12
    rep stosd
    mov dword ptr [ebp-48],32 ; WDF_OBJECT_ATTRIBUTES.
    mov dword ptr [ebp-36],1
    mov dword ptr [ebp-32],1
    mov eax,[ebp+8]
    mov [ebp-28],eax
    lea eax,[esi+CLICK_LOCK]
    push eax
    lea eax,[ebp-48]
    push eax
    push dword ptr [wdf_globals]
    call dword ptr [wdf_lock_create]
    test eax,eax
    js create_done
    mov dword ptr [ebp-16],16 ; KMDF 1.7 WDF_TIMER_CONFIG.
    mov dword ptr [ebp-12],offset click_expired
    lea eax,[esi+CLICK_TIMER]
    push eax
    lea eax,[ebp-48]
    push eax
    lea eax,[ebp-16]
    push eax
    push dword ptr [wdf_globals]
    call dword ptr [wdf_timer_create]
create_done:
    pop edi
    leave
    ret 4

; XP x86 KeQueryInterruptTime: read the shared 100 ns clock without a torn value.
click_clock:
    mov edx,dword ptr ds:[0ffdf000ch]
    mov eax,dword ptr ds:[0ffdf0008h]
    cmp edx,dword ptr ds:[0ffdf0010h]
    jne click_clock
    ret
click_acquire:
    push dword ptr [esi+CLICK_LOCK]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_lock_acquire]
    ret
click_unlock:
    push dword ptr [esi+CLICK_LOCK]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_lock_release]
    ret
click_arm:
    neg eax
    adc edx,0
    neg edx
    push edx
    push eax
    push dword ptr [esi+CLICK_TIMER]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_timer_start]
    ret
click_cancel:
    call click_acquire
    call click_release
    jmp click_unlock
; The caller holds the click lock. Preserve the real button state on release.
click_release:
    cmp dword ptr [esi+CLICK_ACTIVE],0
    je release_done
    mov dword ptr [esi+CLICK_ACTIVE],0
    push 0
    push dword ptr [esi+CLICK_TIMER]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_timer_stop]
    push 0
    push 0
    push 0
    mov al,byte ptr [esi+CLICK_FINGERS]
    mov [esp+8],al
    mov al,byte ptr [esi+CLICK_BUTTONS]
    mov [esp+9],al
    mov eax,esp
    push eax
    lea eax,[esi+14b4h]
    push eax
    call click_queue_original
    add esp,12
release_done:
    ret

; Calls from the two Apple tap-press sites use this entry point.
runtime_click:
    mov eax,1
    jmp queue_enter
runtime_queue:
    xor eax,eax
queue_enter:
    push ebp
    mov ebp,esp
    sub esp,16
    push ebx
    push esi
    push edi
    mov ebx,eax
    mov esi,[ebp+8]
    sub esi,14b4h
    mov edi,[ebp+12]
    mov eax,[edi]
    mov [ebp-16],eax
    mov eax,[edi+4]
    mov [ebp-12],eax
    mov ax,[edi+8]
    mov [ebp-8],ax
    call click_acquire
    test ebx,ebx
    jnz queue_tap
    movzx eax,byte ptr [edi+9]
    cmp eax,[esi+CLICK_BUTTONS]
    jne queue_cancel
    cmp dword ptr [edi],0 ; Finger movement ends a pending tap press.
    je queue_normal
queue_cancel:
    call click_release
queue_normal:
    movzx eax,byte ptr [edi+9]
    mov [esi+CLICK_BUTTONS],eax
    movzx eax,byte ptr [edi+8]
    mov [esi+CLICK_FINGERS],eax
    mov al,byte ptr [esi+CLICK_ACTIVE]
    or [ebp-7],al
    jmp queue_send
queue_tap:
    call click_release
    mov eax,[esi+CLICK_MS]
    test eax,eax
    jz queue_send
    movzx ecx,byte ptr [edi+9]
    mov [esi+CLICK_ACTIVE],ecx
    mov ecx,10000
    mul ecx
    mov ebx,eax
    mov edi,edx
    call click_clock
    add eax,ebx
    adc edx,edi
    mov [esi+CLICK_DUE],eax
    mov [esi+CLICK_DUE+4],edx
    mov eax,ebx
    mov edx,edi
    call click_arm
queue_send:
    lea eax,[ebp-16]
    push eax
    push dword ptr [ebp+8]
    call click_queue_original
    call click_unlock
    pop edi
    pop esi
    pop ebx
    leave
    ret 8
click_queue_original:
    push ebp
    mov ebp,esp
    jmp queue_continue

click_expired:
    push esi
    push dword ptr [esp+8]
    push dword ptr [wdf_globals]
    call dword ptr [wdf_timer_parent]
    mov edx,eax
    mov ecx,[wdf_globals]
    push dword ptr [context_type]
    call dword ptr [wdf_context]
    mov esi,eax
    call click_acquire
    cmp dword ptr [esi+CLICK_ACTIVE],0
    je expired_done
    call click_clock
    sub eax,[esi+CLICK_DUE]
    sbb edx,[esi+CLICK_DUE+4]
    jns expired_release
    ; An earlier callback can run after a new tap starts. Use the new deadline.
    neg eax
    adc edx,0
    neg edx
    call click_arm
    jmp expired_done
expired_release:
    call click_release
expired_done:
    call click_unlock
    pop esi
    ret 4

; Stop callbacks before the original device cleanup releases its resources.
runtime_cleanup:
    push ebp
    mov ebp,esp
    pushad
    mov edx,[ebp+8]
    mov ecx,[wdf_globals]
    push dword ptr [context_type]
    call dword ptr [wdf_context]
    mov eax,[eax+CLICK_TIMER]
    test eax,eax
    jz cleanup_done
    push 1
    push eax
    push dword ptr [wdf_globals]
    call dword ptr [wdf_timer_stop]
cleanup_done:
    popad
    jmp cleanup_continue
