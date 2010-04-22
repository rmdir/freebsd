/*************************************************************************
Copyright (c) 2003-2007  Cavium Networks (support@cavium.com). All rights
reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Cavium Networks nor the names of
      its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

This Software, including technical data, may be subject to U.S. export  control laws, including the U.S. Export Administration Act and its  associated regulations, and may be subject to export or import  regulations in other countries.

TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
*************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/module.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

/*
 * XXX/juli
 * Convert 0444 to tunables, 0644 to sysctls.
 */
#if defined(CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS) && CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS
int num_packet_buffers = CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS;
#else
int num_packet_buffers = 1024;
#endif
#if 0
module_param(num_packet_buffers, int, 0444);
MODULE_PARM_DESC(num_packet_buffers, "\n"
		 "\t\tNumber of packet buffers to allocate and store in the\n"
		 "\t\tFPA. By default, 1024 packet buffers are used unless\n"
		 "\t\tCONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS is defined.");
#endif

int pow_receive_group = 15;
#if 0
module_param(pow_receive_group, int, 0444);
MODULE_PARM_DESC(pow_receive_group, "\n"
		 "\t\tPOW group to receive packets from. All ethernet hardware\n"
		 "\t\twill be configured to send incomming packets to this POW\n"
		 "\t\tgroup. Also any other software can submit packets to this\n"
		 "\t\tgroup for the kernel to process.");
#endif

int pow_send_group = -1;
#if 0
module_param(pow_send_group, int, 0644);
MODULE_PARM_DESC(pow_send_group, "\n"
		 "\t\tPOW group to send packets to other software on. This\n"
		 "\t\tcontrols the creation of the virtual device pow0.\n"
		 "\t\talways_use_pow also depends on this value.");
#endif

int always_use_pow;
#if 0
module_param(always_use_pow, int, 0444);
MODULE_PARM_DESC(always_use_pow, "\n"
		 "\t\tWhen set, always send to the pow group. This will cause\n"
		 "\t\tpackets sent to real ethernet devices to be sent to the\n"
		 "\t\tPOW group instead of the hardware. Unless some other\n"
		 "\t\tapplication changes the config, packets will still be\n"
		 "\t\treceived from the low level hardware. Use this option\n"
		 "\t\tto allow a CVMX app to intercept all packets from the\n"
		 "\t\tlinux kernel. You must specify pow_send_group along with\n"
		 "\t\tthis option.");
#endif

char pow_send_list[128] = "";
#if 0
module_param_string(pow_send_list, pow_send_list, sizeof(pow_send_list), 0444);
MODULE_PARM_DESC(pow_send_list, "\n"
		 "\t\tComma separated list of ethernet devices that should use the\n"
		 "\t\tPOW for transmit instead of the actual ethernet hardware. This\n"
		 "\t\tis a per port version of always_use_pow. always_use_pow takes\n"
		 "\t\tprecedence over this list. For example, setting this to\n"
		 "\t\t\"eth2,spi3,spi7\" would cause these three devices to transmit\n"
		 "\t\tusing the pow_send_group.");
#endif


#if 0
static int disable_core_queueing = 1;
module_param(disable_core_queueing, int, 0444);
MODULE_PARM_DESC(disable_core_queueing, "\n"
		"\t\tWhen set the networking core's tx_queue_len is set to zero.  This\n"
		"\t\tallows packets to be sent without lock contention in the packet scheduler\n"
		"\t\tresulting in some cases in improved throughput.\n");
#endif

extern int octeon_is_simulation(void);

/**
 * Exported from the kernel so we can determine board information. It is
 * passed by the bootloader to the kernel.
 */
extern cvmx_bootinfo_t *octeon_bootinfo;

/*
 * XXX
 * This should be a per-if callout?
 */
#if 0
/**
 * Periodic timer to check auto negotiation
 */
static struct timer_list cvm_oct_poll_timer;
#endif

/**
 * Array of every ethernet device owned by this driver indexed by
 * the ipd input port number.
 */
struct ifnet *cvm_oct_device[TOTAL_NUMBER_OF_PORTS];

#if 0
extern struct semaphore mdio_sem;
#endif


/**
 * Periodic timer tick for slow management operations
 *
 * @param arg    Device to check
 */
