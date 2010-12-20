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

#include "common.h"

#include "db.h"
#include "log.h"
#include "dbcache.h"

#include "httpmacro.h"
#include "httptest.h"

#ifdef HAVE_LIBCURL

static ZBX_HTTPPAGE	page;

/******************************************************************************
 *                                                                            *
 * Function: process_value                                                    *
 *                                                                            *
 * Purpose: process new item value                                            *
 *                                                                            *
 * Parameters: key - item key                                                 *
 *             host - host name                                               *
 *             value - new value of the item                                  *
 *                                                                            *
 * Return value: SUCCEED - new value successfully processed                   *
 *               FAIL - otherwise                                             *
 *                                                                            *
 * Author: Alexei Vladishev                                                   *
 *                                                                            *
 * Comments: can be done in process_data()                                    *
 *                                                                            *
 ******************************************************************************/
static int	process_value(zbx_uint64_t itemid, AGENT_RESULT *value)
{
	DB_RESULT	result;
	DB_ROW		row;
	unsigned char	value_type;

	zabbix_log(LOG_LEVEL_DEBUG, "In process_value(itemid:" ZBX_FS_UI64 ")",
			itemid);

	result = DBselect(
			"select i.itemid,i.value_type"
			" from items i,hosts h"
			" where h.hostid=i.hostid"
				" and h.status=%d"
				" and i.status=%d"
				" and i.type=%d"
				" and i.itemid=" ZBX_FS_UI64
				" and (h.maintenance_status=%d or h.maintenance_type=%d)"
				DB_NODE,
			HOST_STATUS_MONITORED,
			ITEM_STATUS_ACTIVE,
			ITEM_TYPE_HTTPTEST,
			itemid,
			HOST_MAINTENANCE_STATUS_OFF, MAINTENANCE_TYPE_NORMAL,
			DBnode_local("h.hostid"));

	if (NULL == (row = DBfetch(result)))
	{
		DBfree_result(result);
		zabbix_log(LOG_LEVEL_DEBUG, "End process_value(result:FAIL)");
		return FAIL;
	}

	value_type = (unsigned char)atoi(row[1]);

	dc_add_history(itemid, value_type, value, time(NULL), 0, NULL, 0, 0, 0, 0);

	DBfree_result(result);

	zabbix_log(LOG_LEVEL_DEBUG, "End process_value()");

	return SUCCEED;
}

static size_t	WRITEFUNCTION2(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t	r_size = size * nmemb;

	/* first piece of data */
	if (NULL == page.data)
	{
		page.allocated = MAX(8096, r_size);
		page.offset = 0;
		page.data = zbx_malloc(page.data, page.allocated);
	}

	zbx_snprintf_alloc(&page.data, &page.allocated, &page.offset, MAX(8096, r_size), "%s", ptr);

	return r_size;
}

static size_t	HEADERFUNCTION2(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

static void	process_test_data(DB_HTTPTEST *httptest, ZBX_HTTPSTAT *stat)
{
	DB_RESULT	result;
	DB_ROW		row;
	DB_HTTPTESTITEM	httptestitem;

	AGENT_RESULT    value;

	zabbix_log(LOG_LEVEL_DEBUG, "In process_test_data(test:%s,time:" ZBX_FS_DBL ",last step:%d)",
		 httptest->name,
		stat->test_total_time,
		stat->test_last_step);

	result = DBselect("select httptestitemid,httptestid,itemid,type from httptestitem where httptestid=" ZBX_FS_UI64,
		httptest->httptestid);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(httptestitem.httptestitemid, row[0]);
		ZBX_STR2UINT64(httptestitem.httptestid, row[1]);
		ZBX_STR2UINT64(httptestitem.itemid, row[2]);
		httptestitem.type=atoi(row[3]);

		init_result(&value);

		switch (httptestitem.type)
		{
			case ZBX_HTTPITEM_TYPE_TIME:
				SET_DBL_RESULT(&value, stat->test_total_time);
				process_value(httptestitem.itemid, &value);
				break;
			case ZBX_HTTPITEM_TYPE_LASTSTEP:
				SET_UI64_RESULT(&value, stat->test_last_step);
				process_value(httptestitem.itemid, &value);
				break;
			case ZBX_HTTPITEM_TYPE_SPEED:
				SET_UI64_RESULT(&value, stat->speed_download);
				process_value(httptestitem.itemid, &value);
				break;
			default:
				break;
		}

		free_result(&value);
	}

	DBfree_result(result);
	zabbix_log(LOG_LEVEL_DEBUG, "End process_test_data()");
}


