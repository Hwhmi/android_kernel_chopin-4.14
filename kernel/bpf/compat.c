// SPDX-License-Identifier: GPL-2.0
/*
 * 4.14 compat shims for the 5.10 BPF subsystem backport.
 *
 * include/linux/bpf_types.h now enumerates the full 5.10 set of program,
 * map and struct_ops types, but a 4.14 kernel provides none of:
 *   - BPF trampoline / BTF-based tracing attach,
 *   - page_pool / xdp memory-model registry (net/core/xdp.c),
 *   - bpf_sk_storage (net/core/bpf_sk_storage.c),
 *   - TCP congestion-control struct_ops (net/ipv4/bpf_tcp_ca.c),
 *   - net/bpf/test_run.c.
 *
 * None of these types are used by the Android 15 bpfloader / netd path,
 * which only loads cgroup_skb, socket_filter and cgroup_sock* programs and
 * hash / array / perf-event-array maps.  The 5.10 type tables in syscall.c
 * and verifier.c are NULL-checked (find_prog_type() / map_create()), so the
 * goal here is simply to make every declared type resolve to a real object
 * and to FAIL CLOSED (-EOPNOTSUPP / -EINVAL) instead of crashing on a NULL
 * function pointer if such a type is ever requested.
 *
 * This is a deliberate functional downgrade, NOT a real backport of these
 * features.  Keep CONFIG_BPF_LSM / CONFIG_BPF_PRELOAD disabled.
 */

#include <linux/bpf.h>
#include <linux/bpf-netns.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>
#include <net/flow_dissector.h>
#include <net/net_namespace.h>
#include <net/xdp.h>

/* ------------------------------------------------------------------ */
/* 1. raw_tracepoint / raw_tracepoint_writable / tracing programs       */
/* ------------------------------------------------------------------ */

static const struct bpf_func_proto *
compat_no_func_proto(enum bpf_func_id func_id, const struct bpf_prog *prog)
{
	(void)func_id;
	(void)prog;
	return NULL;			/* verifier: unknown helper -> -EINVAL */
}

static bool compat_no_valid_access(int off, int size,
				   enum bpf_access_type type,
				   const struct bpf_prog *prog,
				   struct bpf_insn_access_aux *info)
{
	(void)off; (void)size; (void)type; (void)prog; (void)info;
	return false;			/* verifier: reject all ctx access */
}

static int compat_prog_test_run(struct bpf_prog *prog,
				const union bpf_attr *kattr,
				union bpf_attr __user *uattr)
{
	(void)prog; (void)kattr; (void)uattr;
	return -EOPNOTSUPP;
}

const struct bpf_prog_ops raw_tracepoint_prog_ops = {
	.test_run = compat_prog_test_run,
};
const struct bpf_prog_ops raw_tracepoint_writable_prog_ops = {
	.test_run = compat_prog_test_run,
};
const struct bpf_prog_ops tracing_prog_ops = {
	.test_run = compat_prog_test_run,
};

const struct bpf_verifier_ops raw_tracepoint_verifier_ops = {
	.get_func_proto  = compat_no_func_proto,
	.is_valid_access = compat_no_valid_access,
};
const struct bpf_verifier_ops raw_tracepoint_writable_verifier_ops = {
	.get_func_proto  = compat_no_func_proto,
	.is_valid_access = compat_no_valid_access,
};
const struct bpf_verifier_ops tracing_verifier_ops = {
	.get_func_proto  = compat_no_func_proto,
	.is_valid_access = compat_no_valid_access,
};

/* ------------------------------------------------------------------ */
/* 2. FLOW_DISSECTOR BPF_PROG_TEST_RUN (no net/bpf/test_run.c on 4.14) */
/* ------------------------------------------------------------------ */

int bpf_prog_test_run_flow_dissector(struct bpf_prog *prog,
				     const union bpf_attr *kattr,
				     union bpf_attr __user *uattr)
{
	(void)prog; (void)kattr; (void)uattr;
	return -EOPNOTSUPP;
}

/* BPF_MAP_TYPE_SK_STORAGE: real implementation in net/core/bpf_sk_storage.c */

/* ------------------------------------------------------------------ */
/* 4. tcp_congestion_ops struct_ops (no net/ipv4/bpf_tcp_ca.c)         */
/* ------------------------------------------------------------------ */
/* bpf_struct_ops_init() walks this array and only needs a valid ->name;
 * the BTF value type lookup then fails and the entry is skipped. */
struct bpf_struct_ops bpf_tcp_congestion_ops = {
	.name = "tcp_congestion_ops",
};

/* ------------------------------------------------------------------ */
/* 5. flow_dissector attach guard (real 5.10 logic, deps all present)  */
/* ------------------------------------------------------------------ */

int flow_dissector_bpf_prog_attach_check(struct net *net,
					 struct bpf_prog *prog)
{
	enum netns_bpf_attach_type type = NETNS_BPF_FLOW_DISSECTOR;

	(void)prog;

	if (net == &init_net) {
		/* A root-namespace flow dissector would override every
		 * per-net one, so refuse if any non-root ns has one. */
		struct net *ns;

		for_each_net(ns) {
			if (ns == &init_net)
				continue;
			if (rcu_access_pointer(ns->bpf.run_array[type]))
				return -EEXIST;
		}
	} else {
		if (rcu_access_pointer(init_net.bpf.run_array[type]))
			return -EEXIST;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* 6. XDP frame-return helpers (no page_pool / mem registry on 4.14)  */
/* ------------------------------------------------------------------ */

void xdp_warn(const char *msg, const char *func, const int line)
{
	WARN(1, "XDP_WARN: %s(line:%d): %s\n", func, line, msg);
}
EXPORT_SYMBOL_GPL(xdp_warn);

/* 4.14 has no page_pool, no rhashtable-backed xdp_mem_allocator registry
 * and no zero-copy allocator.  XDP redirect / AF_XDP is not used on this
 * platform; fall back to the plain page-refcount release so pages are not
 * leaked and the symbols resolve. */
static void compat_xdp_return(void *data, u32 mem_type)
{
	struct page *page;

	if (!data)
		return;

	if (mem_type == MEM_TYPE_PAGE_SHARED) {
		page_frag_free(data);
		return;
	}

	/* MEM_TYPE_PAGE_ORDER0, MEM_TYPE_PAGE_POOL, MEM_TYPE_ZERO_COPY:
	 * no 5.x recycling infra here; dropping the page ref is the safest
	 * common denominator. */
	page = virt_to_page(data);
	put_page(page);
}

void xdp_return_frame(struct xdp_frame *xdpf)
{
	compat_xdp_return(xdpf->data, xdpf->mem.type);
}
EXPORT_SYMBOL_GPL(xdp_return_frame);

void xdp_return_frame_rx_napi(struct xdp_frame *xdpf)
{
	compat_xdp_return(xdpf->data, xdpf->mem.type);
}
EXPORT_SYMBOL_GPL(xdp_return_frame_rx_napi);

void xdp_return_buff(struct xdp_buff *xdp)
{
	compat_xdp_return(xdp->data,
			  xdp->rxq ? xdp->rxq->mem.type : MEM_TYPE_PAGE_ORDER0);
}
EXPORT_SYMBOL_GPL(xdp_return_buff);

void __xdp_release_frame(void *data, struct xdp_mem_info *mem)
{
	/* Only reached for MEM_TYPE_PAGE_POOL, unavailable on 4.14. */
	(void)data; (void)mem;
}
EXPORT_SYMBOL_GPL(__xdp_release_frame);
