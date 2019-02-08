/*-
 * Copyright (c) 2017 Ilya Bakulin.  All rights reserved.
 * Copyright (c) 2018-2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Implements the sdiobridge(4) (also in short known as sdiob(4)).
 * This will hide all cam(4) functionality from the SDIO driver implementation
 * which will just be newbus and hence look like any other driver for, e.g.,
 * PCI.  The sdiob(4) will "translate" between the two worlds bridging messages.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cam.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h> /* for cam_path */
#include <cam/cam_debug.h>

#include <dev/sdio/sdiob.h>
#include <dev/sdio/sdio_subr.h>

#ifdef DEBUG
#define	DPRINTF(...)		printf(__VA_ARGS__)
#define	DPRINTFDEV(_dev, ...)	device_printf((_dev), __VA_ARGS__)
#else
#define	DPRINTF(...)
#define	DPRINTFDEV(...)
#endif

struct sdiob_softc {
	uint32_t			sdio_state;
#define	SDIO_STATE_DEAD			0x0001
#define	SDIO_STATE_INITIALIZING		0x0002
#define	SDIO_STATE_READY		0x0004
	uint32_t			nb_state;
#define	NB_STATE_DEAD			0x0001
#define	NB_STATE_SIM_ADDED		0x0002
#define	NB_STATE_PROBE_AND_ATTACH	0x0004


	/* CAM side (including sim_dev). */
	struct card_info		cardinfo;
	struct cam_periph		*periph;
	struct task			discover_task;

	/* Newbus side. */
	device_t			dev;	/* Ourselves. */
	device_t			child;
};

/* -------------------------------------------------------------------------- */

static void __inline
_sdio_printf_dev(device_t dev, int error, const char *_f)
{
	driver_t *dd, *pd;
	device_t pdev;
	devclass_t dc, pdc;

	pdev = device_get_parent(dev);

	dd = device_get_driver(dev);
	pd = device_get_driver(pdev);
	dc = device_get_devclass(dev);
	pdc = device_get_devclass(pdev);
#define DRIVERNAME(d)     ((d)? d->name : "no driver")
	printf("XXX-BZ %s: error %d dev %p\n"
	    "\tdev    %s (%s) driver %p %s softc %p dc %p %s state %s/%s/%s\n"
	    "\tparent %s (%s) driver %p %s softc %p dc %p %s state %s/%s/%s\n", _f, error, dev,
		device_get_nameunit(dev), device_get_desc(dev), dd, DRIVERNAME(dd), device_get_softc(dev),
		dc, devclass_get_name(dc),
		device_is_alive(dev) ? "a" : "-", device_is_attached(dev) ? "a" : "-", device_is_enabled(dev) ? "e" : "-",

		device_get_nameunit(pdev), device_get_desc(pdev), pd, DRIVERNAME(pd), device_get_softc(pdev),
		pdc, devclass_get_name(pdc),
		device_is_alive(pdev) ? "a" : "-", device_is_attached(pdev) ? "a" : "-", device_is_enabled(pdev) ? "e" : "-"
		);

	return;
}

/* -------------------------------------------------------------------------- */

static int
sdiob_probe(device_t dev)
{

	device_printf(dev, "%s:%d\n", __func__, __LINE__);
	device_set_desc(dev, "SDIO CAM-Newbus bridge");
	_sdio_printf_dev(dev, 0, __func__);
	
	return (BUS_PROBE_DEFAULT);
}

static int
sdiob_attach(device_t dev)
{
	struct sdiob_softc *sc;
	int error;

	device_printf(dev, "%s:%d\n", __func__, __LINE__);
	_sdio_printf_dev(dev, 0, __func__);

	printf ("XXX-BZ TODO FIXME >>>> Do funny things here trying to find a child device...\n");
#if 0
	/* Locate our children */
	bus_generic_probe(dev);

	/* launch attachement of the added children */
	bus_generic_attach(dev);
#else
	sc = device_get_softc(dev);
	sc->child = device_add_child(dev, NULL, -1);
	if (sc->child == NULL)
		device_printf(dev, "%s: failed to add child\n", __func__);
	else {
		error = device_probe_and_attach(sc->child);
		if (error != 0)
			device_printf(dev, "%s: device_probe_and_attach(%p %s) failed %d\n", __func__, sc->child,  device_get_nameunit(sc->child), error);
	}
#endif
	printf ("XXX-BZ TODO FIXME <<<< Done funny things here trying to find a child device...\n");

	return (0);
}

static int
sdiob_detach(device_t dev)
{

	device_printf(dev, "%s:%d\n", __func__, __LINE__);
	_sdio_printf_dev(dev, 0, __func__);
	return (0);
}

static device_t
sdiob_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t pdev;

	pdev = device_get_parent(dev);
	printf("XXX-BZ %s:%d dev %p %s pdev %p %s\n", __func__, __LINE__, dev, device_get_nameunit(dev), pdev, device_get_nameunit(pdev));
	return (bus_generic_add_child(dev, order, name, unit));
}

