/*!
 * Copyright (c) 2019-2021 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-io.
 *
 * tuxedo-io is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "tuxedo_io_ioctl.h"
#include "../clevo_interfaces.h"
#include "../uniwill_interfaces.h"

MODULE_DESCRIPTION("Hardware interface for TUXEDO laptops");
MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_VERSION("0.2.4");
MODULE_LICENSE("GPL");

MODULE_ALIAS_CLEVO_INTERFACES();
MODULE_ALIAS("wmi:" CLEVO_WMI_METHOD_GUID);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BA);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BB);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BC);


typedef void (uniwill_set_power_mode_func)(u8);
extern uniwill_set_power_mode_func uniwill_set_power_mode;


// Initialized in module init, global for ioctl interface
static u32 id_check_clevo;
static u32 id_check_uniwill;

static u32 clevo_identify(void)
{
	return clevo_get_active_interface_id(NULL) == 0 ? 1 : 0;
}

static u32 uniwill_identify(void)
{
	return uniwill_get_active_interface_id(NULL) == 0 ? 1 : 0;
}

/*static int fop_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int fop_release(struct inode *inode, struct file *file)
{
	return 0;
}*/

static long clevo_ioctl_interface(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 result = 0, status;
	u32 copy_result;
	u32 argument = (u32) arg;

	u32 clevo_arg;

	const char str_no_if[] = "";
	char *str_clevo_if;
	
	switch (cmd) {
		case R_CL_HW_IF_STR:
			if (clevo_get_active_interface_id(&str_clevo_if) == 0) {
				copy_result = copy_to_user((char *) arg, str_clevo_if, strlen(str_clevo_if) + 1);
			} else {
				copy_result = copy_to_user((char *) arg, str_no_if, strlen(str_no_if) + 1);
			}
			break;
		case R_CL_FANINFO1:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO1, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_FANINFO2:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO2, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_FANINFO3:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO3, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		/*case R_CL_FANINFO4:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO4, 0);
			copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;*/
		case R_CL_WEBCAM_SW:
			status = clevo_evaluate_method(CLEVO_CMD_GET_WEBCAM_SW, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_FLIGHTMODE_SW:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FLIGHTMODE_SW, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_TOUCHPAD_SW:
			status = clevo_evaluate_method(CLEVO_CMD_GET_TOUCHPAD_SW, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
	}

	switch (cmd) {
		case W_CL_FANSPEED:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_FANSPEED_VALUE, argument, &result);
			// Note: Delay needed to let hardware catch up with the written value.
			// No known ready flag. If the value is read too soon, the old value
			// will still be read out.
			// (Theoretically needed for other methods as well.)
			// Can it be lower? 50ms is too low
			msleep(100);
			break;
		case W_CL_FANAUTO:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_FANSPEED_AUTO, argument, &result);
			break;
		case W_CL_WEBCAM_SW:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			status = clevo_evaluate_method(CLEVO_CMD_GET_WEBCAM_SW, 0, &result);
			// Only set status if it isn't already the right value
			// (workaround for old and/or buggy WMI interfaces that toggle on write)
			if ((argument & 0x01) != (result & 0x01)) {
				clevo_evaluate_method(CLEVO_CMD_SET_WEBCAM_SW, argument, &result);
			}
			break;
		case W_CL_FLIGHTMODE_SW:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_FLIGHTMODE_SW, argument, &result);
			break;
		case W_CL_TOUCHPAD_SW:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_TOUCHPAD_SW, argument, &result);
			break;
		case W_CL_PERF_PROFILE:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_arg = (CLEVO_OPT_SUBCMD_SET_PERF_PROF << 0x18) | (argument & 0xff);
			clevo_evaluate_method(CLEVO_CMD_OPT, clevo_arg, &result);
			break;
	}

	return 0;
}

