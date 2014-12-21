/*
 * Author: Jean-Pierre Rasquin <yank555.lu@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * CPU freq hard limit - SysFS interface :
 * ---------------------------------------
 *
 * /sys/kernel/cpufreq_hardlimit/scaling_max_freq_screen_on (rw)
 *
 *   set or show the real hard CPU max frequency limit when screen is on
 *
 * /sys/kernel/cpufreq_hardlimit/scaling_max_freq_screen_off (rw)
 *
 *   set or show the real hard CPU max frequency limit when screen is off
 *
 * /sys/kernel/cpufreq_hardlimit/scaling_min_freq_screen_on (rw)
 *
 *   set or show the real hard CPU min frequency limit when screen is on
 *
 * /sys/kernel/cpufreq_hardlimit/scaling_min_freq_screen_off (rw)
 *
 *   set or show the real hard CPU min frequency limit when screen is off
 *
 * /sys/kernel/cpufreq_hardlimit/wakeup_kick_freq (rw)
 *
 *   set or show the wakeup kick frequency (scaling_min for delay time)
 *
 * /sys/kernel/cpufreq_hardlimit/wakeup_kick_delay (rw)
 *
 *   set or show the wakeup kick duration (in ms)
 *
 * /sys/kernel/cpufreq_hardlimit/touchboost_lo_freq (rw)
 *
 *   set or show touchboost low frequency
 *
 * /sys/kernel/cpufreq_hardlimit/touchboost_hi_freq (rw)
 *
 *   set or show touchboost high frequency
 *
 * /sys/kernel/cpufreq_hardlimit/touchboost_delay (rw)
 *
 *   set or show touchboost delay (0 = disabled, up to 10000ms)
 *
 * /sys/kernel/cpufreq_hardlimit/touchboost_eventcount (rw)
 *
 *   set or show touchboost eventcount necessary to go into high frequency (1-10)
 *
 * /sys/kernel/cpufreq_hardlimit/userspace_dvfs_lock (rw)
 *
 *   0 = allow changes to scaling min/max
 *   1 = ignore (don't apply, but don't return an error)
 *   2 = refuse (don't apply, return EINVAL)
 *
 * /sys/kernel/cpufreq_hardlimit/available_frequencies (ro)
 *
 *   display list of available CPU frequencies for convenience
 *
 * /sys/kernel/cpufreq_hardlimit/current_limit_max (ro)
 *
 *   display current applied hardlimit for CPU max
 *
 * /sys/kernel/cpufreq_hardlimit/current_limit_min (ro)
 *
 *   display current applied hardlimit for CPU min
 *
 * /sys/kernel/cpufreq_hardlimit/version (ro)
 *
 *   display CPU freq hard limit version information
 *
 */

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpufreq_hardlimit.h>
#include <linux/cpufreq.h>
#include <linux/powersuspend.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>

unsigned int hardlimit_max_screen_on  = CPUFREQ_HARDLIMIT_MAX_SCREEN_ON_STOCK;  /* default to stock behaviour */
unsigned int hardlimit_max_screen_off = CPUFREQ_HARDLIMIT_MAX_SCREEN_OFF_STOCK; /* default to stock behaviour */
unsigned int hardlimit_min_screen_on  = CPUFREQ_HARDLIMIT_MIN_SCREEN_ON_STOCK;  /* default to stock behaviour */
unsigned int hardlimit_min_screen_off = CPUFREQ_HARDLIMIT_MIN_SCREEN_OFF_STOCK; /* default to stock behaviour */
unsigned int wakeup_kick_freq         = CPUFREQ_HARDLIMIT_MIN_SCREEN_ON_STOCK;  /* default to stock behaviour */
unsigned int wakeup_kick_delay        = CPUFREQ_HARDLIMIT_WAKEUP_KICK_DISABLED; /* default to stock behaviour */
unsigned int wakeup_kick_active       = CPUFREQ_HARDLIMIT_WAKEUP_KICK_INACTIVE;
unsigned int touchboost_lo_freq       = CPUFREQ_HARDLIMIT_TOUCHBOOST_LO_DEFAULT;
unsigned int touchboost_hi_freq       = CPUFREQ_HARDLIMIT_TOUCHBOOST_HI_DEFAULT;
unsigned int touchboost_active        = CPUFREQ_HARDLIMIT_TOUCHBOOST_INACTIVE;
unsigned int touchboost_delay         = CPUFREQ_HARDLIMIT_TOUCHBOOST_DISABLED;	/* default to stock behaviour */
unsigned int touchboost_eventcount    = CPUFREQ_HARDLIMIT_TOUCHBOOST_EVENTS;    /* default to 5 touches to jump to hi */
unsigned int touchevent_count         = 0;
unsigned int previous_finger_count    = 0;
unsigned int userspace_dvfs_lock      = CPUFREQ_HARDLIMIT_USERSPACE_DVFS_ALLOW;	/* default allows userspace dvfs interaction */


