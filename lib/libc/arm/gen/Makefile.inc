#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c alloca.S fabs.c \
	getcontextx.c infinity.c ldexp.c makecontext.c \
	__aeabi_read_tp.c setjmp.S signalcontext.c sigsetjmp.S divsi3.S flt_rounds.c