static u32 uw_set_fan(u32 fan_index, u8 fan_speed)
{
	u32 i;
	u8 mode_data;
	u16 addr_fan0 = 0x1804;
	u16 addr_fan1 = 0x1809;
	u16 addr_for_fan;

	if (fan_index == 0)
		addr_for_fan = addr_fan0;
	else if (fan_index == 1)
		addr_for_fan = addr_fan1;
	else
		return -EINVAL;

	// Check current mode
	uniwill_read_ec_ram(0x0751, &mode_data);
	if (!(mode_data & 0x40)) {
		// If not "full fan mode" (i.e. 0x40 bit set) switch to it (required for fancontrol)
		uniwill_write_ec_ram(0x0751, mode_data | 0x40);
		// Attempt to write both fans as quick as possible before complete ramp-up
		pr_debug("prevent ramp-up start\n");
		for (i = 0; i < 10; ++i) {
			uniwill_write_ec_ram(addr_fan0, fan_speed & 0xff);
			uniwill_write_ec_ram(addr_fan1, fan_speed & 0xff);
			msleep(10);
		}
		pr_debug("prevent ramp-up done\n");
	} else {
		// Otherwise just set the chosen fan
		uniwill_write_ec_ram(addr_for_fan, fan_speed & 0xff);
	}

	return 0;
}

static u32 uw_set_fan_auto(void)
{
	u8 mode_data;
	// Get current mode
	uniwill_read_ec_ram(0x0751, &mode_data);
	// Switch off "full fan mode" (i.e. unset 0x40 bit)
	uniwill_write_ec_ram(0x0751, mode_data & 0xbf);

	return 0;
}