static void
sdiob_driver_added(device_t dev, driver_t *driver)
{

	printf("%s:%d driver %p dev %p %s\n", __func__, __LINE__, driver, dev, device_get_nameunit(dev));
	bus_generic_driver_added(dev, driver);
}



static device_method_t sdiob_methods[] = {

	/* Device interface. */
	DEVMETHOD(device_probe,		sdiob_probe),
	DEVMETHOD(device_attach,	sdiob_attach),
	DEVMETHOD(device_detach,	sdiob_detach),

	/* Bus interface. */
	DEVMETHOD(bus_add_child,	sdiob_add_child),
	DEVMETHOD(bus_driver_added,	sdiob_driver_added),

	DEVMETHOD_END
};

static devclass_t sdiob_devclass;
static driver_t sdiob_driver = {
	SDIOB_NAME_S,
	sdiob_methods,
	0
};

/* -------------------------------------------------------------------------- */

static int
sdio_newbus_probe_attach(struct sdiob_softc *sc)
{
	int error;

	_sdio_printf_dev(sc->dev, __LINE__, __func__);
#if 1
	/* Probe and attach a child if there is one. */
	mtx_lock(&Giant);
	error = device_probe_and_attach(sc->dev);
	mtx_unlock(&Giant);
#else
	error = 0;
#endif
	_sdio_printf_dev(sc->dev, error, __func__);
	if (error != 0)
		return (error);

	sc->nb_state = NB_STATE_PROBE_AND_ATTACH;

	return (error);
}

static int
sdio_newbus_sim_add(struct sdiob_softc *sc)
{
	device_t pdev;
	devclass_t bus_devclass;
	int error;

	/* Add ourselves to our parent (SIM) device. */

	/* Add ourselves to our parent. That way we can become a parent. */
	KASSERT(sc->periph->sim->sim_dev != NULL, ("%s: sim_dev is NULL, sc %p "
	    "periph %p sim %p\n", __func__, sc, sc->periph, sc->periph->sim));

	if (sc->dev == NULL)
		sc->dev = BUS_ADD_CHILD(sc->periph->sim->sim_dev, 0,
		    SDIOB_NAME_S, -1);
	if (sc->dev == NULL)
		return (ENXIO);
	device_set_softc(sc->dev, sc);
	/*
	 * Don't set description here; devclass_add_driver() ->
	 * device_probe_child() -> device_set_driver() will nuke it again.
	 */
	/* XXX-BZ ivars */
printf("%s:%d sc %p sc->dev %p\n", __func__, __LINE__, sc, sc->dev);
	_sdio_printf_dev(sc->dev, __LINE__, __func__);

	pdev = device_get_parent(sc->dev);
	KASSERT(pdev != NULL, ("%s: sc %p dev %p (%s) parent is NULL\n",
	    __func__, sc, sc->dev, device_get_nameunit(sc->dev)));
	bus_devclass = device_get_devclass(pdev);
	if (bus_devclass == NULL) {
		printf("%s: Failed to get devclass from %s.\n", __func__,
		    device_get_nameunit(pdev));
		return (ENXIO);
	}

	mtx_lock(&Giant);
	error = devclass_add_driver(bus_devclass, &sdiob_driver,
	    BUS_PASS_DEFAULT, &sdiob_devclass);
	mtx_unlock(&Giant);
	if (error != 0) {
		printf("%s: Failed to add driver to devclass: %d.\n",
		    __func__, error);
		return (error);
	}

	/* Done. */
	sc->nb_state = NB_STATE_SIM_ADDED;

	error = sdio_newbus_probe_attach(sc);
	return (error);
}

static void
sdiobdiscover(void *context, int pending)
{
	struct cam_periph *periph;
	struct sdiob_softc *sc;
	union ccb *ccb;
	int error;

	KASSERT(context != NULL, ("%s: context is NULL\n", __func__));
	periph = (struct cam_periph *)context;
	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s\n", __func__));

	/* Periph was held for us when this task was enqueued. */
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_release(periph);
		return;
	}

	sc = periph->softc;
	sc->sdio_state = SDIO_STATE_INITIALIZING;

	ccb = xpt_alloc_ccb();
	xpt_setup_ccb(&ccb->ccb_h, periph->path, CAM_PRIORITY_NONE);

	/*
	 * Read CCCR and FBR of each function, get manufacturer and device IDs,
	 * max block size, and whatever else we deem necessary.
	 */
	cam_periph_lock(periph);
	error = get_sdio_card_info(ccb, &sc->cardinfo);
	if  (error != 0)
		sc->sdio_state = SDIO_STATE_READY;
	else
		sc->sdio_state = SDIO_STATE_DEAD;
	cam_periph_unlock(periph);

	xpt_free_ccb(ccb);

	if (error)
		return;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s: num_func %d\n",
	    __func__, sc->cardinfo.num_funcs));

	/*
	 * Now CAM portion of the driver has been initialized and
	 * we know VID/PID of all the functions on the card.
	 * Time to hook into the newbus.
	 */
	error = sdio_newbus_sim_add(sc);
	if (error != 0)
		sc->nb_state = NB_STATE_DEAD;

	return;
}

