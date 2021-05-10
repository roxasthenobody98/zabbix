/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include <sys/procfs.h>
#include "common.h"
#include "sysinfo.h"
#include "zbxregexp.h"
#include "log.h"

int	PROC_MEM(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	DIR		*dir;
	int		proc;
	struct dirent	*entries;
	zbx_stat_t	buf;
	struct passwd	*usrinfo;
	struct prpsinfo	psinfo;
	char		filename[MAX_STRING_LEN];
	char		*procname, *proccomm, *param, *err_msg = NULL;
	double		memsize = -1;
	int		pgsize = getpagesize();
	int		proccount = 0, invalid_user = 0, do_task;
	pid_t		curr_pid = getpid();
	zbx_regexp_t	*regx = NULL;

	if (4 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	procname = get_rparam(request, 0);
	param = get_rparam(request, 1);

	if (NULL != param && '\0' != *param)
	{
		if (NULL == (usrinfo = getpwnam(param)))
			invalid_user = 1;
	}
	else
		usrinfo = NULL;

	param = get_rparam(request, 2);

	if (NULL == param || '\0' == *param || 0 == strcmp(param, "sum"))	/* default parameter */
		do_task = ZBX_DO_SUM;
	else if (0 == strcmp(param, "avg"))
		do_task = ZBX_DO_AVG;
	else if (0 == strcmp(param, "max"))
		do_task = ZBX_DO_MAX;
	else if (0 == strcmp(param, "min"))
		do_task = ZBX_DO_MIN;
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
		return SYSINFO_RET_FAIL;
	}

	proccomm = get_rparam(request, 3);

	if (1 == invalid_user)	/* handle 0 for non-existent user after all parameters have been parsed and validated */
		goto out;

	if (NULL == (dir = opendir("/proc")))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot open /proc: %s", zbx_strerror(errno)));
		return SYSINFO_RET_FAIL;
	}

	if (NULL != proccomm && '\0' != *proccomm && SUCCEED != zbx_regexp_compile(proccomm, &regx, &err_msg))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "invalid regular expression in the fourth parameter: %s",
				err_msg));
		zbx_free(err_msg);
		closedir(dir);
		return SYSINFO_RET_FAIL;
	}

	while (NULL != (entries = readdir(dir)))
	{
		strscpy(filename, "/proc/");
		zbx_strlcat(filename, entries->d_name, MAX_STRING_LEN);

		if (0 == zbx_stat(filename, &buf))
		{
			proc = open(filename, O_RDONLY);
			if (-1 == proc)
				goto skip_process;

			if (-1 == ioctl(proc, PIOCPSINFO, &psinfo))
				goto skip_process;

			/* Self process information. It leads to incorrect results for proc.mem[zabbix_agentd]. */
			if (psinfo.pr_pid == curr_pid)
				goto skip_process;

			if (NULL != procname && '\0' != *procname)
				if (0 == strcmp(procname, psinfo.pr_fname))
					goto skip_process;

			if (NULL != usrinfo)
				if (usrinfo->pw_uid != psinfo.pr_uid)
					goto skip_process;

			if (NULL != regx)
			{
				int	rc;

				if (ZBX_REGEXP_NO_MATCH == (rc = zbx_regexp_match_precompiled(psinfo.pr_psargs, regx,
						&err_msg)))
				{
					goto skip_process;
				}

				if (ZBX_REGEXP_RUNTIME_FAIL == rc)
				{
					SET_MSG_RESULT(result, zbx_dsprintf(NULL, "error occurred while matching"
							" regular expression in the fourth parameter: %s", err_msg));
					zbx_free(err_msg);
					close(proc);
					zbx_regexp_free(regx);
					closedir(dir);
					return SYSINFO_RET_FAIL;
				}
			}

			proccount++;

			if (0 > memsize) /* first initialization */
			{
				memsize = (double)(psinfo.pr_rssize * pgsize);
			}
			else
			{
				if (ZBX_DO_MAX == do_task)
					memsize = MAX(memsize, (double)(psinfo.pr_rssize * pgsize));
				else if (ZBX_DO_MIN == do_task)
					memsize = MIN(memsize, (double)(psinfo.pr_rssize * pgsize));
				else	/* SUM */
					memsize += (double)(psinfo.pr_rssize * pgsize);
			}
skip_process:
			if (-1 != proc)
				close(proc);
		}
	}

	if (NULL != regx)
		zbx_regexp_free(regx);

	closedir(dir);

	if (0 > memsize)
	{
		/* incorrect process name */
		memsize = 0;
	}