#if 0
static void cvm_do_timer(unsigned long arg)
{
	static int port;
	if (port < CVMX_PIP_NUM_INPUT_PORTS) {
		if (cvm_oct_device[port]) {
			int queues_per_port;
#if 0
			int qos;
#endif
			cvm_oct_private_t *priv = (cvm_oct_private_t *)cvm_oct_device[port]->if_softc;
			if (priv->poll) 
			{
#if 0
				/* skip polling if we don't get the lock */
				if(!down_trylock(&mdio_sem)) {
					priv->poll(cvm_oct_device[port]);
					up(&mdio_sem);
				}
#else
				panic("%s: wrap priv->poll with an mdio lock.", __func__);
#endif
			}

			queues_per_port = cvmx_pko_get_num_queues(port);
			/* Drain any pending packets in the free list */
#if 0
			for (qos = 0; qos < queues_per_port; qos++) {
				if (m_queue_len(&priv->tx_free_list[qos])) {
					spin_lock(&priv->tx_free_list[qos].lock);
					while (m_queue_len(&priv->tx_free_list[qos]) > cvmx_fau_fetch_and_add32(priv->fau+qos*4, 0))
						dev_kfree_m(__m_dequeue(&priv->tx_free_list[qos]));
					spin_unlock(&priv->tx_free_list[qos].lock);
				}
			}
#else
			panic("%s: need to implement Tx queue draining.", __func__);
#endif
#if 0
			cvm_oct_device[port]->get_stats(cvm_oct_device[port]);
#else
			panic("%s: need to implement stats getting.", __func__);
#endif
		}
		port++;
#if 0
		/* Poll the next port in a 50th of a second.
		   This spreads the polling of ports out a little bit */
		mod_timer(&cvm_oct_poll_timer, jiffies + HZ/50);
#endif
	} else {
		port = 0;
#if 0
		/* All ports have been polled. Start the next iteration through
		   the ports in one second */
		mod_timer(&cvm_oct_poll_timer, jiffies + HZ);
#endif
	}
}
#endif


/**
 * Configure common hardware for all interfaces
 */
static void cvm_oct_configure_common_hw(void)
{
#if 0
	int r;
#endif
	/* Setup the FPA */
	cvmx_fpa_enable();
	cvm_oct_mem_fill_fpa(CVMX_FPA_PACKET_POOL, CVMX_FPA_PACKET_POOL_SIZE, num_packet_buffers);
	cvm_oct_mem_fill_fpa(CVMX_FPA_WQE_POOL, CVMX_FPA_WQE_POOL_SIZE, num_packet_buffers);
	if (CVMX_FPA_OUTPUT_BUFFER_POOL != CVMX_FPA_PACKET_POOL)
		cvm_oct_mem_fill_fpa(CVMX_FPA_OUTPUT_BUFFER_POOL, CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE, 128);

	if (USE_RED)
		cvmx_helper_setup_red(num_packet_buffers/4, num_packet_buffers/8);

	/* Enable the MII interface */
	if (!octeon_is_simulation())
		cvmx_write_csr(CVMX_SMI_EN, 1);

#if 0
	/* Register an IRQ hander for to receive POW interrupts */
	r = request_irq(OCTEON_IRQ_WORKQ0 + pow_receive_group, cvm_oct_do_interrupt, IRQF_SHARED, "Ethernet", cvm_oct_device);
#endif

#ifdef SMP
	if (USE_MULTICORE_RECEIVE) {
#if 0
		preempt_disable();
		{
			int cpu;
			for (cpu = 0; cpu < NR_CPUS; cpu++) {
				if (cpu_online(cpu) &&
				   (cpu != smp_processor_id())) {
					cvmx_ciu_intx0_t en;
					en.u64 = cvmx_read_csr(CVMX_CIU_INTX_EN0(cpu_logical_map(cpu)*2));
					en.s.workq |= (1<<pow_receive_group);
					cvmx_write_csr(CVMX_CIU_INTX_EN0(cpu_logical_map(cpu)*2), en.u64);
				}
			}
		}
		preempt_enable();
#else
		panic("%s: need to implement CPU enumeration.", __func__);
#endif
	}
#endif
}


