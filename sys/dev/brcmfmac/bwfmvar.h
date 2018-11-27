/* $OpenBSD: bwfmvar.h,v 1.15 2018/07/17 19:44:38 patrick Exp $ */
/*
 * Copyright (c) 2010-2016 Broadcom Corporation
 * Copyright (c) 2016,2017 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BRCMFMAC_VAR_H
#define _BRCMFMAC_VAR_H

/* Chipcommon Core Chip IDs */
#define BRCM_CC_43143_CHIP_ID		43143
#define BRCM_CC_43235_CHIP_ID		43235
#define BRCM_CC_43236_CHIP_ID		43236
#define BRCM_CC_43238_CHIP_ID		43238
#define BRCM_CC_43241_CHIP_ID		0x4324
#define BRCM_CC_43242_CHIP_ID		43242
#define BRCM_CC_4329_CHIP_ID		0x4329
#define BRCM_CC_4330_CHIP_ID		0x4330
#define BRCM_CC_4334_CHIP_ID		0x4334
#define BRCM_CC_43340_CHIP_ID		43340
#define BRCM_CC_43362_CHIP_ID		43362
#define BRCM_CC_4335_CHIP_ID		0x4335
#define BRCM_CC_4339_CHIP_ID		0x4339
#define BRCM_CC_43430_CHIP_ID		43430
#define BRCM_CC_4345_CHIP_ID		0x4345
#define BRCM_CC_43465_CHIP_ID		43465
#define BRCM_CC_4350_CHIP_ID		0x4350
#define BRCM_CC_43525_CHIP_ID		43525
#define BRCM_CC_4354_CHIP_ID		0x4354
#define BRCM_CC_4356_CHIP_ID		0x4356
#define BRCM_CC_43566_CHIP_ID		43566
#define BRCM_CC_43567_CHIP_ID		43567
#define BRCM_CC_43569_CHIP_ID		43569
#define BRCM_CC_43570_CHIP_ID		43570
#define BRCM_CC_4358_CHIP_ID		0x4358
#define BRCM_CC_4359_CHIP_ID		0x4359
#define BRCM_CC_43602_CHIP_ID		43602
#define BRCM_CC_4365_CHIP_ID		0x4365
#define BRCM_CC_4366_CHIP_ID		0x4366
#define BRCM_CC_4371_CHIP_ID		0x4371
#define CY_CC_4373_CHIP_ID		0x4373

/* Defaults */
#define BWFM_DEFAULT_SCAN_CHANNEL_TIME	40
#define BWFM_DEFAULT_SCAN_UNASSOC_TIME	40
#define BWFM_DEFAULT_SCAN_PASSIVE_TIME	120

#define	BWFM_LOCK_INIT(_sc)	mtx_init(&(_sc)->sc_mtx, \
    device_get_nameunit((_sc)->sc_dev), MTX_NETWORK_LOCK, MTX_DEF);
#define	BWFM_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	BWFM_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	BWFM_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define	BWFM_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)

struct bwfm_softc;

struct bwfm_core {
	uint16_t	 co_id;
	uint16_t	 co_rev;
	uint32_t	 co_base;
	uint32_t	 co_wrapbase;
	LIST_ENTRY(bwfm_core) co_link;
};

struct bwfm_chip {
	uint32_t	 ch_chip;
	uint32_t	 ch_chiprev;
	uint32_t	 ch_cc_caps;
	uint32_t	 ch_cc_caps_ext;
	uint32_t	 ch_pmucaps;
	uint32_t	 ch_pmurev;
	uint32_t	 ch_rambase;
	uint32_t	 ch_ramsize;
	uint32_t	 ch_srsize;
	char		 ch_name[8];
	LIST_HEAD(,bwfm_core) ch_list;
	int (*ch_core_isup)(struct bwfm_softc *, struct bwfm_core *);
	void (*ch_core_disable)(struct bwfm_softc *, struct bwfm_core *,
	    uint32_t prereset, uint32_t reset);
	void (*ch_core_reset)(struct bwfm_softc *, struct bwfm_core *,
	    uint32_t prereset, uint32_t reset, uint32_t postreset);
};

struct bwfm_bus_ops {
	int (*bs_preinit)(struct bwfm_softc *);
	void (*bs_stop)(struct bwfm_softc *);
	int (*bs_txcheck)(struct bwfm_softc *);
	int (*bs_txdata)(struct bwfm_softc *, struct mbuf *);
	int (*bs_txctl)(struct bwfm_softc *, void *);
};

