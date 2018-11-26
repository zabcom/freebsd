# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/bwfm

KMOD=	if_bwwfm
SRCS=	bwfm.c bwfmvar.h
SRCS+=	fwil.c fwil.h
SRCS+=	bwfmreg.h

# Other
SRCS+=	device_if.h bus_if.h opt_bwfm.h opt_wlan.h

.include <bsd.kmod.mk>
