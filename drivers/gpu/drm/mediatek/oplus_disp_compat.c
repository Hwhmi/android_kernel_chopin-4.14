// SPDX-License-Identifier: GPL-2.0
/*
 * OPlus / ColorOS display sysfs compatibility for the 4.14.186 mt6891
 * "chopin" kernel.
 *
 * ColorOS talks to its display HAL through the /sys/kernel/oplus_display/
 * directory,
 * which the Xiaomi tree does not provide.  This file recreates that
 * interface on top of the existing MTK backlight path so one kernel can
 * boot both ROMs:
 *
 *   /sys/kernel/oplus_display/...          (this file, ColorOS)
 *   /sys/class/mi_display/disp_feature/    (mi_disp, Xiaomi - untouched)
 *   /sys/class/leds/lcd-backlight/          (LED class, shared by both)
 *
 * Only oplus_brightness drives real hardware.  ColorOS uses a 12-bit DBV
 * range (0..4095) while this panel is 11-bit (0..2047, see mt6893.dts
 * disp_leds: led-bits = <11>), so the value is scaled before being handed
 * to mtkfb_set_backlight_level().  Every other attribute is a read/write
 * stub that keeps its value in RAM; they exist so the vendor HAL does not
 * fail with ENOENT while probing the display feature set.
 *
 * Deliberately no dependency on any OPlus, vendor/oplus or mediatek_v2
 * header: the whole point is to coexist with the Xiaomi mi_disp stack
 * without touching a single line of it.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/printk.h>

/* mtk_debug.c, part of the same mediatek-drm composite object */
extern int mtkfb_set_backlight_level(unsigned int level);

#define OPLUS_COMPAT_MAX_BRIGHTNESS	4095U
#define OPLUS_COMPAT_NORMAL_MAX		2047U
#define MTK_HW_MAX_BRIGHTNESS		2047U

static struct kobject *oplus_disp_kobj;
static unsigned int oplus_brightness_cache;
static DEFINE_MUTEX(oplus_brightness_lock);

/*
 * ColorOS writes 0..4095, the chopin panel wants 0..2047.  Scale with
 * rounding so 4095 maps to exactly 2047 and no level is lost to truncation.
 */
static unsigned int oplus_to_mtk_level(unsigned int level)
{
	if (level > OPLUS_COMPAT_MAX_BRIGHTNESS)
		level = OPLUS_COMPAT_MAX_BRIGHTNESS;

	return (level * MTK_HW_MAX_BRIGHTNESS +
		OPLUS_COMPAT_MAX_BRIGHTNESS - 1) / OPLUS_COMPAT_MAX_BRIGHTNESS;
}

static ssize_t oplus_brightness_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	unsigned int level;

	mutex_lock(&oplus_brightness_lock);
	level = oplus_brightness_cache;
	mutex_unlock(&oplus_brightness_lock);

	return sprintf(buf, "%u\n", level);
}

static ssize_t oplus_brightness_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int level;
	unsigned int hw_level;
	int ret;

	ret = kstrtouint(buf, 0, &level);
	if (ret)
		return ret;

	if (level > OPLUS_COMPAT_MAX_BRIGHTNESS)
		level = OPLUS_COMPAT_MAX_BRIGHTNESS;
	hw_level = oplus_to_mtk_level(level);

	mutex_lock(&oplus_brightness_lock);
	oplus_brightness_cache = level;
	mutex_unlock(&oplus_brightness_lock);

	mtkfb_set_backlight_level(hw_level);

	return count;
}

static ssize_t oplus_max_brightness_show(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "%u\n", OPLUS_COMPAT_MAX_BRIGHTNESS);
}

static ssize_t oplus_normal_max_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", OPLUS_COMPAT_NORMAL_MAX);
}

static struct kobj_attribute oplus_attr_brightness =
	__ATTR(oplus_brightness, S_IRUGO | S_IWUSR,
	       oplus_brightness_show, oplus_brightness_store);

static struct kobj_attribute oplus_attr_max_brightness =
	__ATTR(oplus_max_brightness, S_IRUGO,
	       oplus_max_brightness_show, NULL);

static struct kobj_attribute oplus_attr_normal_max =
	__ATTR(max_brightness, S_IRUGO, oplus_normal_max_show, NULL);

/*
 * Interface-only attributes.  The real OPlus implementation drives panel
 * registers, fingerprint/HBM state machines, ADFR and thermal compensation
 * through mediatek_v2 helpers that do not exist on 4.14.186.  Keeping the
 * node present with a stable value is enough for the HAL's feature probe;
 * it must never be silently absent.
 */