unsigned int current_limit_max        = CPUFREQ_HARDLIMIT_MAX_SCREEN_ON_STOCK;
unsigned int current_limit_min        = CPUFREQ_HARDLIMIT_MIN_SCREEN_ON_STOCK;
unsigned int current_screen_state     = CPUFREQ_HARDLIMIT_SCREEN_ON;		/* default to screen on */

struct delayed_work stop_wakeup_kick_work;

struct delayed_work stop_touchboost_work;

/* Externally reachable function */

/* Sanitize cpufreq to hardlimits */
unsigned int check_cpufreq_hardlimit(unsigned int freq)
{
// Called way too often, even when debugging
//	#ifdef CPUFREQ_HARDLIMIT_DEBUG
//	pr_info("[HARDLIMIT] check_cpufreq_hardlimit : min = %u / max = %u / freq = %u / result = %u \n",
//			current_limit_min,
//			current_limit_max,
//			freq,
//			max(current_limit_min, min(current_limit_max, freq))
//		);
//	#endif
	return max(current_limit_min, min(current_limit_max, freq));
}

/* Update limits in cpufreq */
void reapply_hard_limits(void)
{
	#ifdef CPUFREQ_HARDLIMIT_DEBUG
	pr_info("[HARDLIMIT] reapply_hard_limits - before : min = %u / max = %u \n",
			current_limit_min,
			current_limit_max
		);
	#endif

	/* Recalculate the currently applicable min/max */
	if (current_screen_state == CPUFREQ_HARDLIMIT_SCREEN_ON) {

		if(wakeup_kick_active == CPUFREQ_HARDLIMIT_WAKEUP_KICK_ACTIVE) {

			current_limit_min  = wakeup_kick_freq;
			current_limit_max  = max(hardlimit_max_screen_on, min(hardlimit_max_screen_on, wakeup_kick_freq));

		} else if(touchboost_active != CPUFREQ_HARDLIMIT_TOUCHBOOST_INACTIVE) {

			if (touchboost_active == CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_LO) {
				current_limit_min  = touchboost_lo_freq;
			} else {
				current_limit_min  = touchboost_hi_freq;
			}
			current_limit_max  = hardlimit_max_screen_on;

		} else {

			current_limit_min  = hardlimit_min_screen_on;
			current_limit_max  = hardlimit_max_screen_on;

		}

	} else {

		current_limit_min  = hardlimit_min_screen_off;
		current_limit_max  = hardlimit_max_screen_off;

	}

	#ifdef CPUFREQ_HARDLIMIT_DEBUG
	pr_info("[HARDLIMIT] reapply_hard_limits - after : min = %u / max = %u \n",
			current_limit_min,
			current_limit_max
		);
	#endif
	update_scaling_limits(current_limit_min, current_limit_max);
}

/* Scaling min/max lock */
unsigned int userspace_dvfs_lock_status(void)
{
	return userspace_dvfs_lock;
}

