/* Provide the contiguous buffer for Xen PVH guests */
#define pr_fmt(fmt) "xen:" KBUILD_MODNAME ": " fmt

#include <linux/memblock.h>
#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/export.h>
#include <xen/page.h>
#include <xen/xen-ops.h>
#include <xen/phy-dma-ops.h>
#include <xen/swiotlb-xen.h>

#include <asm/dma-mapping.h>

#define MAX_DMA_BITS 32


static void *
xen_phy_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flags, unsigned long attrs)
{
	u64 dma_mask = dev->coherent_dma_mask;
	int order = get_order(size);
	phys_addr_t phys;
	void *ret;

	/* Align the allocation to the Xen page size */
	size = 1UL << (order + XEN_PAGE_SHIFT);

	ret = (void *)__get_free_pages(flags, get_order(size));
	if (!ret)
		return ret;
	phys = virt_to_phys(ret);

	*dma_handle = xen_phys_to_dma(dev, phys);

	if (xen_pvh_create_contiguous_region(phys, order, fls64(dma_mask),
					     dma_handle) != 0)
		goto out_free_pages;
	SetPageXenRemapped(virt_to_page(ret));

	memset(ret, 0, size);
	return ret;

out_free_pages:
	free_pages((unsigned long)ret, get_order(size));
	return NULL;
}

static void
xen_phy_free_coherent(struct device *dev, size_t size, void *vaddr,
		      dma_addr_t dma_handle, unsigned long attrs)
{
	phys_addr_t phys = virt_to_phys(vaddr);
	int order = get_order(size);

	/* Convert the size to actually allocated. */
	size = 1UL << (order + XEN_PAGE_SHIFT);

	if (TestClearPageXenRemapped(virt_to_page(vaddr)))
		xen_pvh_destroy_contiguous_region(phys, order);
	free_pages((unsigned long)vaddr, get_order(size));
}

const struct dma_map_ops xen_phy_dma_ops = {
	.alloc = xen_phy_alloc_coherent,
	.free = xen_phy_free_coherent,
};
EXPORT_SYMBOL_GPL(xen_phy_dma_ops);
