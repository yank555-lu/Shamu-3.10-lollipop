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

#ifndef _LINUX_CPUFREQ_HARDLIMIT_H
#define _LINUX_CPUFREQ_HARDLIMIT_H

#define CPUFREQ_HARDLIMIT_VERSION "v2.1 by Yank555.lu"

//#define CPUFREQ_HARDLIMIT_DEBUG // Add debugging prints in dmesg

/* Default frequencies for hammerhead */
#define CPUFREQ_HARDLIMIT_MAX_SCREEN_ON_STOCK	2649600
#define CPUFREQ_HARDLIMIT_MAX_SCREEN_OFF_STOCK	2649600
#define CPUFREQ_HARDLIMIT_MIN_SCREEN_ON_STOCK	300000
#define CPUFREQ_HARDLIMIT_MIN_SCREEN_OFF_STOCK	300000

#define CPUFREQ_HARDLIMIT_SCREEN_ON	0		/* default, consider we boot with screen on */
#define CPUFREQ_HARDLIMIT_SCREEN_OFF	1

/* Userspace access to scaling min/max */
#define CPUFREQ_HARDLIMIT_USERSPACE_DVFS_ALLOW	0
#define CPUFREQ_HARDLIMIT_USERSPACE_DVFS_IGNORE	1
#define CPUFREQ_HARDLIMIT_USERSPACE_DVFS_REFUSE	2

#define CPUFREQ_HARDLIMIT_WAKEUP_KICK_DELAY_MAX	10000	/* Don't allow for more than 10 seconds */

#define CPUFREQ_HARDLIMIT_WAKEUP_KICK_INACTIVE	0
#define CPUFREQ_HARDLIMIT_WAKEUP_KICK_ACTIVE	1

#define CPUFREQ_HARDLIMIT_WAKEUP_KICK_DISABLED	0

/* Touchboost */
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_INACTIVE	0
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_LO	1
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_ACTIVE_HI	2

#define CPUFREQ_HARDLIMIT_TOUCHBOOST_LO_DEFAULT	883200
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_HI_DEFAULT	1190400

#define CPUFREQ_HARDLIMIT_TOUCHBOOST_EVENTS	5
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_EVENTS_MIN	1
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_EVENTS_MAX	10

#define CPUFREQ_HARDLIMIT_TOUCHBOOST_DISABLED	0
#define CPUFREQ_HARDLIMIT_TOUCHBOOST_DELAY_MAX	5000	/* Don't allow for more than 5 seconds */

/* Sanitize cpufreq to hardlimits */
unsigned int check_cpufreq_hardlimit(unsigned int freq);

/* Scaling min/max lock */
unsigned int userspace_dvfs_lock_status(void);

/* Hook in cpufreq for scaling min./max. */
void update_scaling_limits(unsigned int freq_min, unsigned int freq_max);

/* Hook in touchscreen driver for touchboost */
void touchboost_report_touch(unsigned int finger_count);

#endif