/* Powersuspend */
static void cpufreq_hardlimit_suspend(struct power_suspend * h)
{
	#ifdef CPUFREQ_HARDLIMIT_DEBUG
	pr_info("[HARDLIMIT] suspend : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
			current_limit_min,
			current_limit_max,
			hardlimit_min_screen_off,
			hardlimit_max_screen_off
		);
	#endif
	current_screen_state = CPUFREQ_HARDLIMIT_SCREEN_OFF;
	reapply_hard_limits();
	return;
}

static void cpufreq_hardlimit_resume(struct power_suspend * h)
{
	current_screen_state = CPUFREQ_HARDLIMIT_SCREEN_ON;

	if(wakeup_kick_delay == CPUFREQ_HARDLIMIT_WAKEUP_KICK_DISABLED) {
		#ifdef CPUFREQ_HARDLIMIT_DEBUG
		pr_info("[HARDLIMIT] resume (no wakeup kick) : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
				current_limit_min,
				current_limit_max,
				hardlimit_min_screen_on,
				hardlimit_max_screen_on
			);
		#endif
		wakeup_kick_active = CPUFREQ_HARDLIMIT_WAKEUP_KICK_INACTIVE;
	} else {
		#ifdef CPUFREQ_HARDLIMIT_DEBUG
		pr_info("[HARDLIMIT] resume (with wakeup kick) : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
				current_limit_min,
				current_limit_max,
				wakeup_kick_freq,
				max(hardlimit_max_screen_on, min(hardlimit_max_screen_on, wakeup_kick_freq))
			);
		#endif
		wakeup_kick_active = CPUFREQ_HARDLIMIT_WAKEUP_KICK_ACTIVE;
		/* Schedule delayed work to restore stock scaling min after wakeup kick delay */
		schedule_delayed_work(&stop_wakeup_kick_work, usecs_to_jiffies(wakeup_kick_delay * 1000));
	}
	reapply_hard_limits();
	return;
}

static struct power_suspend cpufreq_hardlimit_suspend_data =
{
	.suspend = cpufreq_hardlimit_suspend,
	.resume = cpufreq_hardlimit_resume,
};

/* ------------------------------------------------------------------------------ */
/* Wakeup kick delayed work                                                       */
/* ------------------------------------------------------------------------------ */
static void stop_wakeup_kick(struct work_struct *work)
{
	#ifdef CPUFREQ_HARDLIMIT_DEBUG
	pr_info("[HARDLIMIT] stop wakeup kick : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
			current_limit_min,
			current_limit_max,
			hardlimit_min_screen_on,
			hardlimit_max_screen_on
		);
	#endif

	/* Back to normal scaling min */
	wakeup_kick_active = CPUFREQ_HARDLIMIT_WAKEUP_KICK_INACTIVE;
	reapply_hard_limits();
}