#define OPLUS_COMPAT_STUB(_name)					\
static unsigned int oplus_stub_##_name;					\
static ssize_t oplus_stub_##_name##_show(struct kobject *kobj,		\
					 struct kobj_attribute *attr,	\
					 char *buf)			\
{									\
	return sprintf(buf, "%u\n", READ_ONCE(oplus_stub_##_name));	\
}									\
static ssize_t oplus_stub_##_name##_store(struct kobject *kobj,	\
					  struct kobj_attribute *attr,	\
					  const char *buf, size_t count)	\
{									\
	unsigned int val;						\
	if (kstrtouint(buf, 0, &val))					\
		return -EINVAL;						\
	WRITE_ONCE(oplus_stub_##_name, val);				\
	return count;							\
}									\
static struct kobj_attribute oplus_attr_##_name =			\
	__ATTR(_name, S_IRUGO | S_IWUSR,				\
	       oplus_stub_##_name##_show, oplus_stub_##_name##_store)

OPLUS_COMPAT_STUB(oplus_get_color_status);
OPLUS_COMPAT_STUB(seed);
OPLUS_COMPAT_STUB(panel_pwr);
OPLUS_COMPAT_STUB(dim_alpha);
OPLUS_COMPAT_STUB(panel_serial_number);
OPLUS_COMPAT_STUB(LCM_CABC);
OPLUS_COMPAT_STUB(esd);
OPLUS_COMPAT_STUB(sau_closebl_node);
OPLUS_COMPAT_STUB(dsi_log_switch);
OPLUS_COMPAT_STUB(trace_enable);
OPLUS_COMPAT_STUB(fp_type);
OPLUS_COMPAT_STUB(fp_state);
OPLUS_COMPAT_STUB(fake_aod);
OPLUS_COMPAT_STUB(oplus_notify_fppress);
OPLUS_COMPAT_STUB(hbm);
OPLUS_COMPAT_STUB(aod_light_mode_set);
OPLUS_COMPAT_STUB(adfr_debug);
OPLUS_COMPAT_STUB(adfr_params);
OPLUS_COMPAT_STUB(vsync_switch);
OPLUS_COMPAT_STUB(temp_compensation_config);
OPLUS_COMPAT_STUB(ntc_temp);
OPLUS_COMPAT_STUB(shell_temp);
OPLUS_COMPAT_STUB(disp_trig_db);
OPLUS_COMPAT_STUB(write_panel_reg);
OPLUS_COMPAT_STUB(panel_reg_cmd);
OPLUS_COMPAT_STUB(dimlayer_bl_en);
OPLUS_COMPAT_STUB(dim_dc_alpha);
OPLUS_COMPAT_STUB(osc);
OPLUS_COMPAT_STUB(pq_trigger);
OPLUS_COMPAT_STUB(pwm_turbo);

static struct attribute *oplus_disp_attrs[] = {
	&oplus_attr_brightness.attr,
	&oplus_attr_max_brightness.attr,
	&oplus_attr_normal_max.attr,
	&oplus_attr_oplus_get_color_status.attr,
	&oplus_attr_seed.attr,
	&oplus_attr_panel_pwr.attr,
	&oplus_attr_dim_alpha.attr,
	&oplus_attr_panel_serial_number.attr,
	&oplus_attr_LCM_CABC.attr,
	&oplus_attr_esd.attr,
	&oplus_attr_sau_closebl_node.attr,
	&oplus_attr_dsi_log_switch.attr,
	&oplus_attr_trace_enable.attr,
	&oplus_attr_fp_type.attr,
	&oplus_attr_fp_state.attr,
	&oplus_attr_fake_aod.attr,
	&oplus_attr_oplus_notify_fppress.attr,
	&oplus_attr_hbm.attr,
	&oplus_attr_aod_light_mode_set.attr,
	&oplus_attr_adfr_debug.attr,
	&oplus_attr_adfr_params.attr,
	&oplus_attr_vsync_switch.attr,
	&oplus_attr_temp_compensation_config.attr,
	&oplus_attr_ntc_temp.attr,
	&oplus_attr_shell_temp.attr,
	&oplus_attr_disp_trig_db.attr,
	&oplus_attr_write_panel_reg.attr,
	&oplus_attr_panel_reg_cmd.attr,
	&oplus_attr_dimlayer_bl_en.attr,
	&oplus_attr_dim_dc_alpha.attr,
	&oplus_attr_osc.attr,
	&oplus_attr_pq_trigger.attr,
	&oplus_attr_pwm_turbo.attr,
	NULL,
};

static struct attribute_group oplus_disp_attr_group = {
	.attrs = oplus_disp_attrs,
};

static int __init oplus_disp_compat_init(void)
{
	int ret;

	oplus_disp_kobj = kobject_create_and_add("oplus_display", kernel_kobj);
	if (!oplus_disp_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(oplus_disp_kobj, &oplus_disp_attr_group);
	if (ret) {
		pr_err("oplus_display: failed to create sysfs group (%d)\n", ret);
		kobject_put(oplus_disp_kobj);
		oplus_disp_kobj = NULL;
		return ret;
	}

	pr_info("oplus_display: compat nodes ready (0..%u -> 0..%u MTK level)\n",
		OPLUS_COMPAT_MAX_BRIGHTNESS, MTK_HW_MAX_BRIGHTNESS);

	return 0;
}
late_initcall(oplus_disp_compat_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OPlus/ColorOS display sysfs compatibility for mt6891 4.14");