static void	process_step_data(DB_HTTPTEST *httptest, DB_HTTPSTEP *httpstep, ZBX_HTTPSTAT *stat)
{
	DB_RESULT	result;
	DB_ROW		row;
	DB_HTTPSTEPITEM	httpstepitem;

	AGENT_RESULT    value;

	zabbix_log(LOG_LEVEL_DEBUG, "In process_step_data(step:%s,url:%s,rsp:%d,time:" ZBX_FS_DBL ",speed:" ZBX_FS_DBL ")",
		httpstep->name,
		httpstep->url,
		stat->rspcode,
		stat->total_time,
		stat->speed_download);

	result = DBselect("select httpstepitemid,httpstepid,itemid,type from httpstepitem where httpstepid=" ZBX_FS_UI64,
		httpstep->httpstepid);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(httpstepitem.httpstepitemid, row[0]);
		ZBX_STR2UINT64(httpstepitem.httpstepid, row[1]);
		ZBX_STR2UINT64(httpstepitem.itemid, row[2]);
		httpstepitem.type=atoi(row[3]);

		init_result(&value);

		switch (httpstepitem.type)
		{
			case ZBX_HTTPITEM_TYPE_RSPCODE:
				SET_UI64_RESULT(&value, stat->rspcode);
				process_value(httpstepitem.itemid, &value);
				break;
			case ZBX_HTTPITEM_TYPE_TIME:
				SET_DBL_RESULT(&value, stat->total_time);
				process_value(httpstepitem.itemid, &value);
				break;
			case ZBX_HTTPITEM_TYPE_SPEED:
				SET_DBL_RESULT(&value, stat->speed_download);
				process_value(httpstepitem.itemid, &value);
				break;
			default:
				break;
		}

		free_result(&value);
	}

	DBfree_result(result);
	zabbix_log(LOG_LEVEL_DEBUG, "End process_step_data()");
}

/******************************************************************************
 *                                                                            *
 * Function: process_httptest                                                 *
 *                                                                            *
 * Purpose: process single scenario of http test                              *
 *                                                                            *
 * Parameters: httptestid - ID of http test                                   *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Alexei Vladishev                                                   *
 *                                                                            *
 * Comments: SUCCEED or FAIL                                                  *
 *                                                                            *
 ******************************************************************************/
