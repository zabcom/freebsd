/*-
 * Copyright (c) 2019 The FreeBSD Foundation
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <dev/sdio/sdiob.h>

#ifdef DEBUG
#define	DPRINTF(...)		printf(__VA_ARGS__)
#define	DPRINTFDEV(_dev, ...)	device_printf((_dev), __VA_ARGS__)
#else
#define	DPRINTF(...)
#define	DPRINTFDEV(...)
#endif

#define	SDIOBTEST_NAME_S		"sdiobtest"

static void
sdiobtest_identify(driver_t *driver, device_t parent)
{
#if 0
	device_t dev;
#endif

	printf("%s:%d driver %p parent %p %s\n", __func__, __LINE__, driver, parent, device_get_nameunit(parent));

#if 0
	if (device_find_child(parent, SDIOBTEST_NAME_S, -1) != NULL)
		return;
	dev = BUS_ADD_CHILD(parent, 0, SDIOBTEST_NAME_S, -1);
	if (dev == NULL)
		device_printf(parent, "%s: failed to add child %s\n", __func__, SDIOBTEST_NAME_S);

	printf("%s:%d driver %p parent %p %s dev %p ready to trock\n", __func__, __LINE__, driver, parent, device_get_nameunit(parent), dev);
#endif
}


static int
sdiobtest_probe(device_t dev)
{

	device_printf(dev, "%s:%d\n", __func__, __LINE__);
	device_set_desc(dev, "TA TAA");
	
	return (BUS_PROBE_DEFAULT);
}

static int
sdiobtest_attach(device_t dev)
{

	device_printf(dev, "%s:%d\n", __func__, __LINE__);
	return (0);
}

static int
sdiobtest_detach(device_t dev)
{

	device_printf(dev, "%s:%d\n", __func__, __LINE__);
	return (0);
}

static void
sdiobtest_driver_added(device_t dev, driver_t *driver)
{

	printf("%s:%d driver %p dev %p %s\n", __func__, __LINE__, driver, dev, device_get_nameunit(dev));
	bus_generic_driver_added(dev, driver);
}

static device_method_t sdiobtest_methods[] = {

	/* Device interface. */
	DEVMETHOD(device_identify,	sdiobtest_identify),
	DEVMETHOD(device_probe,		sdiobtest_probe),
	DEVMETHOD(device_attach,	sdiobtest_attach),
	DEVMETHOD(device_detach,	sdiobtest_detach),

	/* Bus interface. */
	DEVMETHOD(bus_driver_added,	sdiobtest_driver_added),

	DEVMETHOD_END
};

static devclass_t sdiobtest_devclass;
static driver_t sdiobtest_driver = {
	"sdiobtest",
	sdiobtest_methods,
	0
};

DRIVER_MODULE(sdiobtest, SDIOB_NAME, sdiobtest_driver, sdiobtest_devclass,
    NULL, NULL);
MODULE_DEPEND(sdiobtest, sdiobridge, 1, 1, 1);