/* Called at the end of cam_periph_alloc() for us to finish allocation. */
static cam_status
sdiobregister(struct cam_periph *periph, void *arg)
{
	struct sdiob_softc *sc;
	int error;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s: arg %p\n", __func__, arg));
	if (arg == NULL) {
		printf("%s: no getdev CCB, can't register device pariph %p\n",
		    __func__, periph);
		return(CAM_REQ_CMP_ERR);
	}
	if (periph->sim == NULL || periph->sim->sim_dev == NULL) {
		printf("%s: no sim %p or sim_dev %p\n", __func__, periph->sim,
		    (periph->sim != NULL) ? periph->sim->sim_dev : NULL);
		return(CAM_REQ_CMP_ERR);
	}

	sc = (struct sdiob_softc *) malloc(sizeof(*sc), M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (sc == NULL) {
		printf("%s: unable to allocate sc\n", __func__);
		return (CAM_REQ_CMP_ERR);
	}
	sc->sdio_state = SDIO_STATE_DEAD;
	sc->nb_state = NB_STATE_DEAD;
	TASK_INIT(&sc->discover_task, 0, sdiobdiscover, periph);

	/* Refcount until we are setup.  Can't block. */
	error = cam_periph_hold(periph, PRIBIO);
	if (error != 0) {
		printf("%s: lost periph during registration!\n", __func__);
		free(sc, M_DEVBUF);
		return(CAM_REQ_CMP_ERR);
	}
	periph->softc = sc;
	sc->periph = periph;
	cam_periph_unlock(periph);

	error = taskqueue_enqueue(taskqueue_thread, &sc->discover_task);

	cam_periph_lock(periph);
	/* We will continue to hold a refcount for discover_task. */
	/* cam_periph_unhold(periph); */

	xpt_schedule(periph, CAM_PRIORITY_XPT);

	return (CAM_REQ_CMP);
}

static void
sdioboninvalidate(struct cam_periph *periph)
{

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s:\n", __func__));

	return;
}

static void
sdiobcleanup(struct cam_periph *periph)
{

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s:\n", __func__));

	return;
}

static void
sdiobstart(struct cam_periph *periph, union ccb *ccb)
{

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s: ccb %p\n", __func__, ccb));

	return;
}

static void
sdiobasync(void *softc, uint32_t code, struct cam_path *path, void *arg)
{
	struct cam_periph *periph;
	struct ccb_getdev *cgd;
	cam_status status;

	periph = (struct cam_periph *)softc;

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("%s(code=%d)\n", __func__, code));
	switch (code) {
	case AC_FOUND_DEVICE:
		if (arg == NULL)
			break;
		cgd = (struct ccb_getdev *)arg;
		if (cgd->protocol != PROTO_MMCSD)
			break;

		/* We do not support SD memory (Combo) Cards. */
		if ((path->device->mmc_ident_data.card_features &
		    CARD_FEATURE_MEMORY)) {
			CAM_DEBUG(path, CAM_DEBUG_TRACE,
			     ("Memory card, not interested\n"));
			break;
		}

		/*
		 * Allocate a peripheral instance for this device which starts
		 * the probe process.
		 */
		status = cam_periph_alloc(sdiobregister, sdioboninvalidate,
		    sdiobcleanup, sdiobstart, SDIOB_NAME_S, CAM_PERIPH_BIO, path,
		    sdiobasync, AC_FOUND_DEVICE, cgd);
		if (status != CAM_REQ_CMP && status != CAM_REQ_INPROG)
			CAM_DEBUG(path, CAM_DEBUG_PERIPH,
			     ("%s: Unable to attach to new device due to "
			     "status %#02x\n", __func__, status));
		break;
	default:
		CAM_DEBUG(path, CAM_DEBUG_PERIPH,
		     ("%s: cannot handle async code %#02x\n", __func__, code));
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static void
sdiobinit(void)
{
	cam_status status;

	/*
	 * Register for new device notification.  We will be notified for all
	 * already existing ones.
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, sdiobasync, NULL, NULL);
	if (status != CAM_REQ_CMP)
		printf("%s: Failed to attach async callback, statux %#02x",
		    __func__, status);
}

/* This function will allow unloading the KLD. */
static int
sdiobdeinit(void)
{

	return (EOPNOTSUPP);
}

static struct periph_driver sdiobdriver =
{
	.init =		sdiobinit,
	.driver_name =	SDIOB_NAME_S,
	.units =	TAILQ_HEAD_INITIALIZER(sdiobdriver.units),
	.generation =	0,
	.flags =	0,
	.deinit =	sdiobdeinit,
};

PERIPHDRIVER_DECLARE(SDIOB_NAME, sdiobdriver);
MODULE_VERSION(SDIOB_NAME, 1);
