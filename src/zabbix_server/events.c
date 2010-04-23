/*
** ZABBIX
** Copyright (C) 2000-2005 SIA Zabbix
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>

#include <signal.h>

#include <string.h>

#include <time.h>

#include <sys/socket.h>
#include <errno.h>

/* Functions: pow(), round() */
#include <math.h>

#include "common.h"
#include "db.h"
#include "log.h"
#include "zlog.h"
#include "zbxserver.h"

#include "actions.h"
#include "events.h"

/******************************************************************************
 *                                                                            *
 * Function: get_latest_event_status                                          *
 *                                                                            *
 * Purpose: get identifiers and values of the last two events                 *
 *                                                                            *
 * Parameters: triggerid    - [IN] trigger identifier from database           *
 *             prev_eventid - [OUT] previous trigger identifier               *
 *             prev_value   - [OUT] previous trigger value                    *
 *             last_eventid - [OUT] last trigger identifier                   *
 *             last_value   - [OUT] last trigger value                        *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Alexei Vladishev, Alexander Vladishev                              *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
static void	get_latest_event_status(zbx_uint64_t triggerid,
		zbx_uint64_t *prev_eventid, int *prev_value,
		zbx_uint64_t *last_eventid, int *last_value)
{
	const char	*__function_name = "get_latest_event_status";
	char		sql[256];
	DB_RESULT	result;
	DB_ROW		row;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() triggerid:" ZBX_FS_UI64,
			__function_name, triggerid);

	/* Object and objectid are used for efficient sort by
	 * the same index as in wehere condition
	 */
	zbx_snprintf(sql, sizeof(sql),
			"select eventid,value"
			" from events"
			" where source=%d"
				" and object=%d"
				" and objectid=" ZBX_FS_UI64
			" order by object desc,objectid desc,eventid desc",
			EVENT_SOURCE_TRIGGERS,
			EVENT_OBJECT_TRIGGER,
			triggerid);

	result = DBselectN(sql, 2);

	if (NULL != (row = DBfetch(result)))
	{
		*prev_eventid = 0;
		*prev_value = TRIGGER_VALUE_UNKNOWN;
		ZBX_STR2UINT64(*last_eventid, row[0]);
		*last_value = atoi(row[1]);

		if (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(*prev_eventid, row[0]);
			*prev_value = atoi(row[1]);
		}
	}
	else
	{
		*prev_eventid = 0;
		*prev_value = TRIGGER_VALUE_UNKNOWN;
		*last_eventid = 0;
		*last_value = TRIGGER_VALUE_UNKNOWN;
	}
	DBfree_result(result);

	zabbix_log(LOG_LEVEL_DEBUG, "%s() prev_eventid:" ZBX_FS_UI64
			" prev_value:%d last_eventid:" ZBX_FS_UI64
			" last_value:%d",
			__function_name, *prev_eventid, *prev_value,
			*last_eventid, *last_value);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()",
			__function_name);
}

/******************************************************************************
 *                                                                            *
 * Function: add_trigger_info                                                 *
 *                                                                            *
 * Purpose: add trigger info to event if required                             *
 *                                                                            *
 * Parameters: event - event data (event.triggerid)                           *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Alexei Vladishev                                                   *
 *                                                                            *
 * Comments: use 'free_trigger_info' function to clear allocated memory       *
 *                                                                            *
 ******************************************************************************/
static void	add_trigger_info(DB_EVENT *event)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	triggerid, prev_eventid, last_eventid;
	int		prev_value, last_value;

	if (event->object == EVENT_OBJECT_TRIGGER && event->objectid != 0)
	{
		triggerid = event->objectid;

		event->trigger_description[0] = '\0';
		zbx_free(event->trigger_comments);
		zbx_free(event->trigger_url);

		result = DBselect(
				"select description,priority,comments,url,type"
				" from triggers"
				" where triggerid=" ZBX_FS_UI64,
				triggerid);

		if (NULL != (row = DBfetch(result)))
		{
			strscpy(event->trigger_description, row[0]);
			event->trigger_priority = atoi(row[1]);
			event->trigger_comments	= strdup(row[2]);
			event->trigger_url	= strdup(row[3]);
			event->trigger_type	= atoi(row[4]);
		}
		DBfree_result(result);

		/* skip actions in next cases:
		 * (1)  -any- / -any- /UNKNOWN
		 * (2)  FALSE /UNKNOWN/ FALSE
		 * (3) UNKNOWN/UNKNOWN/ FALSE
		 * if event->trigger_type is not TRIGGER_TYPE_MULTIPLE_TRUE:
		 * (4)  TRUE  /UNKNOWN/ TRUE
		 */
		if (event->value == TRIGGER_VALUE_UNKNOWN)	/* (1) */
		{
			event->skip_actions = 1;
		}
		else
		{
			get_latest_event_status(triggerid, &prev_eventid, &prev_value,
					&last_eventid, &last_value);

			if (last_value == TRIGGER_VALUE_UNKNOWN)	/* (2), (3) & (4) */
			{
				if (event->value == TRIGGER_VALUE_FALSE &&	/* (2) & (3) */
						(prev_value == TRIGGER_VALUE_FALSE || prev_value == TRIGGER_VALUE_UNKNOWN))
				{
					event->skip_actions = 1;
				}
				else if (event->trigger_type != TRIGGER_TYPE_MULTIPLE_TRUE &&	/* (4) */
						prev_value == TRIGGER_VALUE_TRUE && event->value == TRIGGER_VALUE_TRUE)
				{
					event->skip_actions = 1;
				}

				/* copy acknowledges in next cases:
				 * (1) FALSE/UNKNOWN/FALSE
				 * if event->trigger_type is not TRIGGER_TYPE_MULTIPLE_TRUE:
				 * (2) TRUE /UNKNOWN/TRUE
				 */
				if (prev_value == event->value &&
						(prev_value == TRIGGER_VALUE_FALSE ||			/* (1) */
						(event->trigger_type != TRIGGER_TYPE_MULTIPLE_TRUE &&	/* (2) */
						prev_value == TRIGGER_VALUE_TRUE)))
					event->ack_eventid = prev_eventid;
			}
		}

		if (1 == event->skip_actions)
			zabbix_log(LOG_LEVEL_DEBUG, "Skip actions");
		if (0 != event->ack_eventid)
			zabbix_log(LOG_LEVEL_DEBUG, "Copy acknowledges");
	}
}