struct bwfm_buscore_ops {
	uint32_t (*bc_read)(struct bwfm_softc *, uint32_t);
	void (*bc_write)(struct bwfm_softc *, uint32_t, uint32_t);
	int (*bc_prepare)(struct bwfm_softc *);
	int (*bc_reset)(struct bwfm_softc *);
	int (*bc_setup)(struct bwfm_softc *);
	void (*bc_activate)(struct bwfm_softc *, uint32_t);
};

struct bwfm_proto_ops {
	int (*proto_query_dcmd)(struct bwfm_softc *, int, int,
	    char *, uint32_t *, int *);
	int (*proto_set_dcmd)(struct bwfm_softc *, int, int,
	    char *, uint32_t, int *);
	void (*proto_rx)(struct bwfm_softc *, struct mbuf *);
	void (*proto_rxctl)(struct bwfm_softc *, char *, size_t);
};
extern struct bwfm_proto_ops bwfm_proto_bcdc_ops;

struct bwfm_host_cmd {
	void	 (*cb)(struct bwfm_softc *, void *);
	uint8_t	 data[256];
};

struct bwfm_cmd_key {
	struct ieee80211_node	 *ni;
	struct ieee80211_key	 *k;
};

struct bwfm_cmd_mbuf {
	struct mbuf		 *m;
};

struct bwfm_cmd_flowring_create {
	struct mbuf		*m;
	int			 flowid;
	int			 prio;
};

struct bwfm_host_cmd_ring {
#define BWFM_HOST_CMD_RING_COUNT	32
	struct bwfm_host_cmd	 cmd[BWFM_HOST_CMD_RING_COUNT];
	int			 cur;
	int			 next;
	int			 queued;
};

struct bwfm_proto_bcdc_ctl {
	int				 reqid;
	char				*buf;
	size_t				 len;
	int				 done;
	TAILQ_ENTRY(bwfm_proto_bcdc_ctl) next;
};

#define	brcmf_softc	bwfm_softc
struct bwfm_softc {
	device_t		sc_dev;
	struct ieee80211com	sc_ic;
	struct mtx		sc_mtx;
	struct mbufq		sc_snd;
	int			qfullmsk;
	uint32_t		sc_state;
#define	BWFM_STATE_ATTACHED		0x00000001
#define	BWFM_STATE_INITALIZED		0x00000002
	struct intr_config_hook	sc_preinit_hook;
	struct callout		sc_watchdog;
	struct task		sc_task;
	bool			sc_fwil_fwerr;
	uint8_t			sc_d11inf_io_type;
#define	BRCMU_D11N_IOTYPE		1
#define	BRCMU_D11AC_IOTYPE		2
	unsigned char		sc_proto_buf[BRCMF_DCMD_MAXLEN];

	struct ifmedia		 sc_media;
	struct bwfm_bus_ops	*sc_bus_ops;
	struct bwfm_buscore_ops	*sc_buscore_ops;
	struct bwfm_proto_ops	*sc_proto_ops;
	struct bwfm_chip	 sc_chip;

	int			 sc_tx_timer;

	int			 (*sc_newstate)(struct ieee80211com *,
				     enum ieee80211_state, int);
	struct bwfm_host_cmd_ring sc_cmdq;
	struct taskq		*sc_taskq;

	int			 sc_bcdc_reqid;
	TAILQ_HEAD(, bwfm_proto_bcdc_ctl) sc_bcdc_rxctlq;
};

/* Called from various bus attachment parts. */
int bwfm_attach(device_t);
int bwfm_detach(device_t);

/* OpenBSD stuff.. */
int bwfm_chip_attach(struct bwfm_softc *);
int bwfm_chip_set_active(struct bwfm_softc *, uint32_t);
void bwfm_chip_set_passive(struct bwfm_softc *);
int bwfm_chip_sr_capable(struct bwfm_softc *);
struct bwfm_core *bwfm_chip_get_core(struct bwfm_softc *, int);
struct bwfm_core *bwfm_chip_get_pmu(struct bwfm_softc *);
void bwfm_rx(struct bwfm_softc *, struct mbuf *);
void bwfm_do_async(struct bwfm_softc *, void (*)(struct bwfm_softc *, void *),
    void *, int);

#endif /* _BRCMFMAC_VAR_H */