/**
 * Registers a intercept callback for the names ethernet
 * device. It returns the Linux device structure for the
 * ethernet port. Usign a callback of NULL will remove
 * the callback. Note that this callback must not disturb
 * scratch. It will be called with SYNCIOBDMAs in progress
 * and userspace may be using scratch. It also must not
 * disturb the group mask.
 *
 * @param device_name
 *                 Device name to register for. (Example: "eth0")
 * @param callback Intercept callback to set.
 * @return Device structure for the ethernet port or NULL on failure.
 */
#if 0
struct ifnet *cvm_oct_register_callback(const char *device_name, cvm_oct_callback_t callback)
{
	int port;

	for (port = 0; port < TOTAL_NUMBER_OF_PORTS; port++) {
		if (cvm_oct_device[port] &&
		    (strcmp(device_name, cvm_oct_device[port]->name) == 0)) {
			cvm_oct_private_t *priv = (cvm_oct_private_t *)cvm_oct_device[port]->if_softc;
			priv->intercept_cb = callback;
			wmb();
			return cvm_oct_device[port];
		}
	}

	return NULL;
}
#endif


/**
 * Free a work queue entry received in a intercept callback.
 *
 * @param work_queue_entry
 *               Work queue entry to free
 * @return Zero on success, Negative on failure.
 */
int cvm_oct_free_work(void *work_queue_entry)
{
	cvmx_wqe_t *work = work_queue_entry;

	int segments = work->word2.s.bufs;
	cvmx_buf_ptr_t segment_ptr = work->packet_ptr;

	while (segments--) {
		cvmx_buf_ptr_t next_ptr = *(cvmx_buf_ptr_t *)cvmx_phys_to_ptr(segment_ptr.s.addr-8);
		if (__predict_false(!segment_ptr.s.i))
			cvmx_fpa_free(cvm_oct_get_buffer_ptr(segment_ptr), segment_ptr.s.pool, DONT_WRITEBACK(CVMX_FPA_PACKET_POOL_SIZE/128));
		segment_ptr = next_ptr;
	}
	cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, DONT_WRITEBACK(1));

	return 0;
}


/**
 * Module/ driver initialization. Creates the linux network
 * devices.
 *
 * @return Zero on success
 */