static void	process_httptest(DB_HTTPTEST *httptest)
{
	const char	*__function_name = "process_httptest";

	DB_RESULT	result;
	DB_ROW		row;
	DB_HTTPSTEP	httpstep;
	int		err;
	char		*err_str = NULL, *esc_err_str = NULL;
	char		auth[HTTPTEST_HTTP_USER_LEN_MAX + HTTPTEST_HTTP_PASSWORD_LEN_MAX];
	int		opt, now;
	int		lastfailedstep;

	ZBX_HTTPSTAT	stat;
	double		speed_download = 0;
	int		speed_download_num = 0;

	CURL            *easyhandle = NULL;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() httptestid:" ZBX_FS_UI64 " name:'%s'",
			__function_name, httptest->httptestid, httptest->name);

	now = time(NULL);

	DBexecute("update httptest"
			" set lastcheck=%d,"
				"nextcheck=%d+delay"
			" where httptestid=" ZBX_FS_UI64,
			now, now, httptest->httptestid);

	if (NULL == (easyhandle = curl_easy_init()))
	{
		zabbix_log(LOG_LEVEL_ERR, "Cannot init cURL");
		return;
	}

	if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_COOKIEFILE, "")) ||
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_USERAGENT, httptest->agent)) ||
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_FOLLOWLOCATION, 1)) ||
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_WRITEFUNCTION, WRITEFUNCTION2)) ||
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_HEADERFUNCTION, HEADERFUNCTION2)) ||
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_SSL_VERIFYPEER, 0)) ||
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_SSL_VERIFYHOST, 0)) ||
			/* The pointed data are not copied by the library. As a consequence, the data */
			/* must be preserved by the calling application until the transfer finishes. */
			CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_POSTFIELDS, httpstep.posts)))
	{
		zabbix_log(LOG_LEVEL_ERR, "Cannot set cURL option %d: [%s]", opt, curl_easy_strerror(err));
		curl_easy_cleanup(easyhandle);
		return;
	}

	lastfailedstep = 0;
	httptest->time = 0;

	result = DBselect(
			"select httpstepid,no,name,url,timeout,"
				"posts,required,status_codes"
			" from httpstep"
			" where httptestid=" ZBX_FS_UI64
			" order by no",
			httptest->httptestid);

	now = time(NULL);

	while (NULL == err_str && NULL != (row = DBfetch(result)))
	{
		/* NOTE: do not use break or return for this block!
		 *       process_step_data calling required!
		 */
		ZBX_STR2UINT64(httpstep.httpstepid, row[0]);
		httpstep.httptestid = httptest->httptestid;
		httpstep.no = atoi(row[1]);
		httpstep.name = row[2];
		strscpy(httpstep.url, row[3]);
		httpstep.timeout = atoi(row[4]);
		strscpy(httpstep.posts, row[5]);
		strscpy(httpstep.required, row[6]);
		strscpy(httpstep.status_codes, row[7]);

		DBexecute("update httptest"
				" set curstep=%d,"
					"curstate=%d"
				" where httptestid=" ZBX_FS_UI64,
				httpstep.no,
				HTTPTEST_STATE_BUSY,
				httptest->httptestid);

		memset(&stat, 0, sizeof(stat));

		/* Substitute macros */
		http_substitute_macros(httptest, httpstep.url, sizeof(httpstep.url));
		http_substitute_macros(httptest, httpstep.posts, sizeof(httpstep.posts));

		zabbix_log(LOG_LEVEL_DEBUG, "WEBMonitor: use step [%s]", httpstep.name);

		if ('\0' != *httpstep.posts)
		{
			zabbix_log(LOG_LEVEL_DEBUG, "WEBMonitor: use post [%s]", httpstep.posts);
			curl_easy_setopt(easyhandle, CURLOPT_POST, 1);
		}
		else
			curl_easy_setopt(easyhandle, CURLOPT_POST, 0);

		if (httptest->authentication != HTTPTEST_AUTH_NONE)
		{
			long	curlauth = 0;

			zabbix_log(LOG_LEVEL_DEBUG, "WEBMonitor: Setting HTTPAUTH [%d]", httptest->authentication);
			zabbix_log(LOG_LEVEL_DEBUG, "WEBMonitor: Setting USERPWD for authentication");

			switch (httptest->authentication)
			{
				case HTTPTEST_AUTH_BASIC:
					curlauth = CURLAUTH_BASIC;
					break;
				case HTTPTEST_AUTH_NTLM:
					curlauth = CURLAUTH_NTLM;
					break;
				default:
					THIS_SHOULD_NEVER_HAPPEN;
					break;
			}

			zbx_snprintf(auth, sizeof(auth), "%s:%s", httptest->http_user, httptest->http_password);

			if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_HTTPAUTH, curlauth)) ||
					CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_USERPWD, auth)))
			{
				zabbix_log(LOG_LEVEL_ERR, "Cannot set cURL option %d: [%s]", opt, curl_easy_strerror(err));
				err_str = zbx_strdup(err_str, curl_easy_strerror(err));
			}
		}

		if (NULL == err_str)
		{
			zabbix_log(LOG_LEVEL_DEBUG, "WEBMonitor: go to URL [%s]", httpstep.url);

			if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_URL, httpstep.url)) ||
					CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_TIMEOUT, httpstep.timeout)) ||
					CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_CONNECTTIMEOUT, httpstep.timeout)))
			{
				zabbix_log(LOG_LEVEL_ERR, "Cannot set cURL option %d: [%s]", opt, curl_easy_strerror(err));
				err_str = zbx_strdup(err_str, curl_easy_strerror(err));
			}
		}

		if (NULL == err_str)
		{
			memset(&page, 0, sizeof(page));

			if (CURLE_OK != (err = curl_easy_perform(easyhandle)))
			{
				zabbix_log(LOG_LEVEL_ERR, "Error doing curl_easy_perform [%s]", curl_easy_strerror(err));
				err_str = zbx_strdup(err_str, curl_easy_strerror(err));
			}
			else
			{
				if ('\0' != httpstep.required[0] && NULL == zbx_regexp_match(page.data, httpstep.required, NULL))
				{
					zabbix_log(LOG_LEVEL_DEBUG, "Page did not match [%s]", httpstep.required);
					err_str = zbx_strdup(err_str, "Page did not match");
				}

				if (CURLE_OK != (err = curl_easy_getinfo(easyhandle, CURLINFO_RESPONSE_CODE, &stat.rspcode)))
				{
					zabbix_log(LOG_LEVEL_ERR, "Error getting CURLINFO_RESPONSE_CODE [%s]", curl_easy_strerror(err));
					err_str = (NULL == err_str ? zbx_strdup(err_str, curl_easy_strerror(err)) : err_str);
				}
				else if ('\0' != httpstep.status_codes[0] && FAIL == int_in_list(httpstep.status_codes, stat.rspcode))
				{
					zabbix_log(LOG_LEVEL_DEBUG, "Status code did not match [%s]", httpstep.status_codes);
					err_str = (NULL == err_str ? zbx_strdup(err_str, "Status code did not match") : err_str);
				}

				if (CURLE_OK != (err = curl_easy_getinfo(easyhandle, CURLINFO_TOTAL_TIME, &stat.total_time)))
				{
					zabbix_log(LOG_LEVEL_ERR, "Error getting CURLINFO_TOTAL_TIME [%s]", curl_easy_strerror(err));
					err_str = (NULL == err_str ? zbx_strdup(err_str, curl_easy_strerror(err)) : err_str);
				}

				if (CURLE_OK != (err = curl_easy_getinfo(easyhandle, CURLINFO_SPEED_DOWNLOAD, &stat.speed_download)))
				{
					zabbix_log(LOG_LEVEL_ERR, "Error getting CURLINFO_SPEED_DOWNLOAD [%s]", curl_easy_strerror(err));
					err_str = (NULL == err_str ? zbx_strdup(err_str, curl_easy_strerror(err)) : err_str);
				}
				else
				{
					speed_download += stat.speed_download;
					speed_download_num++;
				}
			}

			zbx_free(page.data);
		}

		if (NULL != err_str)
			lastfailedstep = httpstep.no;

		httptest->time += stat.total_time;
		process_step_data(httptest, &httpstep, &stat);
	}
	DBfree_result(result);

	esc_err_str = DBdyn_escape_string_len(err_str, HTTPTEST_ERROR_LEN);
	zbx_free(err_str);

	curl_easy_cleanup(easyhandle);

	DBexecute("update httptest"
			" set curstep=0,"
				"curstate=%d,"
				"lastcheck=%d,"
				"nextcheck=%d+delay,"
				"lastfailedstep=%d,"
				"time=" ZBX_FS_DBL ","
				"error='%s'"
			" where httptestid=" ZBX_FS_UI64,
			HTTPTEST_STATE_IDLE,
			now, now,
			lastfailedstep,
			httptest->time,
			esc_err_str,
			httptest->httptestid);

	zbx_free(esc_err_str);

	stat.test_total_time = httptest->time;
	stat.test_last_step = lastfailedstep;
	stat.speed_download = speed_download_num ? speed_download / speed_download_num : 0;

	process_test_data(httptest, &stat);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s() total_time:" ZBX_FS_DBL,
			__function_name, httptest->time);
}