static long uniwill_ioctl_interface(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 result = 0;
	u32 copy_result;
	u32 argument;
	u8 byte_data;
	const char str_no_if[] = "";
	char *str_uniwill_if;

#ifdef DEBUG
	union uw_ec_read_return reg_read_return;
	union uw_ec_write_return reg_write_return;
	u32 uw_arg[10];
	u32 uw_result[10];
	int i;
	for (i = 0; i < 10; ++i) {
		uw_result[i] = 0xdeadbeef;
	}
#endif

	switch (cmd) {
		case R_UW_HW_IF_STR:
			if (uniwill_get_active_interface_id(&str_uniwill_if) == 0) {
				copy_result = copy_to_user((char *) arg, str_uniwill_if, strlen(str_uniwill_if) + 1);
			} else {
				copy_result = copy_to_user((char *) arg, str_no_if, strlen(str_no_if) + 1);
			}
			break;
		case R_UW_FANSPEED:
			uniwill_read_ec_ram(0x1804, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FANSPEED2:
			uniwill_read_ec_ram(0x1809, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FAN_TEMP:
			uniwill_read_ec_ram(0x043e, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FAN_TEMP2:
			uniwill_read_ec_ram(0x044f, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_MODE:
			uniwill_read_ec_ram(0x0751, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_POWER_MODE:
			uniwill_read_ec_ram(0x0751, &byte_data);
			result = byte_data & 0x90;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_MODE_ENABLE:
			uniwill_read_ec_ram(0x0741, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
#ifdef DEBUG
		case R_TF_BC:
			copy_result = copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg));
			reg_read_return.dword = 0;
			result = uniwill_read_ec_ram((uw_arg[1] << 8) | uw_arg[0], &reg_read_return.bytes.data_low);
			copy_result = copy_to_user((void *) arg, &reg_read_return.dword, sizeof(reg_read_return.dword));
			// pr_info("R_TF_BC args [%0#2x, %0#2x, %0#2x, %0#2x]\n", uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3]);
			/*if (uniwill_ec_direct) {
				result = uw_ec_read_addr_direct(uw_arg[0], uw_arg[1], &reg_read_return);
				copy_result = copy_to_user((void *) arg, &reg_read_return.dword, sizeof(reg_read_return.dword));
			} else {
				result = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 1, uw_result);
				copy_result = copy_to_user((void *) arg, &uw_result, sizeof(uw_result));
			}*/
			break;
#endif
	}

	switch (cmd) {
		case W_UW_FANSPEED:
			// Get fan speed argument
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_fan(0, argument);
			break;
		case W_UW_FANSPEED2:
			// Get fan speed argument
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_fan(1, argument);
			break;
		case W_UW_MODE:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uniwill_read_ec_ram(0x0751, &byte_data);
			result = byte_data;
			argument &= ~ 0x90;
			argument |= result & 0x90;
			uniwill_write_ec_ram(0x0751, argument & 0xff);
			break;
		case W_UW_POWER_MODE:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uniwill_read_ec_ram(0x0751, &byte_data);
			result = byte_data;
			argument &= 0x90;
			argument |= result & (~ 0x90);
			uniwill_write_ec_ram(0x0751, argument & 0xff);
			break;
		case W_UW_MODE_ENABLE:
			// Note: Is for the moment set and cleared on init/exit of module (uniwill mode)
			/*
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uniwill_write_ec_ram(0x0741, argument & 0x01);
			*/
			break;
		case W_UW_FANAUTO:
			uw_set_fan_auto();
			break;
#ifdef DEBUG
		case W_TF_BC:
			reg_write_return.dword = 0;
			copy_result = copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg));
			uniwill_write_ec_ram((uw_arg[1] << 8) | uw_arg[0], uw_arg[2]);
			copy_result = copy_to_user((void *) arg, &reg_write_return.dword, sizeof(reg_write_return.dword));
			/*if (uniwill_ec_direct) {
				result = uw_ec_write_addr_direct(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], &reg_write_return);
				copy_result = copy_to_user((void *) arg, &reg_write_return.dword, sizeof(reg_write_return.dword));
			} else {
				result = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 0, uw_result);
				copy_result = copy_to_user((void *) arg, &uw_result, sizeof(uw_result));
				reg_write_return.dword = uw_result[0];
			}*/
			/*pr_info("data_high %0#2x\n", reg_write_return.bytes.data_high);
			pr_info("data_low %0#2x\n", reg_write_return.bytes.data_low);
			pr_info("addr_high %0#2x\n", reg_write_return.bytes.addr_high);
			pr_info("addr_low %0#2x\n", reg_write_return.bytes.addr_low);*/
			break;
#endif
	}

	return 0;
}

struct event_stream_t
{
	DECLARE_KFIFO(event_queue, char, (1 << 10));
	spinlock_t kfifo_lock;

	wait_queue_head_t lock;
	struct list_head next_prev_stream;
};

LIST_HEAD(event_streams);


void io_announce_event(char event, char msg){

	struct event_stream_t *event_stream;
	list_for_each_entry ( event_stream , &event_streams, next_prev_stream ) 
	{
		if (kfifo_avail(&event_stream->event_queue) < 5) {
			
			unsigned long _flags;
			spin_lock_irqsave(&event_stream->kfifo_lock, _flags);
			while (kfifo_avail(&event_stream->event_queue) < 6)
			{
				kfifo_skip(&event_stream->event_queue);
			}
			spin_unlock_irqrestore(&event_stream->kfifo_lock, _flags);
			// Warn the reader that he lost events and may need to update itself using ioctl calls
			kfifo_put(&event_stream->event_queue, '\177');

		}

		// This format allows future proofing for more flexibility towards potential longer data in the future

		kfifo_put(&event_stream->event_queue, '\2');
		kfifo_put(&event_stream->event_queue, event);
		kfifo_put(&event_stream->event_queue, '\36');
		kfifo_put(&event_stream->event_queue, msg);
		kfifo_put(&event_stream->event_queue, '\3');

		wake_up(&event_stream->lock);
    }

}
EXPORT_SYMBOL(io_announce_event);

static int open_for_events(struct inode *inode, struct file *file)
{
	struct event_stream_t *event_stream = vmalloc(sizeof(struct event_stream_t));
	if(!event_stream){
		return -ENOMEM;
	}
	init_waitqueue_head(&event_stream->lock);
	INIT_KFIFO(event_stream->event_queue);
	spin_lock_init(&event_stream->kfifo_lock);

	file->private_data = event_stream;

	list_add_tail(&event_stream->next_prev_stream, &event_streams);

	pr_info("Opening!");

    return 0;
}

static ssize_t read_events(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	struct event_stream_t *event_stream = (struct event_stream_t *) file->private_data;
	// ssize_t len = min(my_data->size - *offset, size);
	int copied_size = 0;

	if(wait_event_interruptible(event_stream->lock, !kfifo_is_empty(&event_stream->event_queue))){
		return -EINTR;
	}

	if (size < 1) {
		return 0;
	}

	// Locking here should be impossible. If this locks, it's for the storing process above
	{
		unsigned long _flags;
		int kfifo_result; // can only be logged
		spin_lock_irqsave(&event_stream->kfifo_lock, _flags);
		kfifo_result = kfifo_to_user(&event_stream->event_queue, user_buffer, size, &copied_size);
		if(kfifo_result < 0){
			pr_err("kfifo failed to send the contents to the user. Sending to buffer: (%p)", &user_buffer);
		}
		spin_unlock_irqrestore(&event_stream->kfifo_lock, _flags);
	}

	*offset += copied_size;
	return copied_size;
}

static int closed_for_events(struct inode *inode, struct file *file)
{
	struct event_stream_t *event_stream = (struct event_stream_t *) file->private_data;

	list_del(&event_stream->next_prev_stream);
	vfree(event_stream);
	
	pr_info("Closing");
	return 0;
}


static struct file_operations fops_dev_evt = {
	.owner				= THIS_MODULE,
//	.unlocked_ioctl		= fop_ioctl
	.open				= open_for_events,
	.read				= read_events,
	.release			= closed_for_events
};


static struct miscdevice user_events_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "tuxedo!user_events",
	.fops	= &fops_dev_evt,
	.mode	= 0444,
};



static int create_events_inode(void){

	int err;

	err = misc_register(&user_events_device);
	if (err != 0) {
		pr_err("Failed to allocate user_events\n");
		return err;
	}
	return 0;


}

static void __exit remove_events_file(void)
{
	misc_deregister(&user_events_device);
	pr_debug("Module events exit\n");
}



static long fop_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 status;
	// u32 result = 0;
	u32 copy_result;

	const char *module_version = THIS_MODULE->version;
	switch (cmd) {
		case R_MOD_VERSION:
			copy_result = copy_to_user((char *) arg, module_version, strlen(module_version) + 1);
			break;
		// Hardware id checks, 1 = positive, 0 = negative
		case R_HWCHECK_CL:
			id_check_clevo = clevo_identify();
			copy_result = copy_to_user((void *) arg, (void *) &id_check_clevo, sizeof(id_check_clevo));
			break;
		case R_HWCHECK_UW:
			id_check_uniwill = uniwill_identify();
			copy_result = copy_to_user((void *) arg, (void *) &id_check_uniwill, sizeof(id_check_uniwill));
			break;
	}

	status = clevo_ioctl_interface(file, cmd, arg);
	if (status != 0) return status;
	status = uniwill_ioctl_interface(file, cmd, arg);
	if (status != 0) return status;

	return 0;
}

