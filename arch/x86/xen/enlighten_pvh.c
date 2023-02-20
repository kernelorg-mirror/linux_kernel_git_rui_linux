// SPDX-License-Identifier: GPL-2.0
#include <linux/acpi.h>
#include <linux/export.h>

#include <xen/hvc-console.h>

#include <asm/io_apic.h>
#include <asm/hypervisor.h>
#include <asm/e820/api.h>

#include <xen/xen.h>
#include <asm/xen/interface.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/page.h>

#include <xen/interface/memory.h>

#include "xen-ops.h"

/*
 * PVH variables.
 *
 * The variable xen_pvh needs to live in a data segment since it is used
 * after startup_{32|64} is invoked, which will clear the .bss segment.
 */
bool __ro_after_init xen_pvh;
EXPORT_SYMBOL_GPL(xen_pvh);

#define MAX_CONTIG_ORDER 9 /* 2MB */
static unsigned long discontig_frames[1<<MAX_CONTIG_ORDER];
static DEFINE_SPINLOCK(xen_reservation_lock);

void __init xen_pvh_init(struct boot_params *boot_params)
{
	u32 msr;
	u64 pfn;

	xen_pvh = 1;
	xen_domain_type = XEN_HVM_DOMAIN;
	xen_start_flags = pvh_start_info.flags;

	msr = cpuid_ebx(xen_cpuid_base() + 2);
	pfn = __pa(hypercall_page);
	wrmsr_safe(msr, (u32)pfn, (u32)(pfn >> 32));

	if (xen_initial_domain())
		x86_init.oem.arch_setup = xen_add_preferred_consoles;
	x86_init.oem.banner = xen_banner;

	xen_efi_init(boot_params);
}

void __init mem_map_via_hcall(struct boot_params *boot_params_p)
{
	struct xen_memory_map memmap;
	int rc;

	memmap.nr_entries = ARRAY_SIZE(boot_params_p->e820_table);
	set_xen_guest_handle(memmap.buffer, boot_params_p->e820_table);
	rc = HYPERVISOR_memory_op(XENMEM_memory_map, &memmap);
	if (rc) {
		xen_raw_printk("XENMEM_memory_map failed (%d)\n", rc);
		BUG();
	}
	boot_params_p->e820_entries = memmap.nr_entries;
}

static int xen_pvh_exchange_memory(unsigned long extents_in, unsigned int order_in,
			       unsigned long *pfns_in,
			       unsigned long extents_out,
			       unsigned int order_out,
			       unsigned long *mfns_out,
			       unsigned int address_bits)
{
	long rc;
	int success;

	struct xen_memory_exchange exchange = {
		.in = {
			.nr_extents   = extents_in,
			.extent_order = order_in,
			.extent_start = pfns_in,
			.domid        = DOMID_SELF
		},
		.out = {
			.nr_extents   = extents_out,
			.extent_order = order_out,
			.extent_start = mfns_out,
			.address_bits = address_bits,
			.domid        = DOMID_SELF
		}
	};

	BUG_ON(extents_in << order_in != extents_out << order_out);

	rc = HYPERVISOR_memory_op(XENMEM_exchange, &exchange);
	success = (exchange.nr_exchanged == extents_in);

	BUG_ON(!success && ((exchange.nr_exchanged != 0) || (rc == 0)));
	BUG_ON(success && (rc != 0));

	return success;
}

int xen_pvh_create_contiguous_region(phys_addr_t pstart, unsigned int order,
				     unsigned int address_bits,
				     dma_addr_t *dma_handle)
{
	unsigned long *in_frames = discontig_frames, out_frame;
	unsigned long  flags;
	int            success;
	unsigned long vstart = (unsigned long)phys_to_virt(pstart);

	/*
	 * Currently an auto-translated guest will not perform I/O, nor will
	 * it require PAE page directories below 4GB. Therefore any calls to
	 * this function are redundant and can be ignored.
	 */

	if (unlikely(order > MAX_CONTIG_ORDER))
		return -ENOMEM;

	if (in_frames) {
		unsigned long vaddr = vstart;
		int i;
		for (i = 0; i < (1UL<<order); i++, vaddr += PAGE_SIZE)
			in_frames[i] = virt_to_pfn(vaddr);
	}

	memset((void *) vstart, 0, PAGE_SIZE << order);

	spin_lock_irqsave(&xen_reservation_lock, flags);

	/* Get a new contiguous memory extent. */
	out_frame = virt_to_pfn(vstart);
	success = xen_pvh_exchange_memory(1UL << order, 0, in_frames,
				      1, order, &out_frame,
				      address_bits);

	spin_unlock_irqrestore(&xen_reservation_lock, flags);

	*dma_handle = out_frame << PAGE_SHIFT;
	return success ? 0 : -ENOMEM;
}

void xen_pvh_destroy_contiguous_region(phys_addr_t pstart, unsigned int order)
{
	unsigned long *out_frames = discontig_frames, in_frame;
	unsigned long  flags;
	int success;
	unsigned long vstart;

	if (unlikely(order > MAX_CONTIG_ORDER))
		return;

	vstart = (unsigned long)phys_to_virt(pstart);
	memset((void *) vstart, 0, PAGE_SIZE << order);

	spin_lock_irqsave(&xen_reservation_lock, flags);

	/* 1. Find start MFN of contiguous extent. */
	in_frame = virt_to_mfn(vstart);

	if (out_frames) {
		unsigned long vaddr = vstart;
		int i;
		for (i = 0; i < (1UL<<order); i++, vaddr += PAGE_SIZE)
			out_frames[i] = virt_to_pfn(vaddr);
	}

	/* 3. Do the exchange for non-contiguous MFNs. */
	success = xen_pvh_exchange_memory(1, order, &in_frame, 1UL << order, 0,
					  out_frames, 0);

	spin_unlock_irqrestore(&xen_reservation_lock, flags);
}