/******************************************************************************
 *                                                                            *
 * Function: process_httptests                                                *
 *                                                                            *
 * Purpose: process httptests                                                 *
 *                                                                            *
 * Parameters: now - current timestamp                                        *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Alexei Vladishev                                                   *
 *                                                                            *
 * Comments: always SUCCEED                                                   *
 *                                                                            *
 ******************************************************************************/
void	process_httptests(int now)
{
	const char	*__function_name = "process_httptests";

	DB_RESULT	result;
	DB_ROW		row;

	DB_HTTPTEST	httptest;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	result = DBselect(
			"select t.httptestid,t.name,t.applicationid,t.nextcheck,t.status,t.delay,"
				"t.macros,t.agent,t.authentication,t.http_user,t.http_password"
			" from httptest t,applications a,hosts h"
			" where t.applicationid=a.applicationid"
				" and a.hostid=h.hostid"
				" and t.nextcheck<=%d"
				" and " ZBX_SQL_MOD(t.httptestid,%d) "=%d"
				" and t.status=%d"
				" and h.status=%d"
				" and (h.maintenance_status=%d or h.maintenance_type=%d)"
				DB_NODE,
			now,
			CONFIG_HTTPPOLLER_FORKS, httppoller_num - 1,
			HTTPTEST_STATUS_MONITORED,
			HOST_STATUS_MONITORED,
			HOST_MAINTENANCE_STATUS_OFF, MAINTENANCE_TYPE_NORMAL,
			DBnode_local("t.httptestid"));

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(httptest.httptestid, row[0]);
		httptest.name		= row[1];
		ZBX_STR2UINT64(httptest.applicationid, row[2]);
		httptest.nextcheck	= atoi(row[3]);
		httptest.status		= atoi(row[4]);
		httptest.delay		= atoi(row[5]);
		httptest.macros		= row[6];
		httptest.agent		= row[7];
		httptest.authentication	= atoi(row[8]);
		httptest.http_user	= row[9];
		httptest.http_password	= row[10];

		process_httptest(&httptest);
	}

	DBfree_result(result);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
}

#endif	/* HAVE_LIBCURL */