out:
	if (ZBX_DO_AVG == do_task)
		SET_DBL_RESULT(result, 0 == proccount ? 0 : memsize / (double)proccount);
	else
		SET_UI64_RESULT(result, memsize);

	return SYSINFO_RET_OK;
}

int	PROC_NUM(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	DIR		*dir;
	int		proc;
	struct  dirent	*entries;
	zbx_stat_t	buf;
	struct passwd	*usrinfo;
	struct prpsinfo	psinfo;
	char		filename[MAX_STRING_LEN];
	char		*procname, *proccomm, *param, *err_msg = NULL;
	int		proccount = 0, invalid_user = 0, zbx_proc_stat;
	pid_t		curr_pid = getpid();
	zbx_regexp_t	*regx = NULL;

	if (4 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	procname = get_rparam(request, 0);
	param = get_rparam(request, 1);

	if (NULL != param && '\0' != *param)
	{
		if (NULL == (usrinfo = getpwnam(param)))
			invalid_user = 1;
	}
	else
		usrinfo = NULL;

	param = get_rparam(request, 2);

	if (NULL == param || '\0' == *param || 0 == strcmp(param, "all"))
		zbx_proc_stat = -1;
	else if (0 == strcmp(param, "run"))
		zbx_proc_stat = PR_SRUN;
	else if (0 == strcmp(param, "sleep"))
		zbx_proc_stat = PR_SSLEEP;
	else if (0 == strcmp(param, "zomb"))
		zbx_proc_stat = PR_SZOMB;
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
		return SYSINFO_RET_FAIL;
	}

	proccomm = get_rparam(request, 3);

	if (1 == invalid_user)	/* handle 0 for non-existent user after all parameters have been parsed and validated */
		goto out;

	if (NULL == (dir = opendir("/proc")))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot open /proc: %s", zbx_strerror(errno)));
		return SYSINFO_RET_FAIL;
	}

	if (NULL != proccomm && '\0' != *proccomm && SUCCEED != zbx_regexp_compile(proccomm, &regx, &err_msg))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "invalid regular expression in the fourth parameter: %s",
				err_msg));
		zbx_free(err_msg);
		closedir(dir);
		return SYSINFO_RET_FAIL;
	}

	while (NULL != (entries = readdir(dir)))
	{
		strscpy(filename, "/proc/");
		zbx_strlcat(filename, entries->d_name,MAX_STRING_LEN);

		if (0 == zbx_stat(filename, &buf))
		{
			proc = open(filename, O_RDONLY);
			if (-1 == proc)
				goto skip_process;

			if (-1 == ioctl(proc, PIOCPSINFO, &psinfo))
				goto skip_process;

			/* Self process information. It leads to incorrect results for proc.num[zabbix_agentd]. */
			if (psinfo.pr_pid == curr_pid)
				goto skip_process;

			if (NULL != procname && '\0' != *procname)
				if (0 != strcmp(procname, psinfo.pr_fname))
					goto skip_process;

			if (NULL != usrinfo)
				if (usrinfo->pw_uid != psinfo.pr_uid)
					goto skip_process;

			if (-1 != zbx_proc_stat)
				if (psinfo.pr_sname != zbx_proc_stat)
					goto skip_process;

			if (NULL != regx)
			{
				int	rc;

				if (ZBX_REGEXP_NO_MATCH == (rc = zbx_regexp_match_precompiled(psinfo.pr_psargs, regx,
						&err_msg)))
				{
					goto skip_process;
				}

				if (ZBX_REGEXP_RUNTIME_FAIL == rc)
				{
					SET_MSG_RESULT(result, zbx_dsprintf(NULL, "error occurred while matching"
							" regular expression in the fourth parameter: %s", err_msg));
					zbx_free(err_msg);
					close(proc);
					zbx_regexp_free(regx);
					closedir(dir);
					return SYSINFO_RET_FAIL;
				}
			}

			proccount++;
skip_process:
			if (-1 != proc)
				close(proc);
		}
	}

	if (NULL != regx)
		zbx_regexp_free(regx);

	closedir(dir);
out:
	SET_UI64_RESULT(result, proccount);

	return SYSINFO_RET_OK;
}
