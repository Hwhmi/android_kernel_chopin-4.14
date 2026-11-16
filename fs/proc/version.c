// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
#include <linux/susfs.h>
#endif

static int version_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	char fake[SUSFS_FAKE_PROC_VERSION_SIZE];
	char rel[__NEW_UTS_LEN + 1], ver[__NEW_UTS_LEN + 1];

	/* Full stock line wins: it also replaces the compile-by/host,
	 * compiler and build timestamp baked into linux_proc_banner. */
	if (susfs_get_fake_proc_version(fake, sizeof(fake))) {
		seq_printf(m, "%s\n", fake);
		return 0;
	}
	/* Otherwise at least align release/version with uname(2). */
	if (susfs_is_uname_spoof_active()) {
		susfs_get_spoofed_uname(rel, sizeof(rel), ver, sizeof(ver));
		seq_printf(m, linux_proc_banner,
			utsname()->sysname, rel, ver);
		return 0;
	}
#endif
	seq_printf(m, linux_proc_banner,
		utsname()->sysname,
		utsname()->release,
		utsname()->version);
	return 0;
}

static int version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, version_proc_show, NULL);
}

static const struct file_operations version_proc_fops = {
	.open		= version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_version_init(void)
{
	proc_create("version", 0, NULL, &version_proc_fops);
	return 0;
}
fs_initcall(proc_version_init);
