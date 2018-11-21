# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/bwfm

KMOD=	if_bwwfm
SRCS=	bwfm.c bwfmreg.h bwfmvar.h

# Other
SRCS+=	device_if.h bus_if.h opt_bwfm.h opt_wlan.h

.include <bsd.kmod.mk>