static int cvm_oct_init_module(void)
{
	device_t dev;
	int num_interfaces;
	int interface;
	int fau = FAU_NUM_PACKET_BUFFERS_TO_FREE;
	int qos;

	dev = NULL;

	printf("cavium-ethernet: %s\n", OCTEON_SDK_VERSION_STRING);

#if 0
	cvm_oct_proc_initialize();
#endif
	cvm_oct_rx_initialize();
	cvm_oct_configure_common_hw();

	cvmx_helper_initialize_packet_io_global();

	/* Change the input group for all ports before input is enabled */
	num_interfaces = cvmx_helper_get_number_of_interfaces();
	for (interface = 0; interface < num_interfaces; interface++) {
		int num_ports = cvmx_helper_ports_on_interface(interface);
		int port;

		for (port = cvmx_helper_get_ipd_port(interface, 0); port < cvmx_helper_get_ipd_port(interface, num_ports); port++) {
			cvmx_pip_prt_tagx_t pip_prt_tagx;
			pip_prt_tagx.u64 = cvmx_read_csr(CVMX_PIP_PRT_TAGX(port));
			pip_prt_tagx.s.grp = pow_receive_group;
			cvmx_write_csr(CVMX_PIP_PRT_TAGX(port), pip_prt_tagx.u64);
		}
	}

	cvmx_helper_ipd_and_packet_input_enable();

	memset(cvm_oct_device, 0, sizeof(cvm_oct_device));

	/* Initialize the FAU used for counting packet buffers that need to be freed */
	cvmx_fau_atomic_write32(FAU_NUM_PACKET_BUFFERS_TO_FREE, 0);

	if ((pow_send_group != -1)) {
		struct ifnet *ifp;

		printf("\tConfiguring device for POW only access\n");
		ifp = if_alloc(IFT_ETHER);
		if (ifp) {
			/* Initialize the device private structure. */
			cvm_oct_private_t *priv;
			
			priv = malloc(sizeof(cvm_oct_private_t), M_TEMP, M_WAITOK | M_ZERO);
			priv->init = cvm_oct_common_init;
			priv->imode = CVMX_HELPER_INTERFACE_MODE_DISABLED;
			priv->port = CVMX_PIP_NUM_INPUT_PORTS;
			priv->queue = -1;
			device_set_desc(dev, "Cavium Octeon POW Ethernet");
#if 0
			for (qos = 0; qos < 16; qos++)
				m_queue_head_init(&priv->tx_free_list[qos]);
#endif

			ifp->if_softc = priv;

			if (priv->init(ifp) < 0) {
				printf("\t\tFailed to register ethernet device for POW\n");
				panic("%s: need to free ifp.", __func__);
			} else {
				cvm_oct_device[CVMX_PIP_NUM_INPUT_PORTS] = ifp;
				printf("\t\t%s: POW send group %d, receive group %d\n",
				if_name(ifp), pow_send_group, pow_receive_group);
			}
		} else {
			printf("\t\tFailed to allocate ethernet device for POW\n");
		}
	}

	num_interfaces = cvmx_helper_get_number_of_interfaces();
	for (interface = 0; interface < num_interfaces; interface++) {
		cvmx_helper_interface_mode_t imode = cvmx_helper_interface_get_mode(interface);
		int num_ports = cvmx_helper_ports_on_interface(interface);
		int port;

		for (port = cvmx_helper_get_ipd_port(interface, 0); port < cvmx_helper_get_ipd_port(interface, num_ports); port++) {
			cvm_oct_private_t *priv;
			struct ifnet *ifp;
			
			ifp = if_alloc(IFT_ETHER);
			if (!ifp) {
				printf("\t\tFailed to allocate ethernet device for port %d\n", port);
				continue;
			}
			/* XXX/juli set max send q len.  */
#if 0
			if (disable_core_queueing)
				ifp->tx_queue_len = 0;
#endif

			/* Initialize the device private structure. */
			priv = malloc(sizeof(cvm_oct_private_t), M_TEMP, M_WAITOK | M_ZERO);
			priv->imode = imode;
			priv->port = port;
			priv->queue = cvmx_pko_get_base_queue(priv->port);
			priv->intercept_cb = NULL;
			priv->fau = fau - cvmx_pko_get_num_queues(port) * 4;
#if 0
			for (qos = 0; qos < 16; qos++)
				m_queue_head_init(&priv->tx_free_list[qos]);
#endif
			for (qos = 0; qos < cvmx_pko_get_num_queues(port); qos++)
				cvmx_fau_atomic_write32(priv->fau+qos*4, 0);

			switch (priv->imode) {

			/* These types don't support ports to IPD/PKO */
			case CVMX_HELPER_INTERFACE_MODE_DISABLED:
			case CVMX_HELPER_INTERFACE_MODE_PCIE:
			case CVMX_HELPER_INTERFACE_MODE_PICMG:
				break;

			case CVMX_HELPER_INTERFACE_MODE_NPI:
				priv->init = cvm_oct_common_init;
				priv->uninit = cvm_oct_common_uninit;
				device_set_desc(dev, "Cavium Octeon NPI Ethernet");
				break;

			case CVMX_HELPER_INTERFACE_MODE_XAUI:
				priv->init = cvm_oct_xaui_init;
				priv->uninit = cvm_oct_xaui_uninit;
				device_set_desc(dev, "Cavium Octeon XAUI Ethernet");
				break;

			case CVMX_HELPER_INTERFACE_MODE_LOOP:
				priv->init = cvm_oct_common_init;
				priv->uninit = cvm_oct_common_uninit;
				device_set_desc(dev, "Cavium Octeon LOOP Ethernet");
				break;

			case CVMX_HELPER_INTERFACE_MODE_SGMII:
				priv->init = cvm_oct_sgmii_init;
				priv->uninit = cvm_oct_sgmii_uninit;
				device_set_desc(dev, "Cavium Octeon SGMII Ethernet");
				break;

			case CVMX_HELPER_INTERFACE_MODE_SPI:
				priv->init = cvm_oct_spi_init;
				priv->uninit = cvm_oct_spi_uninit;
				device_set_desc(dev, "Cavium Octeon SPI Ethernet");
				break;

			case CVMX_HELPER_INTERFACE_MODE_RGMII:
				priv->init = cvm_oct_rgmii_init;
				priv->uninit = cvm_oct_rgmii_uninit;
				device_set_desc(dev, "Cavium Octeon RGMII Ethernet");
				break;

			case CVMX_HELPER_INTERFACE_MODE_GMII:
				priv->init = cvm_oct_rgmii_init;
				priv->uninit = cvm_oct_rgmii_uninit;
				device_set_desc(dev, "Cavium Octeon GMII Ethernet");
				break;
			}

			ifp->if_softc = priv;

			if (!priv->init) {
				panic("%s: unsupported device type, need to free ifp.", __func__);
			} else
			if (priv->init(ifp) < 0) {
				printf("\t\tFailed to register ethernet device for interface %d, port %d\n",
				interface, priv->port);
				panic("%s: init failed, need to free ifp.", __func__);
			} else {
				cvm_oct_device[priv->port] = ifp;
				fau -= cvmx_pko_get_num_queues(priv->port) * sizeof(uint32_t);
			}
		}
	}

	if (INTERRUPT_LIMIT) {
		/* Set the POW timer rate to give an interrupt at most INTERRUPT_LIMIT times per second */
		cvmx_write_csr(CVMX_POW_WQ_INT_PC, octeon_bootinfo->eclock_hz/(INTERRUPT_LIMIT*16*256)<<8);

		/* Enable POW timer interrupt. It will count when there are packets available */
		cvmx_write_csr(CVMX_POW_WQ_INT_THRX(pow_receive_group), 0x1ful<<24);
	} else {
		/* Enable POW interrupt when our port has at least one packet */
		cvmx_write_csr(CVMX_POW_WQ_INT_THRX(pow_receive_group), 0x1001);
	}

#if 0
	/* Enable the poll timer for checking RGMII status */
	init_timer(&cvm_oct_poll_timer);
	cvm_oct_poll_timer.data = 0;
	cvm_oct_poll_timer.function = cvm_do_timer;
	mod_timer(&cvm_oct_poll_timer, jiffies + HZ);
#endif

	return 0;
}


