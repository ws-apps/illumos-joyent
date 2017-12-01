/*
 * COPYRIGHT 2015 Pluribus Networks Inc.
 *
 * All rights reserved. This copyright notice is Copyright Management
 * Information under 17 USC 1202 and is included to protect this work and
 * deter copyright infringement.  Removal or alteration of this Copyright
 * Management Information without the express written permission from
 * Pluribus Networks Inc is prohibited, and any such unauthorized removal
 * or alteration will be a violation of federal law.
 */
/*
 * Copyright (c) 2013  Chris Torek <torek @ torek net>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/sysmacros.h>
#include <sys/strsubr.h>
#include <sys/strsun.h>
#include <vm/seg_kmem.h>

#include <sys/dls.h>
#include <sys/mac_client.h>

#include <sys/vmm_drv.h>
#include <sys/viona_io.h>

/*
 * Min. octets in an ethernet frame minus FCS
 */
#define	MIN_BUF_SIZE	60

#define	VIONA_NAME		"Virtio Network Accelerator"

#define	VIONA_CTL_MINOR		0
#define	VIONA_CTL_NODE_NAME	"ctl"

#define	VIONA_CLI_NAME		"viona"

#define	VTNET_MAXSEGS		32

#define	VRING_ALIGN		4096
#define	VRING_MAX_LEN		32768

#define	VRING_DESC_F_NEXT	(1 << 0)
#define	VRING_DESC_F_WRITE	(1 << 1)
#define	VRING_DESC_F_INDIRECT	(1 << 2)

#define	VRING_AVAIL_F_NO_INTERRUPT	1

#define	VRING_USED_F_NO_NOTIFY		1

#define	BCM_NIC_DRIVER		"bnxe"
/*
 * Host capabilities
 */
#define	VIRTIO_NET_F_MAC	(1 <<  5) /* host supplies MAC */
#define	VIRTIO_NET_F_MRG_RXBUF	(1 << 15) /* host can merge RX buffers */
#define	VIRTIO_NET_F_STATUS	(1 << 16) /* config status field available */
#define	VIRTIO_F_RING_NOTIFY_ON_EMPTY	(1 << 24)
#define	VIRTIO_F_RING_INDIRECT_DESC	(1 << 28)
#define	VIRTIO_F_RING_EVENT_IDX		(1 << 29)

#define	VIONA_S_HOSTCAPS					\
	(VIRTIO_NET_F_MAC | VIRTIO_NET_F_MRG_RXBUF |		\
	VIRTIO_NET_F_STATUS |					\
	VIRTIO_F_RING_NOTIFY_ON_EMPTY | VIRTIO_F_RING_INDIRECT_DESC)