/* ------------------------------------------------------------------------------ */
/* Touchboost                                                                     */
/* ------------------------------------------------------------------------------ */
void touchboost_report_touch(unsigned int finger_count)
{
	/* Do nothing if touchboost is disabled */
	if (touchboost_delay == CPUFREQ_HARDLIMIT_TOUCHBOOST_DISABLED)
		return;

	if (touchboost_active == CPUFREQ_HARDLIMIT_TOUCHBOOST_INACTIVE) {

		if (likely(finger_count != 0)) { /* Keep TouchBoost going as long as the screen is touched by a finger */
			#ifdef CPUFREQ_HARDLIMIT_DEBUG
			pr_info("[HARDLIMIT] start touchboost (lo) : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
					current_limit_min,
					current_limit_max,
					touchboost_lo_freq,
					hardlimit_max_screen_on
				);
			#endif
			touchboost_active = CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_LO;
			touchevent_count = 1; // Start counting events
			/* Schedule delayed work to restore stock scaling min after touchboost delay */
			schedule_delayed_work(&stop_touchboost_work, usecs_to_jiffies(touchboost_delay * 1000));
			reapply_hard_limits();
		}

	} else if (touchboost_active == CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_LO) {

		if (likely(finger_count != 0)) { /* Keep TouchBoost going as long as the screen is touched by a finger */
			if (touchevent_count < touchboost_eventcount) {
				if (previous_finger_count < finger_count)
					touchevent_count++; // Increase touchcount for lo -> hi
			} else {
				#ifdef CPUFREQ_HARDLIMIT_DEBUG
				pr_info("[HARDLIMIT] start touchboost (hi) : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
						current_limit_min,
						current_limit_max,
						touchboost_hi_freq,
						hardlimit_max_screen_on
					);
				#endif
				touchboost_active = CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_HI;
				reapply_hard_limits();
			}
			/* Reschedule delayed work to turn touchboost off */
			cancel_delayed_work(&stop_touchboost_work);
			schedule_delayed_work(&stop_touchboost_work, usecs_to_jiffies(touchboost_delay * 1000));
		}

	} else if (touchboost_active == CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_HI) {

		if (likely(finger_count != 0)) { /* Keep TouchBoost going as long as the screen is touched by a finger */
			/* Reschedule delayed work to turn touchboost off */
			cancel_delayed_work(&stop_touchboost_work);
			schedule_delayed_work(&stop_touchboost_work, usecs_to_jiffies(touchboost_delay * 1000));
		}

	}
	previous_finger_count = finger_count; /* Remember fingercount */
	return;
};

static void stop_touchboost(struct work_struct *work)
{
	#ifdef CPUFREQ_HARDLIMIT_DEBUG
	pr_info("[HARDLIMIT] stop touchboost : old_min = %u / old_max = %u / new_min = %u / new_max = %u \n",
			current_limit_min,
			current_limit_max,
			hardlimit_min_screen_on,
			hardlimit_max_screen_on
		);
	#endif

	/* Back to normal scaling min */
	touchboost_active = CPUFREQ_HARDLIMIT_TOUCHBOOST_INACTIVE;
	touchevent_count = 0; // Reset counter
	reapply_hard_limits();
}

/* ------------------------------------------------------------------------------ */
/* sysfs interface functions                                                      */
/* ------------------------------------------------------------------------------ */

/* sysfs interface for "hardlimit_max_screen_on" */
static ssize_t hardlimit_max_screen_on_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hardlimit_max_screen_on);
}

static ssize_t hardlimit_max_screen_on_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_hardlimit, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_hardlimit))
		return -EINVAL;

	if (new_hardlimit == hardlimit_max_screen_on)
		return count;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_hardlimit) {
			hardlimit_max_screen_on = new_hardlimit;
			/* Wakeup kick can never be higher than CPU max. hardlimit */
			if(hardlimit_max_screen_on < wakeup_kick_freq)
				wakeup_kick_freq = hardlimit_max_screen_on;
			reapply_hard_limits();
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "hardlimit_max_screen_off" */
static ssize_t hardlimit_max_screen_off_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hardlimit_max_screen_off);
}

static ssize_t hardlimit_max_screen_off_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_hardlimit, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_hardlimit))
		return -EINVAL;

	if (new_hardlimit == hardlimit_max_screen_off)
		return count;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_hardlimit) {
			hardlimit_max_screen_off = new_hardlimit;
			reapply_hard_limits();
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "hardlimit_min_screen_on" */
static ssize_t hardlimit_min_screen_on_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hardlimit_min_screen_on);
}

static ssize_t hardlimit_min_screen_on_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_hardlimit, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_hardlimit))
		return -EINVAL;

	if (new_hardlimit == hardlimit_min_screen_on)
		return count;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_hardlimit) {
			hardlimit_min_screen_on = new_hardlimit;
			/* Wakeup kick can never be higher than CPU max. hardlimit */
			if(hardlimit_min_screen_on > wakeup_kick_freq)
				wakeup_kick_freq = hardlimit_min_screen_on;
			reapply_hard_limits();
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "hardlimit_max_screen_off" */
static ssize_t hardlimit_min_screen_off_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hardlimit_min_screen_off);
}