/**
 * Module / driver shutdown
 *
 * @return Zero on success
 */
static void cvm_oct_cleanup_module(void)
{
	int port;

	/* Disable POW interrupt */
	cvmx_write_csr(CVMX_POW_WQ_INT_THRX(pow_receive_group), 0);

	cvmx_ipd_disable();

#if 0
	/* Free the interrupt handler */
	free_irq(8 + pow_receive_group, cvm_oct_device);
#endif

#if 0
	del_timer(&cvm_oct_poll_timer);
#endif
	cvm_oct_rx_shutdown();
	cvmx_pko_disable();

	/* Free the ethernet devices */
	for (port = 0; port < TOTAL_NUMBER_OF_PORTS; port++) {
		if (cvm_oct_device[port]) {
			cvm_oct_tx_shutdown(cvm_oct_device[port]);
#if 0
			unregister_netdev(cvm_oct_device[port]);
			kfree(cvm_oct_device[port]);
#else
			panic("%s: need to detach and free interface.", __func__);
#endif
			cvm_oct_device[port] = NULL;
		}
	}

	cvmx_pko_shutdown();
#if 0
	cvm_oct_proc_shutdown();
#endif

	cvmx_ipd_free_ptr();

	/* Free the HW pools */
	cvm_oct_mem_empty_fpa(CVMX_FPA_PACKET_POOL, CVMX_FPA_PACKET_POOL_SIZE, num_packet_buffers);
	cvm_oct_mem_empty_fpa(CVMX_FPA_WQE_POOL, CVMX_FPA_WQE_POOL_SIZE, num_packet_buffers);
	if (CVMX_FPA_OUTPUT_BUFFER_POOL != CVMX_FPA_PACKET_POOL)
		cvm_oct_mem_empty_fpa(CVMX_FPA_OUTPUT_BUFFER_POOL, CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE, 128);
}

static int
cvm_oct_modevent(module_t mod __unused, int type, void *data __unused)
{
	switch(type) {
	case MOD_LOAD:
		cvm_oct_init_module();
		break;
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		cvm_oct_cleanup_module();
		break;
	default:
		return (EOPNOTSUPP);
        }
	return (0);
}
DEV_MODULE(octe, cvm_oct_modevent, NULL);
MODULE_VERSION(octe, 1);
