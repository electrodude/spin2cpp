DAT
	org	0

_waitcycles
	mov	_waitcycles_end, CNT
	add	_waitcycles_end, arg1
L_047_
	cmps	CNT, _waitcycles_end wc,wz
 if_b	jmp	#L_047_
_waitcycles_ret
	ret

_waitcycles_end
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