static ssize_t hardlimit_min_screen_off_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_hardlimit, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_hardlimit))
		return -EINVAL;

	if (new_hardlimit == hardlimit_min_screen_off)
		return count;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_hardlimit) {
			hardlimit_min_screen_off = new_hardlimit;
			reapply_hard_limits();
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "wakeup_kick_freq" */
static ssize_t wakeup_kick_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wakeup_kick_freq);
}

static ssize_t wakeup_kick_freq_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_wakeup_kick_freq, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_wakeup_kick_freq))
		return -EINVAL;

	if (new_wakeup_kick_freq == wakeup_kick_freq)
		return count;

	/* Only allow values between current screen on hardlimits */
	if (new_wakeup_kick_freq > hardlimit_max_screen_on || new_wakeup_kick_freq < hardlimit_min_screen_on)
		return -EINVAL;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_wakeup_kick_freq) {
			wakeup_kick_freq = new_wakeup_kick_freq;
			reapply_hard_limits();
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "wakeup_kick_delay" */
static ssize_t wakeup_kick_delay_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wakeup_kick_delay);
}

static ssize_t wakeup_kick_delay_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_wakeup_kick_delay;

	if (!sscanf(buf, "%du", &new_wakeup_kick_delay))
		return -EINVAL;

	if (new_wakeup_kick_delay >= CPUFREQ_HARDLIMIT_WAKEUP_KICK_DISABLED &&
	    new_wakeup_kick_delay <= CPUFREQ_HARDLIMIT_WAKEUP_KICK_DELAY_MAX   ) {

		wakeup_kick_delay = new_wakeup_kick_delay;
		return count;

	}

	return -EINVAL;

}

/* sysfs interface for "touchboost_lo_freq" */
static ssize_t touchboost_lo_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", touchboost_lo_freq);
}

static ssize_t touchboost_lo_freq_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_touchboost_lo_freq, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_touchboost_lo_freq))
		return -EINVAL;

	if (new_touchboost_lo_freq == touchboost_lo_freq)
		return count;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_touchboost_lo_freq) {
			touchboost_lo_freq = new_touchboost_lo_freq;
			/* Touchboost high freq can never be lower than touchboost low freq */
			if(touchboost_hi_freq < touchboost_lo_freq)
				touchboost_hi_freq = touchboost_lo_freq;
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "touchboost_hi_freq" */
static ssize_t touchboost_hi_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", touchboost_hi_freq);
}

static ssize_t touchboost_hi_freq_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_touchboost_hi_freq, i;

	struct cpufreq_frequency_table *table;

	if (!sscanf(buf, "%du", &new_touchboost_hi_freq))
		return -EINVAL;

	if (new_touchboost_hi_freq == touchboost_hi_freq)
		return count;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency == new_touchboost_hi_freq) {
			touchboost_hi_freq = new_touchboost_hi_freq;
			/* If touchboost was disabled, reenable lo with scaling min screen on freq */
			if(touchboost_lo_freq == CPUFREQ_HARDLIMIT_TOUCHBOOST_DISABLED)
				touchboost_lo_freq = CPUFREQ_HARDLIMIT_MIN_SCREEN_ON_STOCK;
			/* Touchboost low freq can never be higher than touchboost high freq */
			if(touchboost_lo_freq > touchboost_hi_freq)
				touchboost_lo_freq = touchboost_hi_freq;
			return count;
		}

	return -EINVAL;

}

/* sysfs interface for "touchboost_delay" */
static ssize_t touchboost_delay_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", touchboost_delay);
}

static ssize_t touchboost_delay_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_touchboost_delay;

	if (!sscanf(buf, "%du", &new_touchboost_delay))
		return -EINVAL;

	if (new_touchboost_delay == touchboost_delay)
		return count;

	/* Accept to disabled touchboost, but disabled it completely */
	if (new_touchboost_delay >= CPUFREQ_HARDLIMIT_TOUCHBOOST_DISABLED  &&
	    new_touchboost_delay <= CPUFREQ_HARDLIMIT_TOUCHBOOST_DELAY_MAX   ) {
		touchboost_delay = new_touchboost_delay;
		return count;
	}

	return -EINVAL;

}

