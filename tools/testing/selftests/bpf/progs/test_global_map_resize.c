// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Meta Platforms, Inc. and affiliates. */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char _license[] SEC("license") = "GPL";

/* rodata section */
const volatile pid_t pid;
const volatile size_t bss_array_len;
const volatile size_t data_array_len;

/* bss section */
int sum = 0;
int array[1];

/* custom data section */
int my_array[1] SEC(".data.custom");

/* custom data section which should NOT be resizable,
 * since it contains a single var which is not an array
 */
int my_int SEC(".data.non_array");

/* custom data section which should NOT be resizable,
 * since its last var is not an array
 */
int my_array_first[1] SEC(".data.array_not_last");
int my_int_last SEC(".data.array_not_last");

int percpu_arr[1] SEC(".data.percpu_arr");

/* at least one extern is included, to ensure that a specific
 * regression is tested whereby resizing resulted in a free-after-use
 * bug after type information is invalidated by the resize operation.
 *
 * There isn't a particularly good API to test for this specific condition,
 * but by having externs for the resizing tests it will cover this path.
 */
extern int LINUX_KERNEL_VERSION __kconfig;
long version_sink;

SEC("tp/syscalls/sys_enter_getpid")
int bss_array_sum(void *ctx)
{
	if (pid != (bpf_get_current_pid_tgid() >> 32))
		return 0;

	/* this will be zero, we just rely on verifier not rejecting this */
	sum = percpu_arr[bpf_get_smp_processor_id()];

	for (size_t i = 0; i < bss_array_len; ++i)
		sum += array[i];

	/* see above; ensure this is not optimized out */
	version_sink = LINUX_KERNEL_VERSION;

	return 0;
}

SEC("tp/syscalls/sys_enter_getuid")
int data_array_sum(void *ctx)
{
	if (pid != (bpf_get_current_pid_tgid() >> 32))
		return 0;

	/* this will be zero, we just rely on verifier not rejecting this */
	sum = percpu_arr[bpf_get_smp_processor_id()];

	for (size_t i = 0; i < data_array_len; ++i)
		sum += my_array[i];

	/* see above; ensure this is not optimized out */
	version_sink = LINUX_KERNEL_VERSION;

	return 0;
}

SEC("struct_ops/test_1")
int BPF_PROG(test_1)
{
	return 0;
}

struct bpf_testmod_ops {
	int (*test_1)(void);
};

SEC(".struct_ops.link")
struct bpf_testmod_ops st_ops_resize = {
	.test_1 = (void *)test_1
};