/******************************************************************************
 *                                                                            *
 * Function: copy_acknowledges                                                *
 *                                                                            *
 * Purpose: copy acknowledges from src_eventid to dst_eventid                 *
 *                                                                            *
 * Parameters: src_eventid - [IN] source event identifier from database       *
 *             dst_eventid - [IN] destination event identifier from database  *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
static void	copy_acknowledges(zbx_uint64_t src_eventid, zbx_uint64_t dst_eventid)
{
	const char	*__function_name = "copy_acknowledges";
	zbx_uint64_t	acknowledgeid, *ids = NULL;
	int		ids_alloc = 0, ids_num = 0, i;
	DB_RESULT	result;
	DB_ROW		row;
	char		*sql = NULL;
	int		sql_alloc = 4096, sql_offset = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() src_eventid:" ZBX_FS_UI64
			" dst_eventid:" ZBX_FS_UI64,
			__function_name, src_eventid, dst_eventid);

	result = DBselect(
			"select acknowledgeid"
			" from acknowledges"
			" where eventid=" ZBX_FS_UI64,
			src_eventid);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(acknowledgeid, row[0]);
		uint64_array_add(&ids, &ids_alloc, &ids_num, acknowledgeid, 64);
	}
	DBfree_result(result);

	if (NULL == ids)
		goto out;

	sql = zbx_malloc(sql, sql_alloc * sizeof(char));

#ifdef HAVE_ORACLE
	zbx_snprintf_alloc(&sql, &sql_allocated, &sql_offset, 8, "begin\n");
#endif

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, 96,
			"update events"
			" set acknowledged=1"
			" where eventid=" ZBX_FS_UI64 ";\n",
			dst_eventid);

	acknowledgeid = DBget_maxid_num("acknowledges", "acknowledgeid", ids_num);

	for (i = 0; i < ids_num; i++, acknowledgeid++)
	{
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, 192,
				"insert into acknowledges"
				" (acknowledgeid,userid,eventid,clock,message)"
					" select " ZBX_FS_UI64 ",userid," ZBX_FS_UI64 ",clock,message"
					" from acknowledges"
					" where acknowledgeid=" ZBX_FS_UI64 ";\n",
				acknowledgeid, dst_eventid, ids[i]);
	}

#ifdef HAVE_ORACLE
	zbx_snprintf_alloc(&sql, &sql_allocated, &sql_offset, 8, "end;\n");
#endif

	if (sql_offset > 16) /* In ORACLE always present begin..end; */
		DBexecute("%s", sql);

	zbx_free(sql);
	zbx_free(ids);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
}

/******************************************************************************
 *                                                                            *
 * Function: free_trigger_info                                                *
 *                                                                            *
 * Purpose: clean allocated memory by function 'add_trigger_info'             *
 *                                                                            *
 * Parameters: event - event data (event.triggerid)                           *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
static void	free_trigger_info(DB_EVENT *event)
{
	zbx_free(event->trigger_url);
	zbx_free(event->trigger_comments);
}

/******************************************************************************
 *                                                                            *
 * Function: process_event                                                    *
 *                                                                            *
 * Purpose: process new event                                                 *
 *                                                                            *
 * Parameters: event - event data (event.eventid - new event)                 *
 *                                                                            *
 * Return value: SUCCESS - event added                                        *
 *                                                                            *
 * Author: Alexei Vladishev                                                   *
 *                                                                            *
 * Comments: Cannot use action->userid as it may also be groupid              *
 *                                                                            *
 ******************************************************************************/
int	process_event(DB_EVENT *event)
{
	const char	*__function_name = "process_event";
	int		ret = SUCCEED;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() eventid:" ZBX_FS_UI64
			" object:%d objectid:" ZBX_FS_UI64 " value:%d",
			__function_name, event->eventid, event->object,
			event->objectid, event->value);

	add_trigger_info(event);

	if (0 == event->eventid)
		event->eventid = DBget_maxid("events", "eventid");

	DBexecute("insert into events (eventid,source,object,objectid,clock,value)"
			" values (" ZBX_FS_UI64 ",%d,%d," ZBX_FS_UI64 ",%d,%d)",
			event->eventid,
			event->source,
			event->object,
			event->objectid,
			event->clock,
			event->value);

	if (0 != event->ack_eventid)
		copy_acknowledges(event->ack_eventid, event->eventid);

	if (0 == event->skip_actions)
		process_actions(event);

	if (event->object == EVENT_OBJECT_TRIGGER)
		DBupdate_services(event->objectid, (TRIGGER_VALUE_TRUE == event->value) ? event->trigger_priority : 0, event->clock);

	free_trigger_info(event);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s",
			__function_name, zbx_result_string(ret));

	return ret;
}