/* sysfs interface for "touchboost_eventcount" */
static ssize_t touchboost_eventcount_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", touchboost_eventcount);
}

static ssize_t touchboost_eventcount_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_touchboost_eventcount;

	if (!sscanf(buf, "%du", &new_touchboost_eventcount))
		return -EINVAL;

	if (new_touchboost_eventcount == touchboost_eventcount)
		return count;

	/* Accept to disabled touchboost, but disabled it completely */
	if (new_touchboost_eventcount >= CPUFREQ_HARDLIMIT_TOUCHBOOST_EVENTS_MIN  &&
	    new_touchboost_eventcount <= CPUFREQ_HARDLIMIT_TOUCHBOOST_EVENTS_MAX   ) {
		touchboost_eventcount = new_touchboost_eventcount;
		return count;
	}

	return -EINVAL;

}

/* sysfs interface for "userspace_dvfs_lock" */
static ssize_t userspace_dvfs_lock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", userspace_dvfs_lock);
}

static ssize_t userspace_dvfs_lock_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

	unsigned int new_userspace_dvfs_lock;

	if (!sscanf(buf, "%du", &new_userspace_dvfs_lock))
		return -EINVAL;

	if (new_userspace_dvfs_lock == userspace_dvfs_lock)
		return count;

	switch (new_userspace_dvfs_lock) {
		case CPUFREQ_HARDLIMIT_USERSPACE_DVFS_ALLOW :
		case CPUFREQ_HARDLIMIT_USERSPACE_DVFS_IGNORE :
		case CPUFREQ_HARDLIMIT_USERSPACE_DVFS_REFUSE :
			userspace_dvfs_lock = new_userspace_dvfs_lock;
			return count;
		default:
			return -EINVAL;
	}

	/* We should never get here */
	return -EINVAL;

}

/* sysfs interface for "available_frequencies" */
static ssize_t available_frequencies_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t j = 0;

	struct cpufreq_frequency_table *table;

	table = cpufreq_frequency_get_table(0); /* Get frequency table */

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		j += sprintf(&buf[j], "%d ", table[i].frequency);

	j += sprintf(&buf[j], "\n");
	return j;
}

/* sysfs interface for "current_limit_min" */
static ssize_t current_limit_min_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_limit_min);
}

/* sysfs interface for "current_limit_max" */
static ssize_t current_limit_max_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_limit_max);
}

/* sysfs interface for "version" */
static ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CPUFREQ_HARDLIMIT_VERSION);
}

/* ------------------------------------------------------------------------------ */
/* sysfs interface functions - end                                                */
/* ------------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------------ */
/* sysfs interface structure                                                      */
/* ------------------------------------------------------------------------------ */

static struct kobject *hardlimit_kobj;

/* Define sysfs entry attributes */

static struct kobj_attribute hardlimit_max_screen_on_attribute =
__ATTR(scaling_max_freq_screen_on, 0666, hardlimit_max_screen_on_show, hardlimit_max_screen_on_store);

static struct kobj_attribute hardlimit_max_screen_off_attribute =
__ATTR(scaling_max_freq_screen_off, 0666, hardlimit_max_screen_off_show, hardlimit_max_screen_off_store);

static struct kobj_attribute hardlimit_min_screen_on_attribute =
__ATTR(scaling_min_freq_screen_on, 0666, hardlimit_min_screen_on_show, hardlimit_min_screen_on_store);

static struct kobj_attribute hardlimit_min_screen_off_attribute =
__ATTR(scaling_min_freq_screen_off, 0666, hardlimit_min_screen_off_show, hardlimit_min_screen_off_store);

