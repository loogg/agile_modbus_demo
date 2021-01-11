/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-09-07     armink       the first version
 */

#include <stdarg.h>
#include <stdlib.h>
#include <ulog.h>
#include <rthw.h>
#include <stdint.h>
#include "syslog.h"

#ifdef ULOG_OUTPUT_FLOAT
#include <stdio.h>
#endif

/*
 * reference:
 * http://pubs.opengroup.org/onlinepubs/7908799/xsh/syslog.h.html
 * https://www.gnu.org/software/libc/manual/html_node/Submitting-Syslog-Messages.html
 * http://man7.org/linux/man-pages/man3/syslog.3.html
 */

#ifdef ULOG_USING_SYSLOG

#include <sys/time.h>

#ifndef ULOG_SYSLOG_IDENT_MAX_LEN
#define ULOG_SYSLOG_IDENT_MAX_LEN      ULOG_FILTER_TAG_MAX_LEN
#endif

static char local_ident[ULOG_SYSLOG_IDENT_MAX_LEN + 1];
static int local_facility = LOG_USER;
static int local_option = LOG_USER;
static rt_bool_t is_open = RT_FALSE;

/**
 * open connection to syslog
 *
 * @param ident is an arbitrary identification string which future syslog invocations will prefix to each message.
 * @param option is not using on ulog.
 * @param facility is the default facility code for this connection.
 */
void openlog(const char *ident, int option, int facility)
{
    rt_base_t level;

    ulog_init();

    level = rt_hw_interrupt_disable();

    rt_memset(local_ident, 0, sizeof(local_ident));
    if (ident)
    {
        rt_strncpy(local_ident, ident, ULOG_SYSLOG_IDENT_MAX_LEN);
    }
    else
    {
        rt_strncpy(local_ident, "rtt", ULOG_SYSLOG_IDENT_MAX_LEN);
    }

    local_option = option;

    if (facility)
    {
        local_facility = facility;
    }
    else
    {
        /* default facility is LOG_USER */
        local_facility = LOG_USER;
    }
    /* output all level log */
    setlogmask(LOG_UPTO(LOG_DEBUG));

    is_open = RT_TRUE;

    rt_hw_interrupt_enable(level);

}

/**
 * This is functionally identical to syslog.
 *
 * @param priority log priority, can be generated by the macro LOG_MAKEPRI
 * @param format log format
 * @param args log arguments
 */
void vsyslog(int priority, const char *format, va_list args)
{
    if (LOG_FAC(priority) == 0)
    {
        /* using local facility */
        priority |= local_facility;
    }

    ulog_voutput(priority, local_ident, RT_TRUE, format, args);
}

/**
 * generates a log message
 *
 * @param priority log priority, can be generated by the macro LOG_MAKEPRI
 * @param format log format, like printf()
 */
void syslog(int priority, const char *format, ...)
{
    va_list args;

    if (!is_open)
    {
        openlog(0, 0, 0);
    }
    /* args point to the first variable parameter */
    va_start(args, format);

    vsyslog(priority, format, args);

    va_end(args);
}

/**
 * close the syslog
 */
void closelog(void)
{
    ulog_deinit();

    is_open = RT_FALSE;
}

/**
 * set log priority mask
 *
 * @param mask The log priority mask which generate by macro LOG_MASK and LOG_UPTO.
 *
 * @return This function returns the previous log priority mask.
 */
int setlogmask(int mask)
{
    static int old_mask = 0;
    int return_mask = old_mask;

    ulog_tag_lvl_filter_set(local_ident, mask);

    old_mask = mask;

    return return_mask;
}

static const char *get_month_str(uint8_t month)
{
    switch(month)
    {
    case 1: return "Jan";
    case 2: return "Feb";
    case 3: return "Mar";
    case 4: return "Apr";
    case 5: return "May";
    case 6: return "June";
    case 7: return "July";
    case 8: return "Aug";
    case 9: return "Sept";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
    default: return "Unknown";
    }
}

RT_WEAK rt_size_t syslog_formater(char *log_buf, int level, const char *tag, rt_bool_t newline, const char *format, va_list args)
{
    extern size_t ulog_strcpy(size_t cur_len, char *dst, const char *src);

    rt_size_t log_len = 0, newline_len = rt_strlen(ULOG_NEWLINE_SIGN);
    int fmt_result;

    RT_ASSERT(log_buf);
    RT_ASSERT(LOG_PRI(level) <= LOG_DEBUG);
    RT_ASSERT(tag);
    RT_ASSERT(format);

    /* add time and priority (level) info */
    {
        time_t now = time(RT_NULL);
        struct tm *tm, tm_tmp;

        tm = gmtime_r(&now, &tm_tmp);

#ifdef ULOG_OUTPUT_LEVEL
        rt_snprintf(log_buf + log_len, ULOG_LINE_BUF_SIZE - log_len, "<%d>%s%3d %02d:%02d:%02d", level,
                get_month_str(tm->tm_mon + 1), tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
#else
        rt_snprintf(log_buf + log_len, ULOG_LINE_BUF_SIZE - log_len, "%s%3d %02d:%02d:%02d",
                get_month_str(tm->tm_mon + 1), tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif /* ULOG_OUTPUT_LEVEL */

        log_len += rt_strlen(log_buf + log_len);
    }

#ifdef ULOG_OUTPUT_TAG
    /* add identification (tag) info */
    {
        log_len += ulog_strcpy(log_len, log_buf + log_len, " ");
        log_len += ulog_strcpy(log_len, log_buf + log_len, tag);
    }
#endif /* ULOG_OUTPUT_TAG */

#ifdef ULOG_OUTPUT_THREAD_NAME
    /* add thread info */
    {
        log_len += ulog_strcpy(log_len, log_buf + log_len, " ");
        /* is not in interrupt context */
        if (rt_interrupt_get_nest() == 0)
        {
            log_len += ulog_strcpy(log_len, log_buf + log_len, rt_thread_self()->name);
        }
        else
        {
            log_len += ulog_strcpy(log_len, log_buf + log_len, "ISR");
        }
    }
#endif /* ULOG_OUTPUT_THREAD_NAME */

    log_len += ulog_strcpy(log_len, log_buf + log_len, ": ");

#ifdef ULOG_OUTPUT_FLOAT
    fmt_result = vsnprintf(log_buf + log_len, ULOG_LINE_BUF_SIZE - log_len, format, args);
#else
    fmt_result = rt_vsnprintf(log_buf + log_len, ULOG_LINE_BUF_SIZE - log_len, format, args);
#endif /* ULOG_OUTPUT_FLOAT */

    /* calculate log length */
    if ((log_len + fmt_result <= ULOG_LINE_BUF_SIZE) && (fmt_result > -1))
    {
        log_len += fmt_result;
    }
    else
    {
        /* using max length */
        log_len = ULOG_LINE_BUF_SIZE;
    }

    /* overflow check and reserve some space for newline sign */
    if (log_len + newline_len > ULOG_LINE_BUF_SIZE)
    {
        /* using max length */
        log_len = ULOG_LINE_BUF_SIZE;
        /* reserve some space for newline sign */
        log_len -= newline_len;
    }

    /* package newline sign */
    if (newline)
    {
        log_len += ulog_strcpy(log_len, log_buf + log_len, ULOG_NEWLINE_SIGN);
    }

    return log_len;
}

#endif /* ULOG_USING_SYSLOG */
