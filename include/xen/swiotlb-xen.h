/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_SWIOTLB_XEN_H
#define __LINUX_SWIOTLB_XEN_H

#include <linux/swiotlb.h>
#include <asm/xen/swiotlb-xen.h>

void xen_dma_sync_for_cpu(struct device *dev, dma_addr_t handle,
			  size_t size, enum dma_data_direction dir);
void xen_dma_sync_for_device(struct device *dev, dma_addr_t handle,
			     size_t size, enum dma_data_direction dir);

extern const struct dma_map_ops xen_swiotlb_dma_ops;

dma_addr_t xen_phys_to_dma(struct device *dev, phys_addr_t paddr);
int range_straddles_page_boundary(phys_addr_t p, size_t size);

#endif /* __LINUX_SWIOTLB_XEN_H */
