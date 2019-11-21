// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stop_machine.h>

#include <asm/cpucaps.h>
#include <asm/cputype.h>
#include <asm/hwcap.h>
#include <asm/sysreg.h>

static struct static_key_false *errata_219_key =
			&cpu_hwcap_keys[ARM64_WORKAROUND_CAVIUM_TX2_219_TVM];
static int cpu_unleashed;
static atomic_t primary_cpu;

static int errata_runtime_switch(void *data)
{
	int *val = data;

	/*
	 * Let the first CPU here to patch the kernel,
	 * the rest CPUs are lazily waiting.
	 */
	if (atomic_add_unless(&primary_cpu, 1, 1)) {
		/*
		 * static_branc_{disable|enable}() performs
		 * __flush_icache_range() once the kernel is patched.
		 */
		if (*val == 0)
			static_branch_disable(errata_219_key);
		else
			static_branch_enable(errata_219_key);

		isb();
		WRITE_ONCE(cpu_unleashed, 1);
	} else {
		while (!READ_ONCE(cpu_unleashed))
			cpu_relax();
		isb();
	}

	return 0;
}


/* Sysfs functions */

static struct kobject *kobj_ref;

static ssize_t errata_219_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	bool errata_219 = cpus_have_const_cap(ARM64_WORKAROUND_CAVIUM_TX2_219_TVM);
	return sprintf(buf, "ThunderX2 Errata 219 workaround %s\n",
			errata_219 ? "ON" : "OFF");
}

static ssize_t errata_219_store(struct kobject *kobj,
		struct kobj_attribute *attr,const char *buf, size_t count)
{
	int ret = 0;
	int value;

	if (kstrtoint(buf, 0, &value))
		return -EINVAL;

	switch (value) {
	case 0 ... 1:
		atomic_set(&primary_cpu, 0);
		ret = stop_machine(errata_runtime_switch, &value, cpu_online_mask);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret < 0 ? ret : count;
}

static struct kobj_attribute errata_219_attr =
		__ATTR(errata_219, 0660, errata_219_show, errata_219_store);
/* !Sysfs functions */


int tx2errata_init(struct kobject *parent)
{
	int error = 0;

	/* Create sysfs directory for errata workaround list*/
	kobj_ref = kobject_create_and_add("tx2_erratas", parent);
	if (!kobj_ref) {
		pr_err("Cannot create sysfs directory\n");
		return -ENOMEM;
	}

	/* Create sysfs file for errata 219 workaround*/
	error = sysfs_create_file(kobj_ref, &errata_219_attr.attr);
	if (error)
		pr_err("Cannot create sysfs file\n");

	return error;
}

void tx2errata_cleanup(struct kobject *parent)
{
	sysfs_remove_file(kobj_ref, &errata_219_attr.attr);
	kobject_put(kobj_ref);
}