static struct file_operations fops_dev = {
	.owner              = THIS_MODULE,
	.unlocked_ioctl     = fop_ioctl
//	.open               = fop_open,
//	.release            = fop_release
};

struct class *tuxedo_io_device_class;
dev_t tuxedo_io_device_handle;

static struct cdev tuxedo_io_cdev;

static int __init tuxedo_io_init(void)
{
	int err;

	// Hardware identification
	id_check_clevo = clevo_identify();
	id_check_uniwill = uniwill_identify();

#ifdef DEBUG
	if (id_check_clevo == 0 && id_check_uniwill == 0) {
		pr_debug("No matching hardware found on module load\n");
	}
#endif

	err = alloc_chrdev_region(&tuxedo_io_device_handle, 0, 1, "tuxedo_io_cdev");
	if (err != 0) {
		pr_err("Failed to allocate chrdev region\n");
		return err;
	}
	cdev_init(&tuxedo_io_cdev, &fops_dev);
	err = (cdev_add(&tuxedo_io_cdev, tuxedo_io_device_handle, 1));
	if (err < 0) {
		pr_err("Failed to add cdev\n");
		unregister_chrdev_region(tuxedo_io_device_handle, 1);
	}
	tuxedo_io_device_class = class_create(THIS_MODULE, "tuxedo_io");
	device_create(tuxedo_io_device_class, NULL, tuxedo_io_device_handle, NULL, "tuxedo_io");
	create_events_inode();
	
	pr_debug("Module init successful\n");
	
	return 0;
}

static void __exit tuxedo_io_exit(void)
{
	remove_events_file();

	sysfs_remove_link(&tuxedo_io_cdev.kobj, "tuxedo_io");

	device_destroy(tuxedo_io_device_class, tuxedo_io_device_handle);
	class_destroy(tuxedo_io_device_class);
	cdev_del(&tuxedo_io_cdev);
	unregister_chrdev_region(tuxedo_io_device_handle, 1);
	pr_debug("Module exit\n");
}

module_init(tuxedo_io_init);
module_exit(tuxedo_io_exit);
