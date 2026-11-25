/* QEMU Bao reference only. Reserve and map the dedicated FDT resource.
 * No userspace device, raw physical access, reset, or mailbox writes.
 * Mapping-only control is non-cacheable and never dereferences the window.
 * Coherent mode requires a separate mediator proof before enabling it.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/errno.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/vm.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define TMR_BASE ((rman_res_t)0xf0000000)
#define TMR_SIZE ((rman_res_t)0x10000)

struct tmr_map_softc {
    struct resource *res;
    struct resource_map map;
    int rid;
    int mapped;
};

/* Exactly one explicitly declared mapping mode. */
static int
tmr_map_mode(phandle_t node)
{
    int control = OF_hasprop(node, "exodus,mapping-only");
    int coherent = OF_hasprop(node, "exodus,coherent-normal-memory");
    if (control == coherent)
        return (0);
    if (OF_getproplen(node, control ? "exodus,mapping-only" :
        "exodus,coherent-normal-memory") != 0)
        return (0);
    return (control ? 1 : 2);
}

static int
tmr_map_probe(device_t dev)
{
    pcell_t role;
    phandle_t node = ofw_bus_get_node(dev);
    char model[64] = {0};
    phandle_t root = OF_finddevice("/");
    if (root == -1 || OF_getprop(root, "model", model, sizeof(model)) !=
        sizeof("Tumor Bao QEMU reference") ||
        strcmp(model, "Tumor Bao QEMU reference") != 0 ||
        !ofw_bus_status_okay(dev) ||
        !ofw_bus_is_compatible(dev, "exodus,tumor-mailbox-reference-v1") ||
        OF_getencprop(node, "exodus,role", &role, sizeof(role)) != sizeof(role) ||
        (role != 1 && role != 2) ||
        tmr_map_mode(node) == 0)
        return (ENXIO);
    device_set_desc(dev, "Tumor dedicated reference mailbox resource");
    return (BUS_PROBE_DEFAULT);
}

static int
tmr_map_detach(device_t dev)
{
    struct tmr_map_softc *sc = device_get_softc(dev);
    int error;
    if (sc->mapped) {
        error = bus_unmap_resource(dev, sc->res, &sc->map);
        if (error != 0)
            return (error);
        sc->mapped = 0;
    }
    if (sc->res != NULL) {
        error = bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
        if (error != 0)
            return (error);
        sc->res = NULL;
    }
    return (0);
}

static int
tmr_map_attach(device_t dev)
{
    struct tmr_map_softc *sc = device_get_softc(dev);
    struct resource_map_request req;
    int error, mode;
    rman_res_t start, count;
    mode = tmr_map_mode(ofw_bus_get_node(dev));
    if (mode == 0)
        return (EINVAL);
    /* Validate the declared resource before reserving or mapping it. */
    sc->rid = 0;
    error = bus_get_resource(dev, SYS_RES_MEMORY, sc->rid, &start, &count);
    if (error != 0)
        return (error);
    if (start != TMR_BASE || count != TMR_SIZE)
        return (EINVAL);
    sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
        RF_ACTIVE | RF_UNMAPPED);
    if (sc->res == NULL)
        return (ENXIO);
    if (rman_get_start(sc->res) != TMR_BASE ||
        rman_get_size(sc->res) != TMR_SIZE) {
        (void)tmr_map_detach(dev);
        return (EINVAL);
    }
    resource_init_map_request(&req);
    req.memattr = mode == 1 ? VM_MEMATTR_UNCACHEABLE : VM_MEMATTR_WRITE_BACK;
    req.length = TMR_SIZE;
    error = bus_map_resource(dev, sc->res, &req, &sc->map);
    if (error != 0) {
        (void)tmr_map_detach(dev);
        return (error);
    }
    sc->mapped = 1;
    if (sc->map.r_vaddr == NULL || sc->map.r_size != TMR_SIZE) {
        (void)tmr_map_detach(dev);
        return (ENXIO);
    }
    device_printf(dev, "%s; no shared-memory access performed\n",
        mode == 1 ? "TMR_MAPPING_ONLY_ATTACHED" : "TMR_COHERENT_MAPPING_ATTACHED");
    return (0);
}
static device_method_t tmr_map_methods[] = {
    DEVMETHOD(device_probe, tmr_map_probe),
    DEVMETHOD(device_attach, tmr_map_attach),
    DEVMETHOD(device_detach, tmr_map_detach),
    DEVMETHOD_END
};
static driver_t tmr_map_driver = {
    "tumor_mailbox", tmr_map_methods, sizeof(struct tmr_map_softc)
};
DRIVER_MODULE(tumor_mailbox, ofwbus, tmr_map_driver, 0, 0);
MODULE_VERSION(tumor_mailbox, 1);
