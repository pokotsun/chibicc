.intel_syntax noprefix
.global main
main:
  push rbp
  mov rbp, rsp
  sub rsp, 0
  push 4
  push 3
  pop rsi
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .L.call.1
  mov rax, 0
  call sub2
  jmp .L.end.1
.L.call.1:
  sub rsp, 8
  mov rax, 0
  call sub2
  add rsp, 8
.L.end.1:
  push rax
  pop rax
  jmp .L.return.main
.L.return.main:
  mov rsp, rbp
  pop rbp
  ret
.global sub2
sub2:
  push rbp
  mov rbp, rsp
  sub rsp, 0
  mov [rbp-16], rdi 
  mov [rbp-8], rsi
  lea rax, [rbp-16]
  push rax
  pop rax
  mov rax, [rax]
  push rax
  lea rax, [rbp-8]
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rax
  jmp .L.return.sub2
.L.return.sub2:
  mov rsp, rbp
  pop rbp
  ret
