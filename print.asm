section .text
global sprint
sprint:
	push rbp
	mov rbp, rsp
	sub rsp, 16

	mov [rbp-16], rdi
	mov QWORD[rbp-8], 0

	jmp _noi
	_n:
	inc rdi
	inc QWORD[rbp-8]
	_noi:
	cmp BYTE[rdi], 0
	jne _n

	mov rax, 1
	mov rdx, [rbp-8]
	mov rsi, [rbp-16]
	mov rdi, 1
	syscall
;	mov rdi, 0
;	mov rax, 60
;	syscall

	mov rax, [rbp-16]
	mov rsp, rbp
	pop rbp
	ret
