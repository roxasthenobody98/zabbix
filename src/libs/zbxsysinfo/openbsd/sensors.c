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

#include "common.h"
#include "sysinfo.h"
#include "zbxregexp.h"
#include "log.h"

#include <sys/sensors.h>

#ifdef HAVE_SENSORDEV

static void	count_sensor(int do_task, const struct sensor *sensor, double *aggr, int *cnt)
{
	double	value = sensor->value;

	switch (sensor->type)
	{
		case SENSOR_TEMP:
			value = (value - 273150000) / 1000000;
			break;
		case SENSOR_VOLTS_DC:
		case SENSOR_VOLTS_AC:
		case SENSOR_AMPS:
		case SENSOR_LUX:
			value /= 1000000;
			break;
		case SENSOR_TIMEDELTA:
			value /= 1000000000;
			break;
		default:
			break;
	}

	(*cnt)++;

	switch (do_task)
	{
		case ZBX_DO_ONE:
			*aggr = value;
			break;
		case ZBX_DO_AVG:
			*aggr += value;
			break;
		case ZBX_DO_MAX:
			*aggr = (1 == *cnt ? value : MAX(*aggr, value));
			break;
		case ZBX_DO_MIN:
			*aggr = (1 == *cnt ? value : MIN(*aggr, value));
			break;
	}
}

static int	get_device_sensors(int do_task, int *mib, const struct sensordev *sensordev, const char *name,
		double *aggr, int *cnt, char **err_msg)
{
	if (ZBX_DO_ONE == do_task)
	{
		int		i, len = 0;
		struct sensor	sensor;
		size_t		slen = sizeof(sensor);

		for (i = 0; i < SENSOR_MAX_TYPES; i++)
		{
			if (0 == strncmp(name, sensor_type_s[i], len = strlen(sensor_type_s[i])))
				break;
		}

		if (i == SENSOR_MAX_TYPES)
		{
			*err_msg = zbx_dsprintf(*err_msg, "Sensor name \"%s\" does not match a known sensor type.",
					name);
			return FAIL;
		}

		if (SUCCEED != is_uint31(name + len, &mib[4]))
		{
			*err_msg = zbx_dsprintf(*err_msg, "Invalid number in sensor name \"%s\".", name + len);
			return FAIL;
		}

		mib[3] = i;

		if (-1 == sysctl(mib, 5, &sensor, &slen, NULL, 0))
		{
			*err_msg = zbx_dsprintf(*err_msg, "sysctl() returned error for sensor \"%s\": %s",
					name, zbx_strerror(errno));
			return FAIL;
		}

		count_sensor(do_task, &sensor, aggr, cnt);
	}
	else
	{
		char	*err_msg_local = NULL;
		int	i, j;

		for (i = 0; i < SENSOR_MAX_TYPES; i++)
		{
			for (j = 0; j < sensordev->maxnumt[i]; j++)
			{
				char		human[64];
				struct sensor	sensor;
				size_t		slen = sizeof(sensor);
				int		rc;

				zbx_snprintf(human, sizeof(human), "%s%d", sensor_type_s[i], j);

				if (ZBX_REGEXP_NO_MATCH == (rc = zbx_regexp_match2(human, name, NULL, NULL,
						&err_msg_local)))
				{
					continue;
				}
				else if (ZBX_REGEXP_COMPILE_FAIL == rc || ZBX_REGEXP_RUNTIME_FAIL == rc)
				{
					*err_msg = zbx_dsprintf(*err_msg, "%s regular expression \"%s\": %s",
							(ZBX_REGEXP_COMPILE_FAIL == rc) ? "Invalid" :
							"Error occurred while matching", name, err_msg_local);
					zbx_free(err_msg_local);
					return FAIL;
				}

				mib[3] = i;
				mib[4] = j;

				if (-1 == sysctl(mib, 5, &sensor, &slen, NULL, 0))
				{
					*err_msg = zbx_dsprintf(*err_msg, "sysctl() returned error for sensor \"%s\":"
							" %s", human, zbx_strerror(errno));
					return FAIL;
				}

				count_sensor(do_task, &sensor, aggr, cnt);
			}
		}
	}

	return SUCCEED;
}

int	GET_SENSOR(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char	*device, *name, *function, *err_msg = NULL;
	int	do_task, mib[5], dev, cnt = 0;
	double	aggr = 0;

	if (3 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	device = get_rparam(request, 0);
	name = get_rparam(request, 1);
	function = get_rparam(request, 2);

	if (NULL == device || '\0' == *device)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == name || '\0' == *name)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == function || '\0' == *function)
		do_task = ZBX_DO_ONE;
	else if (0 == strcmp(function, "avg"))
		do_task = ZBX_DO_AVG;
	else if (0 == strcmp(function, "max"))
		do_task = ZBX_DO_MAX;
	else if (0 == strcmp(function, "min"))
		do_task = ZBX_DO_MIN;
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
		return SYSINFO_RET_FAIL;
	}

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;

	for (dev = 0;; dev++)
	{
		struct sensordev	sensordev;
		size_t			sdlen = sizeof(sensordev);
		int			rc;

		mib[2] = dev;

		if (-1 == sysctl(mib, 3, &sensordev, &sdlen, NULL, 0))
		{
			if (errno == ENXIO)
				continue;
			if (errno == ENOENT)
				break;

			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain system information: %s",
					zbx_strerror(errno)));
			return SYSINFO_RET_FAIL;
		}

		if (ZBX_DO_ONE == do_task)
		{
			if (0 != strcmp(sensordev.xname, device))
				continue;
		}
		else if (ZBX_REGEXP_NO_MATCH == (rc = zbx_regexp_match2(sensordev.xname, device, NULL, NULL,
				&err_msg)))
		{
			continue;
		}
		else if (ZBX_REGEXP_COMPILE_FAIL == rc || ZBX_REGEXP_RUNTIME_FAIL == rc)
		{
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "%s regular expression \"%s\": %s",
					(ZBX_REGEXP_COMPILE_FAIL == rc) ?
					"Invalid" : "Error occurred while matching", device, err_msg));
			zbx_free(err_msg);
			return SYSINFO_RET_FAIL;
		}

		if (SUCCEED != get_device_sensors(do_task, mib, &sensordev, name, &aggr, &cnt, &err_msg))
		{
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain sensor information: %s", err_msg));
			zbx_free(err_msg);
			return SYSINFO_RET_FAIL;
		}
	}

	if (0 == cnt)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot obtain sensor information."));
		return SYSINFO_RET_FAIL;
	}

	if (ZBX_DO_AVG == do_task)
		SET_DBL_RESULT(result, aggr / cnt);
	else
		SET_DBL_RESULT(result, aggr);

	return SYSINFO_RET_OK;
}

#else

int	GET_SENSOR(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	SET_MSG_RESULT(result, zbx_strdup(NULL, "Agent was compiled without support for \"sensordev\" structure."));
	return SYSINFO_RET_FAIL;
}

#endif	/* HAVE_SENSORDEV */
