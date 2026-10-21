// SPDX-License-Identifier: GPL-2.0
#include <linux/uaccess.h>

/* out-of-line parts */

#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long res = n;
	might_fault();
	if (likely(access_ok(VERIFY_READ, from, n))) {
		kasan_check_write(to, n);
		res = raw_copy_from_user(to, from, n);
	}
	if (unlikely(res))
		memset(to + (n - res), 0, res);
	return res;
}
EXPORT_SYMBOL(_copy_from_user);
#endif

#ifndef INLINE_COPY_TO_USER
unsigned long _copy_to_user(void __user *to, const void *from, unsigned long n)
{
	might_fault();
	if (likely(access_ok(VERIFY_WRITE, to, n))) {
		kasan_check_read(from, n);
		n = raw_copy_to_user(to, from, n);
	}
	return n;
}
EXPORT_SYMBOL(_copy_to_user);
#endif

/* 4.14 compat: simple byte-wise check_zeroed_user for BPF uattr tail check.
 * (5.x uses user_access_begin batching which 4.14 arm64 lacks.) */
int check_zeroed_user(const void __user *from, size_t size)
{
	unsigned char v;
	size_t i;

	for (i = 0; i < size; i++) {
		if (get_user(v, (unsigned char __user *)(from + i)))
			return -EFAULT;
		if (v)
			return 0;
	}
	return 1;
}
EXPORT_SYMBOL(check_zeroed_user);