static struct kobj_attribute wakeup_kick_freq_attribute =
__ATTR(wakeup_kick_freq, 0666, wakeup_kick_freq_show, wakeup_kick_freq_store);

static struct kobj_attribute wakeup_kick_delay_attribute =
__ATTR(wakeup_kick_delay, 0666, wakeup_kick_delay_show, wakeup_kick_delay_store);

static struct kobj_attribute touchboost_lo_freq_attribute =
__ATTR(touchboost_lo_freq, 0666, touchboost_lo_freq_show, touchboost_lo_freq_store);

static struct kobj_attribute touchboost_hi_freq_attribute =
__ATTR(touchboost_hi_freq, 0666, touchboost_hi_freq_show, touchboost_hi_freq_store);

static struct kobj_attribute touchboost_delay_attribute =
__ATTR(touchboost_delay, 0666, touchboost_delay_show, touchboost_delay_store);

static struct kobj_attribute touchboost_eventcount_attribute =
__ATTR(touchboost_eventcount, 0666, touchboost_eventcount_show, touchboost_eventcount_store);

static struct kobj_attribute userspace_dvfs_lock_attribute =
__ATTR(userspace_dvfs_lock, 0666, userspace_dvfs_lock_show, userspace_dvfs_lock_store);

static struct kobj_attribute available_frequencies_attribute =
__ATTR(available_frequencies, 0444, available_frequencies_show, NULL);

static struct kobj_attribute current_limit_min_attribute =
__ATTR(current_limit_min, 0444, current_limit_min_show, NULL);

static struct kobj_attribute current_limit_max_attribute =
__ATTR(current_limit_max, 0444, current_limit_max_show, NULL);

static struct kobj_attribute version_attribute =
__ATTR(version, 0444, version_show, NULL);

static struct attribute *hardlimit_attrs[] = {
	&hardlimit_max_screen_on_attribute.attr,
	&hardlimit_max_screen_off_attribute.attr,
	&hardlimit_min_screen_on_attribute.attr,
	&hardlimit_min_screen_off_attribute.attr,
	&wakeup_kick_freq_attribute.attr,
	&wakeup_kick_delay_attribute.attr,
	&touchboost_lo_freq_attribute.attr,
	&touchboost_hi_freq_attribute.attr,
	&touchboost_delay_attribute.attr,
	&touchboost_eventcount_attribute.attr,
	&userspace_dvfs_lock_attribute.attr,
	&available_frequencies_attribute.attr,
	&current_limit_min_attribute.attr,
	&current_limit_max_attribute.attr,
	&version_attribute.attr,
	NULL,
};

static struct attribute_group hardlimit_attr_group = {
.attrs = hardlimit_attrs,
};

/* ------------------------------------------------------------------------------ */
/* sysfs new interface structure - end                                            */
/* ------------------------------------------------------------------------------ */

int hardlimit_init(void)
{
	int hardlimit_retval;

	/* Enable the sysfs interface */
        hardlimit_kobj = kobject_create_and_add("cpufreq_hardlimit", kernel_kobj);
        if (!hardlimit_kobj) {
                return -ENOMEM;
        }
        hardlimit_retval = sysfs_create_group(hardlimit_kobj, &hardlimit_attr_group);
        if (hardlimit_retval)
                kobject_put(hardlimit_kobj);

        if (!hardlimit_retval) {
		/* Only register to powersuspend and delayed work if we were able to create the sysfs interface */
		register_power_suspend(&cpufreq_hardlimit_suspend_data);
		INIT_DELAYED_WORK_DEFERRABLE(&stop_wakeup_kick_work, stop_wakeup_kick);
		INIT_DELAYED_WORK_DEFERRABLE(&stop_touchboost_work, stop_touchboost);
	}

        return (hardlimit_retval);
}
/* end sysfs interface */

void hardlimit_exit(void)
{
	unregister_power_suspend(&cpufreq_hardlimit_suspend_data);
	kobject_put(hardlimit_kobj);
}

module_init(hardlimit_init);
module_exit(hardlimit_exit);