#define	VIONA_PROBE(name)	DTRACE_PROBE(viona__##name)
#define	VIONA_PROBE1(name, arg1, arg2)	\
	DTRACE_PROBE1(viona__##name, arg1, arg2)
#define	VIONA_PROBE2(name, arg1, arg2, arg3, arg4)	\
	DTRACE_PROBE2(viona__##name, arg1, arg2, arg3, arg4)
#define	VIONA_PROBE3(name, arg1, arg2, arg3, arg4, arg5, arg6)	\
	DTRACE_PROBE3(viona__##name, arg1, arg2, arg3, arg4, arg5, arg6)
#define	VIONA_PROBE_BAD_RING_ADDR(r, a)		\
	VIONA_PROBE2(bad_ring_addr, viona_vring_t *, r, void *, (void *)(a))

#pragma pack(1)
struct virtio_desc {
	uint64_t	vd_addr;
	uint32_t	vd_len;
	uint16_t	vd_flags;
	uint16_t	vd_next;
};
#pragma pack()

#pragma pack(1)
struct virtio_used {
	uint32_t	vu_idx;
	uint32_t	vu_tlen;
};
#pragma pack()

#pragma pack(1)
struct virtio_net_mrgrxhdr {
	uint8_t		vrh_flags;
	uint8_t		vrh_gso_type;
	uint16_t	vrh_hdr_len;
	uint16_t	vrh_gso_size;
	uint16_t	vrh_csum_start;
	uint16_t	vrh_csum_offset;
	uint16_t	vrh_bufs;
};
struct virtio_net_hdr {
	uint8_t		vrh_flags;
	uint8_t		vrh_gso_type;
	uint16_t	vrh_hdr_len;
	uint16_t	vrh_gso_size;
	uint16_t	vrh_csum_start;
	uint16_t	vrh_csum_offset;
};
#pragma pack()

struct viona_link;
typedef struct viona_link viona_link_t;

typedef struct viona_vring {
	/* Internal state */
	kmutex_t		vr_a_mutex;
	kmutex_t		vr_u_mutex;
	viona_link_t		*vr_link;

	uint16_t		vr_size;
	uint16_t		vr_mask;	/* cached from vr_size */
	uint16_t		vr_cur_aidx;	/* trails behind 'avail_idx' */

	/* Host-context pointers to the queue */
	volatile struct virtio_desc	*vr_descr;

	volatile uint16_t		*vr_avail_flags;
	volatile uint16_t		*vr_avail_idx;
	volatile uint16_t		*vr_avail_ring;
	volatile uint16_t		*vr_avail_used_event;

	volatile uint16_t		*vr_used_flags;
	volatile uint16_t		*vr_used_idx;
	volatile struct virtio_used	*vr_used_ring;
	volatile uint16_t		*vr_used_avail_event;
} viona_vring_t;

struct viona_link {
	datalink_id_t		l_linkid;

	vmm_hold_t		*l_vm_hold;

	mac_handle_t		l_mh;
	mac_client_handle_t	l_mch;

	pollhead_t		l_pollhead;

	viona_vring_t		l_rx_vring;
	uint_t			l_rx_intr;

	viona_vring_t		l_tx_vring;
	kcondvar_t		l_tx_cv;
	uint_t			l_tx_intr;
	kmutex_t		l_tx_mutex;
	int			l_tx_outstanding;
	uint32_t		l_features;
};

typedef struct {
	frtn_t			d_frtn;
	viona_link_t		*d_link;
	uint_t			d_ref;
	uint16_t		d_cookie;
	int			d_len;
} viona_desb_t;

typedef struct viona_soft_state {
	viona_link_t		*ss_link;
} viona_soft_state_t;

typedef struct used_elem {
	uint16_t	id;
	uint32_t	len;
} used_elem_t;


static void			*viona_state;
static dev_info_t		*viona_dip;
static id_space_t		*viona_minors;
static kmem_cache_t		*viona_desb_cache;
/*
 * copy tx mbufs from virtio ring to avoid necessitating a wait for packet
 * transmission to free resources.
 */
static boolean_t		copy_tx_mblks = B_TRUE;

extern struct vm *vm_lookup_by_name(char *name);
extern uint64_t vm_gpa2hpa(struct vm *vm, uint64_t gpa, size_t len);

static int viona_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int viona_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int viona_open(dev_t *devp, int flag, int otype, cred_t *credp);
static int viona_close(dev_t dev, int flag, int otype, cred_t *credp);
static int viona_ioctl(dev_t dev, int cmd, intptr_t data, int mode,
    cred_t *credp, int *rval);
static int viona_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp);

static int viona_ioc_create(viona_soft_state_t *, void *, int, cred_t *);
static int viona_ioc_delete(viona_soft_state_t *ss);

static void *viona_gpa2kva(viona_link_t *link, uint64_t gpa, size_t len);

static int viona_ioc_ring_init(viona_link_t *, viona_vring_t *, void *, int);
static int viona_ioc_rx_ring_reset(viona_link_t *link);
static int viona_ioc_tx_ring_reset(viona_link_t *link);
static void viona_ioc_rx_ring_kick(viona_link_t *link);
static void viona_ioc_tx_ring_kick(viona_link_t *link);
static int viona_ioc_rx_intr_clear(viona_link_t *link);
static int viona_ioc_tx_intr_clear(viona_link_t *link);

static void viona_intr_rx(viona_link_t *);
static void viona_intr_tx(viona_link_t *);

static void viona_rx(void *, mac_resource_handle_t, mblk_t *, boolean_t);
static void viona_tx(viona_link_t *, viona_vring_t *);

static struct cb_ops viona_cb_ops = {
	viona_open,
	viona_close,
	nodev,
	nodev,
	nodev,
	nodev,
	nodev,
	viona_ioctl,
	nodev,
	nodev,
	nodev,
	viona_chpoll,
	ddi_prop_op,
	0,
	D_MP | D_NEW | D_HOTPLUG,
	CB_REV,
	nodev,
	nodev
};

static struct dev_ops viona_ops = {
	DEVO_REV,
	0,
	nodev,
	nulldev,
	nulldev,
	viona_attach,
	viona_detach,
	nodev,
	&viona_cb_ops,
	NULL,
	ddi_power,
	ddi_quiesce_not_needed
};

static struct modldrv modldrv = {
	&mod_driverops,
	VIONA_NAME,
	&viona_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

int
_init(void)
{
	int	ret;

	ret = ddi_soft_state_init(&viona_state,
	    sizeof (viona_soft_state_t), 0);
	if (ret == 0) {
		ret = mod_install(&modlinkage);
		if (ret != 0) {
			ddi_soft_state_fini(&viona_state);
			return (ret);
		}
	}

	return (ret);
}

int
_fini(void)
{
	int	ret;

	ret = mod_remove(&modlinkage);
	if (ret == 0) {
		ddi_soft_state_fini(&viona_state);
	}

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static void
set_viona_tx_mode()
{
	major_t bcm_nic_major;

	if ((bcm_nic_major = ddi_name_to_major(BCM_NIC_DRIVER))
	    != DDI_MAJOR_T_NONE) {
		if (ddi_hold_installed_driver(bcm_nic_major) != NULL) {
			copy_tx_mblks = B_FALSE;
			ddi_rele_driver(bcm_nic_major);
		}
	}
}

static int
viona_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	viona_minors = id_space_create("viona_minors",
	    VIONA_CTL_MINOR + 1, UINT16_MAX);

	if (ddi_create_minor_node(dip, VIONA_CTL_NODE_NAME,
	    S_IFCHR, VIONA_CTL_MINOR, DDI_PSEUDO, 0) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	viona_desb_cache = kmem_cache_create("viona_desb_cache",
	    sizeof (viona_desb_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	viona_dip = dip;

	set_viona_tx_mode();
	ddi_report_dev(viona_dip);

	return (DDI_SUCCESS);
}

static int
viona_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	kmem_cache_destroy(viona_desb_cache);

	id_space_destroy(viona_minors);

	ddi_remove_minor_node(viona_dip, NULL);

	viona_dip = NULL;

	return (DDI_SUCCESS);
}

static int
viona_open(dev_t *devp, int flag, int otype, cred_t *credp)
{
	int	minor;

	if (otype != OTYP_CHR) {
		return (EINVAL);
	}
	if (drv_priv(credp) != 0) {
		return (EPERM);
	}
	if (getminor(*devp) != VIONA_CTL_MINOR) {
		return (ENXIO);
	}

	minor = id_alloc_nosleep(viona_minors);
	if (minor == 0) {
		/* All minors are busy */
		return (EBUSY);
	}
	if (ddi_soft_state_zalloc(viona_state, minor) != DDI_SUCCESS) {
		id_free(viona_minors, minor);
		return (ENOMEM);
	}

	*devp = makedevice(getmajor(*devp), minor);

	return (0);
}

static int
viona_close(dev_t dev, int flag, int otype, cred_t *credp)
{
	int			minor;
	viona_soft_state_t	*ss;

	if (otype != OTYP_CHR) {
		return (EINVAL);
	}

	if (drv_priv(credp) != 0) {
		return (EPERM);
	}

	minor = getminor(dev);

	ss = ddi_get_soft_state(viona_state, minor);
	if (ss == NULL) {
		return (ENXIO);
	}

	viona_ioc_delete(ss);
	ddi_soft_state_free(viona_state, minor);
	id_free(viona_minors, minor);

	return (0);
}

static int
viona_ioctl(dev_t dev, int cmd, intptr_t data, int md, cred_t *cr, int *rv)
{
	viona_soft_state_t *ss;
	void *dptr = (void *)data;
	int err = 0, val;
	viona_link_t *link;

	ss = ddi_get_soft_state(viona_state, getminor(dev));
	if (ss == NULL) {
		return (ENXIO);
	}

	switch (cmd) {
	case VNA_IOC_CREATE:
		return (viona_ioc_create(ss, dptr, md, cr));
	case VNA_IOC_DELETE:
		return (viona_ioc_delete(ss));
	default:
		break;
	}

	if ((link = ss->ss_link) == NULL || vmm_drv_expired(link->l_vm_hold)) {
		return (ENXIO);
	}
	switch (cmd) {
	case VNA_IOC_SET_FEATURES:
		if (ddi_copyin(dptr, &val, sizeof (val), md) != 0) {
			return (EFAULT);
		}
		link->l_features = val & VIONA_S_HOSTCAPS;
		break;
	case VNA_IOC_GET_FEATURES:
		val = VIONA_S_HOSTCAPS;
		if (ddi_copyout(&val, dptr, sizeof (val), md) != 0) {
			return (EFAULT);
		}
		break;
	case VNA_IOC_RX_RING_INIT:
		err = viona_ioc_ring_init(link, &link->l_rx_vring, dptr, md);
		break;
	case VNA_IOC_TX_RING_INIT:
		err = viona_ioc_ring_init(link, &link->l_tx_vring, dptr, md);
		break;
	case VNA_IOC_RX_RING_RESET:
		err = viona_ioc_rx_ring_reset(link);
		break;
	case VNA_IOC_TX_RING_RESET:
		err = viona_ioc_tx_ring_reset(link);
		break;
	case VNA_IOC_RX_RING_KICK:
		viona_ioc_rx_ring_kick(link);
		break;
	case VNA_IOC_TX_RING_KICK:
		viona_ioc_tx_ring_kick(link);
		break;
	case VNA_IOC_RX_INTR_CLR:
		err = viona_ioc_rx_intr_clear(link);
		break;
	case VNA_IOC_TX_INTR_CLR:
		err = viona_ioc_tx_intr_clear(link);
		break;
	default:
		err = ENOTTY;
		break;
	}

	return (err);
}

static int
viona_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	viona_soft_state_t	*ss;

	ss = ddi_get_soft_state(viona_state, getminor(dev));
	if (ss == NULL || ss->ss_link == NULL) {
		return (ENXIO);
	}

	*reventsp = 0;

	if (ss->ss_link->l_rx_intr && (events & POLLIN)) {
		*reventsp |= POLLIN;
	}

	if (ss->ss_link->l_tx_intr && (events & POLLOUT)) {
		*reventsp |= POLLOUT;
	}

	if (*reventsp == 0 && !anyyet) {
		*phpp = &ss->ss_link->l_pollhead;
	}

	return (0);
}

static int
viona_ioc_create(viona_soft_state_t *ss, void *dptr, int md, cred_t *cr)
{
	vioc_create_t	kvc;
	viona_link_t	*link;
	char		cli_name[MAXNAMELEN];
	int		err;
	file_t		*fp;
	vmm_hold_t	*hold = NULL;

	if (ss->ss_link != NULL) {
		return (EEXIST);
	}
	if (ddi_copyin(dptr, &kvc, sizeof (kvc), md) != 0) {
		return (EFAULT);
	}

	if ((fp = getf(kvc.c_vmfd)) == NULL) {
		return (EBADF);
	}
	err = vmm_drv_hold(fp, cr, &hold);
	releasef(kvc.c_vmfd);
	if (err != NULL) {
		return (err);
	}

	link = kmem_zalloc(sizeof (viona_link_t), KM_SLEEP);
	link->l_linkid = kvc.c_linkid;
	link->l_vm_hold = hold;

	err = mac_open_by_linkid(link->l_linkid, &link->l_mh);
	if (err != 0) {
		goto bail;
	}

	(void) snprintf(cli_name, sizeof (cli_name), "%s-%d", VIONA_CLI_NAME,
	    link->l_linkid);
	err = mac_client_open(link->l_mh, &link->l_mch, cli_name, 0);
	if (err != 0) {
		goto bail;
	}

	link->l_features = VIONA_S_HOSTCAPS;

	link->l_rx_vring.vr_link = link;
	link->l_tx_vring.vr_link = link;
	mutex_init(&link->l_rx_vring.vr_a_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&link->l_rx_vring.vr_u_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&link->l_rx_vring.vr_a_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&link->l_tx_vring.vr_u_mutex, NULL, MUTEX_DRIVER, NULL);
	if (copy_tx_mblks) {
		mutex_init(&link->l_tx_mutex, NULL, MUTEX_DRIVER, NULL);
		cv_init(&link->l_tx_cv, NULL, CV_DRIVER, NULL);
	}
	ss->ss_link = link;

	return (0);

bail:
	if (link->l_mch != NULL) {
		mac_client_close(link->l_mch, 0);
	}
	if (link->l_mh != NULL) {
		mac_close(link->l_mh);
	}
	if (hold != NULL) {
		vmm_drv_rele(hold);
	}
	kmem_free(link, sizeof (viona_link_t));

	return (err);
}

static int
viona_ioc_delete(viona_soft_state_t *ss)
{
	viona_link_t	*link;

	link = ss->ss_link;
	if (link == NULL) {
		return (ENOSYS);
	}
	if (copy_tx_mblks) {
		mutex_enter(&link->l_tx_mutex);
		while (link->l_tx_outstanding != 0) {
			cv_wait(&link->l_tx_cv, &link->l_tx_mutex);
		}
		mutex_exit(&link->l_tx_mutex);
	}
	if (link->l_mch != NULL) {
		mac_rx_clear(link->l_mch);
		mac_client_close(link->l_mch, 0);
	}
	if (link->l_mh != NULL) {
		mac_close(link->l_mh);
	}
	if (link->l_vm_hold != NULL) {
		vmm_drv_rele(link->l_vm_hold);
		link->l_vm_hold = NULL;
	}

	mutex_destroy(&link->l_tx_vring.vr_a_mutex);
	mutex_destroy(&link->l_tx_vring.vr_u_mutex);
	mutex_destroy(&link->l_rx_vring.vr_a_mutex);
	mutex_destroy(&link->l_rx_vring.vr_u_mutex);
	if (copy_tx_mblks) {
		mutex_destroy(&link->l_tx_mutex);
		cv_destroy(&link->l_tx_cv);
	}

	kmem_free(link, sizeof (viona_link_t));

	ss->ss_link = NULL;

	return (0);
}

/*
 * Translate a guest physical address into a kernel virtual address.
 */
static void *
viona_gpa2kva(viona_link_t *link, uint64_t gpa, size_t len)
{
	return (vmm_drv_gpa2kva(link->l_vm_hold, gpa, len));
}

static int
viona_ioc_ring_init(viona_link_t *link, viona_vring_t *ring, void *u_ri, int md)
{
	vioc_ring_init_t	kri;
	uintptr_t		pos;
	size_t			desc_sz, avail_sz, used_sz;
	uint16_t		cnt;

	if (ddi_copyin(u_ri, &kri, sizeof (kri), md) != 0) {
		return (EFAULT);
	}

	cnt = kri.ri_qsize;
	if (cnt == 0 || cnt > VRING_MAX_LEN || (1 << (ffs(cnt) - 1)) != cnt) {
		return (EINVAL);
	}

	pos = kri.ri_qaddr;
	desc_sz = cnt * sizeof (struct virtio_desc);
	avail_sz = (cnt + 3) * sizeof (uint16_t);
	used_sz = (cnt * sizeof (struct virtio_used)) + (sizeof (uint16_t) * 3);

	ring->vr_size = kri.ri_qsize;
	ring->vr_mask = (ring->vr_size - 1);
	ring->vr_descr = viona_gpa2kva(link, pos, desc_sz);
	if (ring->vr_descr == NULL) {
		return (EINVAL);
	}
	pos += desc_sz;

	ring->vr_avail_flags = viona_gpa2kva(link, pos, avail_sz);
	if (ring->vr_avail_flags == NULL) {
		return (EINVAL);
	}
	ring->vr_avail_idx = ring->vr_avail_flags + 1;
	ring->vr_avail_ring = ring->vr_avail_flags + 2;
	ring->vr_avail_used_event = ring->vr_avail_ring + cnt;
	pos += avail_sz;

	pos = P2ROUNDUP(pos, VRING_ALIGN);
	ring->vr_used_flags = viona_gpa2kva(link, pos, used_sz);
	if (ring->vr_used_flags == NULL) {
		return (EINVAL);
	}
	ring->vr_used_idx = ring->vr_used_flags + 1;
	ring->vr_used_ring = (struct virtio_used *)(ring->vr_used_flags + 2);
	ring->vr_used_avail_event = (uint16_t *)(ring->vr_used_ring + cnt);

	/* Initialize queue indexes */
	ring->vr_cur_aidx = 0;

	return (0);
}

static int
viona_ioc_ring_reset_common(viona_vring_t *ring)
{
	/*
	 * Reset all soft state
	 */
	ring->vr_cur_aidx = 0;

	return (0);
}

static int
viona_ioc_rx_ring_reset(viona_link_t *link)
{
	viona_vring_t	*ring;

	mac_rx_clear(link->l_mch);

	ring = &link->l_rx_vring;

	return (viona_ioc_ring_reset_common(ring));
}

static int
viona_ioc_tx_ring_reset(viona_link_t *link)
{
	viona_vring_t	*ring;

	ring = &link->l_tx_vring;

	return (viona_ioc_ring_reset_common(ring));
}

static void
viona_ioc_rx_ring_kick(viona_link_t *link)
{
	viona_vring_t	*ring = &link->l_rx_vring;

	atomic_or_16(ring->vr_used_flags, VRING_USED_F_NO_NOTIFY);

	mac_rx_set(link->l_mch, viona_rx, link);
}

/*
 * Return the number of available descriptors in the vring taking care
 * of the 16-bit index wraparound.
 */
static inline int
viona_vr_num_avail(viona_vring_t *ring)
{
	uint16_t ndesc;

	/*
	 * We're just computing (a-b) in GF(216).
	 *
	 * The only glitch here is that in standard C,
	 * uint16_t promotes to (signed) int when int has
	 * more than 16 bits (pretty much always now), so
	 * we have to force it back to unsigned.
	 */
	ndesc = (unsigned)*ring->vr_avail_idx - (unsigned)ring->vr_cur_aidx;

	ASSERT(ndesc <= ring->vr_size);

	return (ndesc);
}

static void
viona_ioc_tx_ring_kick(viona_link_t *link)
{
	viona_vring_t	*ring = &link->l_tx_vring;

	do {
		atomic_or_16(ring->vr_used_flags, VRING_USED_F_NO_NOTIFY);
		while (viona_vr_num_avail(ring)) {
			viona_tx(link, ring);
		}
		if (copy_tx_mblks) {
			mutex_enter(&link->l_tx_mutex);
			if (link->l_tx_outstanding != 0) {
				cv_wait_sig(&link->l_tx_cv, &link->l_tx_mutex);
			}
			mutex_exit(&link->l_tx_mutex);
		}
		atomic_and_16(ring->vr_used_flags, ~VRING_USED_F_NO_NOTIFY);
	} while (viona_vr_num_avail(ring));

	if ((link->l_features & VIRTIO_F_RING_NOTIFY_ON_EMPTY) != 0) {
		viona_intr_tx(link);
	}
}

static int
viona_ioc_rx_intr_clear(viona_link_t *link)
{
	link->l_rx_intr = 0;

	return (0);
}

static int
viona_ioc_tx_intr_clear(viona_link_t *link)
{
	link->l_tx_intr = 0;

	return (0);
}

static int
vq_popchain(viona_vring_t *ring, struct iovec *iov, int niov, uint16_t *cookie)
{
	viona_link_t *link = ring->vr_link;
	uint_t i, ndesc, idx, head, next;
	struct virtio_desc vdir;
	void *buf;

	ASSERT(iov != NULL);
	ASSERT(niov > 0);

	mutex_enter(&ring->vr_a_mutex);
	idx = ring->vr_cur_aidx;
	ndesc = (uint16_t)((unsigned)*ring->vr_avail_idx - (unsigned)idx);

	if (ndesc == 0) {
		mutex_exit(&ring->vr_a_mutex);
		return (0);
	}
	if (ndesc > ring->vr_size) {
		VIONA_PROBE2(ndesc_too_high, viona_vring_t *, ring,
		    uint16_t, ndesc);
		mutex_exit(&ring->vr_a_mutex);
		return (-1);
	}

	head = ring->vr_avail_ring[idx & ring->vr_mask];
	next = head;

	for (i = 0; i < niov; next = vdir.vd_next) {
		if (next >= ring->vr_size) {
			VIONA_PROBE2(bad_idx, viona_vring_t *, ring,
			    uint16_t, next);
			goto bail;
		}

		vdir = ring->vr_descr[next];
		if ((vdir.vd_flags & VRING_DESC_F_INDIRECT) == 0) {
			buf = viona_gpa2kva(link, vdir.vd_addr, vdir.vd_len);
			if (buf == NULL) {
				VIONA_PROBE_BAD_RING_ADDR(ring, vdir.vd_addr);
				goto bail;
			}
			iov[i].iov_base = buf;
			iov[i].iov_len = vdir.vd_len;
			i++;
		} else {
			const uint_t nindir = vdir.vd_len / 16;
			volatile struct virtio_desc *vindir;

			if ((vdir.vd_len & 0xf) || nindir == 0) {
				VIONA_PROBE2(indir_bad_len,
				    viona_vring_t *, ring,
				    uint32_t, vdir.vd_len);
				goto bail;
			}
			vindir = viona_gpa2kva(link, vdir.vd_addr, vdir.vd_len);
			if (vindir == NULL) {
				VIONA_PROBE_BAD_RING_ADDR(ring, vdir.vd_addr);
				goto bail;
			}
			next = 0;
			for (;;) {
				struct virtio_desc vp;

				vp = vindir[next];
				if (vp.vd_flags & VRING_DESC_F_INDIRECT) {
					VIONA_PROBE1(indir_bad_nest,
					    viona_vring_t *, ring);
					goto bail;
				}
				buf = viona_gpa2kva(link, vp.vd_addr,
				    vp.vd_len);
				if (buf == NULL) {
					VIONA_PROBE_BAD_RING_ADDR(ring,
					    vp.vd_addr);
					goto bail;
				}
				iov[i].iov_base = buf;
				iov[i].iov_len = vp.vd_len;
				i++;

				if ((vp.vd_flags & VRING_DESC_F_NEXT) == 0)
					break;
				if (i >= niov) {
					goto loopy;
				}

				next = vp.vd_next;
				if (next >= nindir) {
					VIONA_PROBE3(indir_bad_next,
					    viona_vring_t *, ring,
					    uint16_t, next,
					    uint_t, nindir);
					goto bail;
				}
			}
		}
		if ((vdir.vd_flags & VRING_DESC_F_NEXT) == 0) {
			*cookie = head;
			ring->vr_cur_aidx++;
			mutex_exit(&ring->vr_a_mutex);
			return (i);
		}
	}

loopy:
	VIONA_PROBE1(too_many_desc, viona_vring_t *, ring);
bail:
	mutex_exit(&ring->vr_a_mutex);
	return (-1);
}

static void
vq_pushchain(viona_vring_t *ring, uint32_t len, uint16_t cookie)
{
	volatile struct virtio_used *vu;
	uint_t uidx;

	mutex_enter(&ring->vr_u_mutex);

	uidx = *ring->vr_used_idx;
	vu = &ring->vr_used_ring[uidx++ & ring->vr_mask];
	vu->vu_idx = cookie;
	vu->vu_tlen = len;
	membar_producer();
	*ring->vr_used_idx = uidx;

	mutex_exit(&ring->vr_u_mutex);
}

static void
vq_pushchain_mrgrx(viona_vring_t *ring, int num_bufs, used_elem_t *elem)
{
	volatile struct virtio_used *vu;
	uint_t uidx, i;

	mutex_enter(&ring->vr_u_mutex);

	uidx = *ring->vr_used_idx;
	if (num_bufs == 1) {
		vu = &ring->vr_used_ring[uidx++ & ring->vr_mask];
		vu->vu_idx = elem[0].id;
		vu->vu_tlen = elem[0].len;
	} else {
		for (i = 0; i < num_bufs; i++) {
			vu = &ring->vr_used_ring[(uidx + i) & ring->vr_mask];
			vu->vu_idx = elem[i].id;
			vu->vu_tlen = elem[i].len;
		}
		uidx = uidx + num_bufs;
	}
	membar_producer();
	*ring->vr_used_idx = uidx;

	mutex_exit(&ring->vr_u_mutex);
}

static void
viona_intr_rx(viona_link_t *link)
{
	if (atomic_cas_uint(&link->l_rx_intr, 0, 1) == 0) {
		pollwakeup(&link->l_pollhead, POLLIN);
	}
}

static void
viona_intr_tx(viona_link_t *link)
{
	if (atomic_cas_uint(&link->l_tx_intr, 0, 1) == 0) {
		pollwakeup(&link->l_pollhead, POLLOUT);
	}
}

static size_t
viona_copy_mblk(mblk_t *mp, size_t seek, caddr_t buf, size_t len,
    boolean_t *end)
{
	size_t copied = 0;
	size_t off = 0;

	/* Seek past already-consumed data */
	while (seek > 0 && mp != NULL) {
		size_t chunk = MBLKL(mp);

		if (chunk > seek) {
			off = seek;
			break;
		}
		mp = mp->b_cont;
		seek -= chunk;
	}

	while (mp != NULL) {
		const size_t chunk = MBLKL(mp) - off;
		const size_t to_copy = MIN(chunk, len);

		bcopy(mp->b_rptr + off, buf, to_copy);
		copied += to_copy;
		buf += to_copy;
		len -= to_copy;

		/* Go no further if the buffer has been filled */
		if (len == 0) {
			break;
		}

		/*
		 * Any offset into the initially chosen mblk_t buffer is
		 * consumed on the first copy operation.
		 */
		off = 0;
		mp = mp->b_cont;
	}
	*end = (mp == NULL);
	return (copied);
}

static int
viona_recv_plain(viona_vring_t *ring, mblk_t *mp, size_t msz)
{
	struct iovec iov[VTNET_MAXSEGS];
	uint16_t cookie;
	int n;
	const size_t hdr_sz = sizeof (struct virtio_net_hdr);
	struct virtio_net_hdr *hdr;
	size_t len, copied = 0;
	caddr_t buf = NULL;
	boolean_t end = B_FALSE;

	n = vq_popchain(ring, iov, VTNET_MAXSEGS, &cookie);
	if (n <= 0) {
		/* Without available buffers, the frame must be dropped. */
		return (ENOSPC);
	}
	if (iov[0].iov_len < hdr_sz) {
		/*
		 * There is little to do if there is not even space available
		 * for the sole header.  Zero the buffer and bail out as a last
		 * act of desperation.
		 */
		bzero(iov[0].iov_base, iov[0].iov_len);
		goto bad_frame;
	}

	/* Grab the address of the header before anything else */
	hdr = (struct virtio_net_hdr *)iov[0].iov_base;

	/*
	 * If there is any space remaining in the first buffer after writing
	 * the header, fill it with frame data.
	 */
	if (iov[0].iov_len > hdr_sz) {
		buf = (caddr_t)iov[0].iov_base + hdr_sz;
		len = iov[0].iov_len - hdr_sz;

		copied += viona_copy_mblk(mp, copied, buf, len, &end);
	}

	/* Copy any remaining data into subsequent buffers, if present */
	for (int i = 1; i < n && !end; i++) {
		buf = (caddr_t)iov[i].iov_base;
		len = iov[i].iov_len;

		copied += viona_copy_mblk(mp, copied, buf, len, &end);
	}

	/*
	 * Is the copied data long enough to be considered an ethernet frame of
	 * the minimum length?  Does it match the total length of the mblk?
	 */
	if (copied < MIN_BUF_SIZE || copied != msz) {
		goto bad_frame;
	}

	/* Populate (read: zero) the header and account for it in the size */
	bzero(hdr, hdr_sz);
	copied += hdr_sz;

	/* Release this chain */
	vq_pushchain(ring, copied, cookie);
	return (0);

bad_frame:
	VIONA_PROBE3(bad_rx_frame, viona_vring_t *, ring, uint16_t, cookie,
	    mblk_t *, mp);
	vq_pushchain(ring, MAX(copied, MIN_BUF_SIZE + hdr_sz), cookie);
	return (EINVAL);
}

static int
viona_recv_merged(viona_vring_t *ring, mblk_t *mp, size_t msz)
{
	struct iovec iov[VTNET_MAXSEGS];
	used_elem_t uelem[VTNET_MAXSEGS];
	int n, i = 0, buf_idx = 0, err = 0;
	uint16_t cookie;
	caddr_t buf;
	size_t len, copied = 0, chunk = 0;
	struct virtio_net_mrgrxhdr *hdr = NULL;
	const size_t hdr_sz = sizeof (struct virtio_net_mrgrxhdr);
	boolean_t end = B_FALSE;

	n = vq_popchain(ring, iov, VTNET_MAXSEGS, &cookie);
	if (n <= 0) {
		/* Without available buffers, the frame must be dropped. */
		return (ENOSPC);
	}
	if (iov[0].iov_len < hdr_sz) {
		/*
		 * There is little to do if there is not even space available
		 * for the sole header.  Zero the buffer and bail out as a last
		 * act of desperation.
		 */
		bzero(iov[0].iov_base, iov[0].iov_len);
		uelem[0].id = cookie;
		uelem[0].len = iov[0].iov_len;
		err = EINVAL;
		goto done;
	}

	/* Grab the address of the header and do initial population */
	hdr = (struct virtio_net_mrgrxhdr *)iov[0].iov_base;
	bzero(hdr, hdr_sz);
	hdr->vrh_bufs = 1;

	/*
	 * If there is any space remaining in the first buffer after writing
	 * the header, fill it with frame data.
	 */
	if (iov[0].iov_len > hdr_sz) {
		buf = iov[0].iov_base + hdr_sz;
		len = iov[0].iov_len - hdr_sz;

		chunk += viona_copy_mblk(mp, copied, buf, len, &end);
		copied += chunk;
	}
	i = 1;

	do {
		while (i < n && !end) {
			buf = iov[i].iov_base;
			len = iov[i].iov_len;

			chunk += viona_copy_mblk(mp, copied, buf, len, &end);
			copied += chunk;
			i++;
		}

		uelem[buf_idx].id = cookie;
		uelem[buf_idx].len = chunk;

		/*
		 * Try to grab another buffer from the ring if the mblk has not
		 * yet been entirely copied out.
		 */
		if (!end) {
			if (buf_idx == (VTNET_MAXSEGS - 1)) {
				/*
				 * Our arbitrary limit on the number of buffers
				 * to offer for merge has already been reached.
				 */
				err = EOVERFLOW;
				break;
			}
			n = vq_popchain(ring, iov, VTNET_MAXSEGS, &cookie);
			if (n <= 0) {
				/*
				 * Without more immediate space to perform the
				 * copying, there is little choice left but to
				 * drop the packet.
				 */
				err = EMSGSIZE;
			}
			chunk = 0;
			i = 0;
			buf_idx++;
			/*
			 * Keep the header up-to-date with the number of
			 * buffers, but never reference its value since the
			 * guest could meddle with it.
			 */
			hdr->vrh_bufs++;
		}
	} while (!end && copied < msz);

	/* Account for the header size in the first buffer */
	uelem[0].len += hdr_sz;

	/*
	 * Is the copied data long enough to be considered an ethernet frame of
	 * the minimum length?  Does it match the total length of the mblk?
	 */
	if (copied < MIN_BUF_SIZE || copied != msz) {
		/* Do not override an existing error */
		err = (err == 0) ? EINVAL : err;
	}

done:
	switch (err) {
	case 0:
		/* Success can fall right through to ring delivery */
		break;

	case EMSGSIZE:
		VIONA_PROBE3(rx_merge_underrun, viona_vring_t *, ring,
		    uint16_t, cookie, mblk_t *, mp);
		break;

	case EOVERFLOW:
		VIONA_PROBE3(rx_merge_overrun, viona_vring_t *, ring,
		    uint16_t, cookie, mblk_t *, mp);
		break;

	default:
		VIONA_PROBE3(bad_rx_frame, viona_vring_t *, ring,
		    uint16_t, cookie, mblk_t *, mp);
	}
	vq_pushchain_mrgrx(ring, buf_idx + 1, uelem);
	return (err);
}

static void
viona_rx(void *arg, mac_resource_handle_t mrh, mblk_t *mp, boolean_t loopback)
{
	viona_link_t *link = (viona_link_t *)arg;
	viona_vring_t *ring = &link->l_rx_vring;
	mblk_t *mprx = NULL, **mprx_prevp = &mprx;
	mblk_t *mpdrop = NULL, **mpdrop_prevp = &mpdrop;
	const boolean_t do_merge =
	    ((link->l_features & VIRTIO_NET_F_MRG_RXBUF) != 0);
	size_t nrx = 0, ndrop = 0;

	while (mp != NULL) {
		mblk_t *next  = mp->b_next;
		const size_t size = msgsize(mp);
		int err = 0;

		mp->b_next = NULL;
		if (do_merge) {
			err = viona_recv_merged(ring, mp, size);
		} else {
			err = viona_recv_plain(ring, mp, size);
		}

		if (err != 0) {
			*mpdrop_prevp = mp;
			mpdrop_prevp = &mp->b_next;

			/*
			 * If the available ring is empty, do not bother
			 * attempting to deliver any more frames.  Count the
			 * rest as dropped too.
			 */
			if (err == ENOSPC) {
				mp->b_next = next;
				break;
			}
		} else {
			/* Chain successful mblks to be freed later */
			*mprx_prevp = mp;
			mprx_prevp = &mp->b_next;
			nrx++;
		}
		mp = next;
	}

	if ((*ring->vr_avail_flags & VRING_AVAIL_F_NO_INTERRUPT) == 0) {
		viona_intr_rx(link);
	}

	/* Free successfully received frames */
	if (mprx != NULL) {
		freemsgchain(mprx);
	}

	/* Free dropped frames, also tallying them */
	mp = mpdrop;
	while (mp != NULL) {
		mblk_t *next = mp->b_next;

		mp->b_next = NULL;
		freemsg(mp);
		mp = next;
		ndrop++;
	}
	VIONA_PROBE3(rx, viona_link_t *, link, size_t, nrx, size_t, ndrop);
}

static void
viona_desb_free(viona_desb_t *dp)
{
	viona_link_t		*link;
	viona_vring_t	*ring;
	uint_t			ref;

	ref = atomic_dec_uint_nv(&dp->d_ref);
	if (ref != 0)
		return;

	link = dp->d_link;
	ring = &link->l_tx_vring;

	vq_pushchain(ring, dp->d_len, dp->d_cookie);

	kmem_cache_free(viona_desb_cache, dp);

	if ((*ring->vr_avail_flags & VRING_AVAIL_F_NO_INTERRUPT) == 0) {
		viona_intr_tx(link);
	}
	if (copy_tx_mblks) {
		mutex_enter(&link->l_tx_mutex);
		if (--link->l_tx_outstanding == 0) {
			cv_broadcast(&link->l_tx_cv);
		}
		mutex_exit(&link->l_tx_mutex);
	}
}

static void
viona_tx(viona_link_t *link, viona_vring_t *ring)
{
	struct iovec		iov[VTNET_MAXSEGS];
	uint16_t		cookie;
	int			i, n;
	mblk_t			*mp_head, *mp_tail, *mp;
	viona_desb_t		*dp;
	mac_client_handle_t	link_mch = link->l_mch;

	mp_head = mp_tail = NULL;

	n = vq_popchain(ring, iov, VTNET_MAXSEGS, &cookie);
	if (n <= 0) {
		VIONA_PROBE1(bad_tx, viona_vring_t *, ring);
		return;
	}

	dp = kmem_cache_alloc(viona_desb_cache, KM_SLEEP);
	dp->d_frtn.free_func = viona_desb_free;
	dp->d_frtn.free_arg = (void *)dp;
	dp->d_link = link;
	dp->d_cookie = cookie;

	dp->d_ref = 0;
	dp->d_len = iov[0].iov_len;

	for (i = 1; i < n; i++) {
		dp->d_ref++;
		dp->d_len += iov[i].iov_len;
		if (copy_tx_mblks) {
			mp = desballoc((uchar_t *)iov[i].iov_base,
			    iov[i].iov_len, BPRI_MED, &dp->d_frtn);
			ASSERT(mp);
		} else {
			mp = allocb(iov[i].iov_len, BPRI_MED);
			ASSERT(mp);
			bcopy((uchar_t *)iov[i].iov_base, mp->b_wptr,
			    iov[i].iov_len);
		}
		mp->b_wptr += iov[i].iov_len;
		if (mp_head == NULL) {
			ASSERT(mp_tail == NULL);
			mp_head = mp;
		} else {
			ASSERT(mp_tail != NULL);
			mp_tail->b_cont = mp;
		}
		mp_tail = mp;
	}
	if (copy_tx_mblks == B_FALSE) {
		viona_desb_free(dp);
	}
	if (copy_tx_mblks) {
		mutex_enter(&link->l_tx_mutex);
		link->l_tx_outstanding++;
		mutex_exit(&link->l_tx_mutex);
	}
	mac_tx(link_mch, mp_head, 0, MAC_DROP_ON_NO_DESC, NULL);
}
