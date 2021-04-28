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

#include "db.h"
#include "log.h"
#include "dbcache.h"
#include "zbxserver.h"
#include "../../libs/zbxaudit/audit.h"
#include "../../libs/zbxalgo/vectorimpl.h"

static char	*get_template_names(const zbx_vector_uint64_t *templateids)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		*sql = NULL, *template_names = NULL;
	size_t		sql_alloc = 256, sql_offset=0, tmp_alloc = 64, tmp_offset = 0;

	sql = (char *)zbx_malloc(sql, sql_alloc);
	template_names = (char *)zbx_malloc(template_names, tmp_alloc);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select host"
			" from hosts"
			" where");

	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
			templateids->values, templateids->values_num);

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
		zbx_snprintf_alloc(&template_names, &tmp_alloc, &tmp_offset, "\"%s\", ", row[0]);

	template_names[tmp_offset - 2] = '\0';

	DBfree_result(result);
	zbx_free(sql);

	return template_names;
}

/******************************************************************************
 *                                                                            *
 * Function: DBget_profiles_by_source_idxs_values                             *
 *                                                                            *
 * Description: gets a vector of profile identifiers used with the specified  *
 *              source, indexes and value identifiers                         *
 *                                                                            *
 * Parameters: profileids - [OUT] the screen item identifiers                 *
 *             source     - [IN] the source                                   *
 *             idxs       - [IN] an array of index values                     *
 *             idxs_num   - [IN] the number of values in idxs array           *
 *             value_ids  - [IN] the resource identifiers                     *
 *                                                                            *
 ******************************************************************************/
static void	DBget_profiles_by_source_idxs_values(zbx_vector_uint64_t *profileids, const char *source,
		const char **idxs, int idxs_num, zbx_vector_uint64_t *value_ids)
{
	char	*sql = NULL;
	size_t	sql_alloc = 0, sql_offset = 0;

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select distinct profileid from profiles where");

	if (NULL != source)
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, " source='%s' and", source);

	if (0 != idxs_num)
	{
		DBadd_str_condition_alloc(&sql, &sql_alloc, &sql_offset, "idx", idxs, idxs_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " and");
	}

	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "value_id", value_ids->values, value_ids->values_num);

	DBselect_uint64(sql, profileids);

	zbx_free(sql);

	zbx_vector_uint64_sort(profileids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

/******************************************************************************
 *                                                                            *
 * Function: DBget_sysmapelements_by_element_type_ids                         *
 *                                                                            *
 * Description: gets a vector of sysmap element identifiers used with the     *
 *              specified element type and identifiers                        *
 *                                                                            *
 * Parameters: selementids - [OUT] the sysmap element identifiers             *
 *             elementtype - [IN] the element type                            *
 *             elementids  - [IN] the element identifiers                     *
 *                                                                            *
 ******************************************************************************/
static void	DBget_sysmapelements_by_element_type_ids(zbx_vector_uint64_t *selementids, int elementtype,
		zbx_vector_uint64_t *elementids)
{
	char	*sql = NULL;
	size_t	sql_alloc = 0, sql_offset = 0;

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct selementid"
			" from sysmaps_elements"
			" where elementtype=%d"
				" and",
			elementtype);

	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "elementid", elementids->values, elementids->values_num);
	DBselect_uint64(sql, selementids);

	zbx_free(sql);

	zbx_vector_uint64_sort(selementids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

/******************************************************************************
 *                                                                            *
 * Function: validate_linked_templates                                        *
 *                                                                            *
 * Description: Check collisions between linked templates                     *
 *                                                                            *
 * Parameters: templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Return value: SUCCEED if no collisions found                               *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	validate_linked_templates(const zbx_vector_uint64_t *templateids, char *error, size_t max_error_len)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		*sql = NULL;
	size_t		sql_alloc = 256, sql_offset;
	int		ret = SUCCEED;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	if (0 == templateids->values_num)
		goto out;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	/* items */
	if (SUCCEED == ret && 1 < templateids->values_num)
	{
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select key_,count(*)"
				" from items"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
				templateids->values, templateids->values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				" group by key_"
				" having count(*)>1");

		result = DBselectN(sql, 1);

		if (NULL != (row = DBfetch(result)))
		{
			ret = FAIL;
			zbx_snprintf(error, max_error_len, "conflicting item key \"%s\" found", row[0]);
		}
		DBfree_result(result);
	}

	/* trigger expressions */
	if (SUCCEED == ret)
	{
		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select t1.description,h2.host"
				" from items i1,functions f1,triggers t1,functions f2,items i2,hosts h2"
				" where i1.itemid=f1.itemid"
					" and f1.triggerid=t1.triggerid"
					" and t1.triggerid=f2.triggerid"
					" and f2.itemid=i2.itemid"
					" and i2.hostid=h2.hostid"
					" and h2.status=%d"
					" and",
				HOST_STATUS_TEMPLATE);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i1.hostid",
				templateids->values, templateids->values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " and not");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i2.hostid",
				templateids->values, templateids->values_num);

		result = DBselectN(sql, 1);

		if (NULL != (row = DBfetch(result)))
		{
			ret = FAIL;
			zbx_snprintf(error, max_error_len,
					"trigger \"%s\" has items from template \"%s\"",
					row[0], row[1]);
		}
		DBfree_result(result);
	}

	/* trigger dependencies */
	if (SUCCEED == ret)
	{
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				/* don't remove "description2 and host2" aliases, the ORACLE needs them */
				"select t1.description,h1.host,t2.description as description2,h2.host as host2"
				" from trigger_depends td,triggers t1,functions f1,items i1,hosts h1,"
					"triggers t2,functions f2,items i2,hosts h2"
				" where td.triggerid_down=t1.triggerid"
					" and t1.triggerid=f1.triggerid"
					" and f1.itemid=i1.itemid"
					" and i1.hostid=h1.hostid"
					" and td.triggerid_up=t2.triggerid"
					" and t2.triggerid=f2.triggerid"
					" and f2.itemid=i2.itemid"
					" and i2.hostid=h2.hostid"
					" and");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i1.hostid",
				templateids->values, templateids->values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " and not");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i2.hostid",
				templateids->values, templateids->values_num);
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, " and h2.status=%d", HOST_STATUS_TEMPLATE);

		result = DBselectN(sql, 1);

		if (NULL != (row = DBfetch(result)))
		{
			ret = FAIL;
			zbx_snprintf(error, max_error_len,
					"trigger \"%s\" in template \"%s\""
					" has dependency from trigger \"%s\" in template \"%s\"",
					row[0], row[1], row[2], row[3]);
		}
		DBfree_result(result);
	}

	/* graphs */
	if (SUCCEED == ret && 1 < templateids->values_num)
	{
		zbx_vector_uint64_t	graphids;

		zbx_vector_uint64_create(&graphids);

		/* select all linked graphs */
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select distinct gi.graphid"
				" from graphs_items gi,items i"
				" where gi.itemid=i.itemid"
					" and");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i.hostid",
				templateids->values, templateids->values_num);

		DBselect_uint64(sql, &graphids);

		/* check for names */
		if (0 != graphids.values_num)
		{
			sql_offset = 0;
			zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
					"select name,count(*)"
					" from graphs"
					" where");
			DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "graphid",
					graphids.values, graphids.values_num);
			zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
					" group by name"
					" having count(*)>1");

			result = DBselect("%s", sql);

			if (NULL != (row = DBfetch(result)))
			{
				ret = FAIL;
				zbx_snprintf(error, max_error_len,
						"template with graph \"%s\" already linked to the host", row[0]);
			}
			DBfree_result(result);
		}

		zbx_vector_uint64_destroy(&graphids);
	}

	/* httptests */
	if (SUCCEED == ret && 1 < templateids->values_num)
	{
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select name,count(*)"
				" from httptest"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
				templateids->values, templateids->values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				" group by name"
				" having count(*)>1");

		result = DBselectN(sql, 1);

		if (NULL != (row = DBfetch(result)))
		{
			ret = FAIL;
			zbx_snprintf(error, max_error_len,
					"template with web scenario \"%s\" already linked to the host", row[0]);
		}
		DBfree_result(result);
	}

	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: DBcmp_triggers                                                   *
 *                                                                            *
 * Purpose: compare two triggers                                              *
 *                                                                            *
 * Parameters: triggerid1 - first trigger identificator from database         *
 *             triggerid2 - second trigger identificator from database        *
 *                                                                            *
 * Return value: SUCCEED - if triggers coincide                               *
 *                                                                            *
 ******************************************************************************/
static int	DBcmp_triggers(zbx_uint64_t triggerid1, const char *expression1, const char *recovery_expression1,
		zbx_uint64_t triggerid2, const char *expression2, const char *recovery_expression2)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		search[MAX_ID_LEN + 3], replace[MAX_ID_LEN + 3], *old_expr = NULL, *expr = NULL, *rexpr = NULL;
	int		res = SUCCEED;

	expr = zbx_strdup(NULL, expression2);
	rexpr = zbx_strdup(NULL, recovery_expression2);

	result = DBselect(
			"select f1.functionid,f2.functionid"
			" from functions f1,functions f2,items i1,items i2"
			" where f1.name=f2.name"
				" and f1.parameter=f2.parameter"
				" and i1.key_=i2.key_"
				" and i1.itemid=f1.itemid"
				" and i2.itemid=f2.itemid"
				" and f1.triggerid=" ZBX_FS_UI64
				" and f2.triggerid=" ZBX_FS_UI64,
				triggerid1, triggerid2);

	while (NULL != (row = DBfetch(result)))
	{
		zbx_snprintf(search, sizeof(search), "{%s}", row[1]);
		zbx_snprintf(replace, sizeof(replace), "{%s}", row[0]);

		old_expr = expr;
		expr = string_replace(expr, search, replace);
		zbx_free(old_expr);

		old_expr = rexpr;
		rexpr = string_replace(rexpr, search, replace);
		zbx_free(old_expr);
	}
	DBfree_result(result);

	if (0 != strcmp(expression1, expr) || 0 != strcmp(recovery_expression1, rexpr))
		res = FAIL;

	zbx_free(rexpr);
	zbx_free(expr);

	return res;
}

/******************************************************************************
 *                                                                            *
 * Function: validate_inventory_links                                         *
 *                                                                            *
 * Description: Check collisions in item inventory links                      *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Return value: SUCCEED if no collisions found                               *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	validate_inventory_links(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids,
		char *error, size_t max_error_len)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		*sql = NULL;
	size_t		sql_alloc = 512, sql_offset;
	int		ret = SUCCEED;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select inventory_link,count(*)"
			" from items"
			" where inventory_link<>0"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
			templateids->values, templateids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			" group by inventory_link"
			" having count(*)>1");

	result = DBselectN(sql, 1);

	if (NULL != (row = DBfetch(result)))
	{
		ret = FAIL;
		zbx_strlcpy(error, "two items cannot populate one host inventory field", max_error_len);
	}
	DBfree_result(result);

	if (FAIL == ret)
		goto out;

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select ti.itemid"
			" from items ti,items i"
			" where ti.key_<>i.key_"
				" and ti.inventory_link=i.inventory_link"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "ti.hostid",
			templateids->values, templateids->values_num);
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				" and i.hostid=" ZBX_FS_UI64
				" and ti.inventory_link<>0"
				" and not exists ("
					"select *"
					" from items",
				hostid);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "items.hostid",
			templateids->values, templateids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
						" and items.key_=i.key_"
					")");

	result = DBselectN(sql, 1);

	if (NULL != (row = DBfetch(result)))
	{
		ret = FAIL;
		zbx_strlcpy(error, "two items cannot populate one host inventory field", max_error_len);
	}
	DBfree_result(result);
out:
	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: validate_httptests                                               *
 *                                                                            *
 * Description: checking collisions on linking of web scenarios               *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Return value: SUCCEED if no collisions found                               *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	validate_httptests(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids,
		char *error, size_t max_error_len)
{
	DB_RESULT	tresult;
	DB_RESULT	sresult;
	DB_ROW		trow;
	char		*sql = NULL;
	size_t		sql_alloc = 512, sql_offset = 0;
	int		ret = SUCCEED;
	zbx_uint64_t	t_httptestid, h_httptestid;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	/* selects web scenarios from templates and host with identical names */
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select t.httptestid,t.name,h.httptestid"
			" from httptest t"
				" inner join httptest h"
					" on h.name=t.name"
						" and h.hostid=" ZBX_FS_UI64
			" where", hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.hostid", templateids->values, templateids->values_num);

	tresult = DBselect("%s", sql);

	while (NULL != (trow = DBfetch(tresult)))
	{
		ZBX_STR2UINT64(t_httptestid, trow[0]);
		ZBX_STR2UINT64(h_httptestid, trow[2]);

		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				/* don't remove "h_httpstepid" alias, the ORACLE needs it */
				"select t.httpstepid,h.httpstepid as h_httpstepid"
				" from httpstep t"
					" left join httpstep h"
						" on h.httptestid=" ZBX_FS_UI64
							" and h.no=t.no"
							" and h.name=t.name"
				" where t.httptestid=" ZBX_FS_UI64
					" and h.httpstepid is null"
				" union "
				"select t.httpstepid,h.httpstepid as h_httpstepid"
				" from httpstep h"
					" left outer join httpstep t"
						" on t.httptestid=" ZBX_FS_UI64
							" and t.no=h.no"
							" and t.name=h.name"
				" where h.httptestid=" ZBX_FS_UI64
					" and t.httpstepid is null",
				h_httptestid, t_httptestid, t_httptestid, h_httptestid);

		sresult = DBselectN(sql, 1);

		if (NULL != DBfetch(sresult))
		{
			ret = FAIL;
			zbx_snprintf(error, max_error_len,
					"web scenario \"%s\" already exists on the host (steps are not identical)",
					trow[1]);
		}
		DBfree_result(sresult);

		if (SUCCEED != ret)
			break;
	}
	DBfree_result(tresult);

	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return ret;
}

static void	DBget_graphitems(const char *sql, ZBX_GRAPH_ITEMS **gitems, size_t *gitems_alloc, size_t *gitems_num)
{
	DB_RESULT	result;
	DB_ROW		row;
	ZBX_GRAPH_ITEMS	*gitem;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	*gitems_num = 0;

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		if (*gitems_alloc == *gitems_num)
		{
			*gitems_alloc += 16;
			*gitems = (ZBX_GRAPH_ITEMS *)zbx_realloc(*gitems, *gitems_alloc * sizeof(ZBX_GRAPH_ITEMS));
		}

		gitem = &(*gitems)[*gitems_num];

		ZBX_STR2UINT64(gitem->gitemid, row[0]);
		ZBX_STR2UINT64(gitem->itemid, row[1]);
		zbx_strlcpy(gitem->key, row[2], sizeof(gitem->key));
		gitem->drawtype = atoi(row[3]);
		gitem->sortorder = atoi(row[4]);
		zbx_strlcpy(gitem->color, row[5], sizeof(gitem->color));
		gitem->yaxisside = atoi(row[6]);
		gitem->calc_fnc = atoi(row[7]);
		gitem->type = atoi(row[8]);
		gitem->flags = (unsigned char)atoi(row[9]);

		zabbix_log(LOG_LEVEL_DEBUG, "%s() [" ZBX_FS_SIZE_T "] itemid:" ZBX_FS_UI64 " key:'%s'",
				__func__, (zbx_fs_size_t)*gitems_num, gitem->itemid, gitem->key);

		(*gitems_num)++;
	}
	DBfree_result(result);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBcmp_graphitems                                                 *
 *                                                                            *
 * Purpose: Compare graph items from two graphs                               *
 *                                                                            *
 * Parameters: gitems1     - [IN] first graph items, sorted by itemid         *
 *             gitems1_num - [IN] number of first graph items                 *
 *             gitems2     - [IN] second graph items, sorted by itemid        *
 *             gitems2_num - [IN] number of second graph items                *
 *                                                                            *
 * Return value: SUCCEED if graph items coincide                              *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	DBcmp_graphitems(ZBX_GRAPH_ITEMS *gitems1, int gitems1_num,
		ZBX_GRAPH_ITEMS *gitems2, int gitems2_num)
{
	int	res = FAIL, i;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	if (gitems1_num != gitems2_num)
		goto clean;

	for (i = 0; i < gitems1_num; i++)
		if (0 != strcmp(gitems1[i].key, gitems2[i].key))
			goto clean;

	res = SUCCEED;
clean:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(res));

	return res;
}

/******************************************************************************
 *                                                                            *
 * Function: validate_host                                                    *
 *                                                                            *
 * Description: Check collisions between host and linked template             *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Return value: SUCCEED if no collisions found                               *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	validate_host(zbx_uint64_t hostid, zbx_vector_uint64_t *templateids, char *error, size_t max_error_len)
{
	DB_RESULT	tresult;
	DB_RESULT	hresult;
	DB_ROW		trow;
	DB_ROW		hrow;
	char		*sql = NULL, *name_esc;
	size_t		sql_alloc = 256, sql_offset;
	ZBX_GRAPH_ITEMS	*gitems = NULL, *chd_gitems = NULL;
	size_t		gitems_alloc = 0, gitems_num = 0,
			chd_gitems_alloc = 0, chd_gitems_num = 0;
	int		ret = SUCCEED, i;
	zbx_uint64_t	graphid, interfaceids[INTERFACE_TYPE_COUNT];
	unsigned char	t_flags, h_flags, type;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	if (SUCCEED != (ret = validate_inventory_links(hostid, templateids, error, max_error_len)))
		goto out;

	if (SUCCEED != (ret = validate_httptests(hostid, templateids, error, max_error_len)))
		goto out;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct g.graphid,g.name,g.flags"
			" from graphs g,graphs_items gi,items i"
			" where g.graphid=gi.graphid"
				" and gi.itemid=i.itemid"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i.hostid", templateids->values, templateids->values_num);

	tresult = DBselect("%s", sql);

	while (SUCCEED == ret && NULL != (trow = DBfetch(tresult)))
	{
		ZBX_STR2UINT64(graphid, trow[0]);
		t_flags = (unsigned char)atoi(trow[2]);

		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select 0,0,i.key_,gi.drawtype,gi.sortorder,gi.color,gi.yaxisside,gi.calc_fnc,"
					"gi.type,i.flags"
				" from graphs_items gi,items i"
				" where gi.itemid=i.itemid"
					" and gi.graphid=" ZBX_FS_UI64
				" order by i.key_",
				graphid);

		DBget_graphitems(sql, &gitems, &gitems_alloc, &gitems_num);

		name_esc = DBdyn_escape_string(trow[1]);

		hresult = DBselect(
				"select distinct g.graphid,g.flags"
				" from graphs g,graphs_items gi,items i"
				" where g.graphid=gi.graphid"
					" and gi.itemid=i.itemid"
					" and i.hostid=" ZBX_FS_UI64
					" and g.name='%s'"
					" and g.templateid is null",
				hostid, name_esc);

		zbx_free(name_esc);

		/* compare graphs */
		while (NULL != (hrow = DBfetch(hresult)))
		{
			ZBX_STR2UINT64(graphid, hrow[0]);
			h_flags = (unsigned char)atoi(hrow[1]);

			if (t_flags != h_flags)
			{
				ret = FAIL;
				zbx_snprintf(error, max_error_len,
						"graph prototype and real graph \"%s\" have the same name", trow[1]);
				break;
			}

			sql_offset = 0;
			zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
					"select gi.gitemid,i.itemid,i.key_,gi.drawtype,gi.sortorder,gi.color,"
						"gi.yaxisside,gi.calc_fnc,gi.type,i.flags"
					" from graphs_items gi,items i"
					" where gi.itemid=i.itemid"
						" and gi.graphid=" ZBX_FS_UI64
					" order by i.key_",
					graphid);

			DBget_graphitems(sql, &chd_gitems, &chd_gitems_alloc, &chd_gitems_num);

			if (SUCCEED != DBcmp_graphitems(gitems, gitems_num, chd_gitems, chd_gitems_num))
			{
				ret = FAIL;
				zbx_snprintf(error, max_error_len,
						"graph \"%s\" already exists on the host (items are not identical)",
						trow[1]);
				break;
			}
		}
		DBfree_result(hresult);
	}
	DBfree_result(tresult);

	if (SUCCEED == ret)
	{
		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select i.key_"
				" from items i,items t"
				" where i.key_=t.key_"
					" and i.flags<>t.flags"
					" and i.hostid=" ZBX_FS_UI64
					" and",
				hostid);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.hostid",
				templateids->values, templateids->values_num);

		tresult = DBselectN(sql, 1);

		if (NULL != (trow = DBfetch(tresult)))
		{
			ret = FAIL;
			zbx_snprintf(error, max_error_len,
					"item prototype and real item \"%s\" have the same key", trow[0]);
		}
		DBfree_result(tresult);
	}

	/* interfaces */
	if (SUCCEED == ret)
	{
		memset(&interfaceids, 0, sizeof(interfaceids));

		tresult = DBselect(
				"select type,interfaceid"
				" from interface"
				" where hostid=" ZBX_FS_UI64
					" and type in (%d,%d,%d,%d)"
					" and main=1",
				hostid, INTERFACE_TYPE_AGENT, INTERFACE_TYPE_SNMP,
				INTERFACE_TYPE_IPMI, INTERFACE_TYPE_JMX);

		while (NULL != (trow = DBfetch(tresult)))
		{
			type = (unsigned char)atoi(trow[0]);
			ZBX_STR2UINT64(interfaceids[type - 1], trow[1]);
		}
		DBfree_result(tresult);

		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select distinct type"
				" from items"
				" where type not in (%d,%d,%d,%d,%d,%d,%d,%d)"
					" and",
				ITEM_TYPE_TRAPPER, ITEM_TYPE_INTERNAL, ITEM_TYPE_ZABBIX_ACTIVE, ITEM_TYPE_AGGREGATE,
				ITEM_TYPE_HTTPTEST, ITEM_TYPE_DB_MONITOR, ITEM_TYPE_CALCULATED, ITEM_TYPE_DEPENDENT);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
				templateids->values, templateids->values_num);

		tresult = DBselect("%s", sql);

		while (SUCCEED == ret && NULL != (trow = DBfetch(tresult)))
		{
			type = (unsigned char)atoi(trow[0]);
			type = get_interface_type_by_item_type(type);

			if (INTERFACE_TYPE_ANY == type)
			{
				for (i = 0; INTERFACE_TYPE_COUNT > i; i++)
				{
					if (0 != interfaceids[i])
						break;
				}

				if (INTERFACE_TYPE_COUNT == i)
				{
					zbx_strlcpy(error, "cannot find any interfaces on host", max_error_len);
					ret = FAIL;
				}
			}
			else if (0 == interfaceids[type - 1])
			{
				zbx_snprintf(error, max_error_len, "cannot find \"%s\" host interface",
						zbx_interface_type_string((zbx_interface_type_t)type));
				ret = FAIL;
			}
		}
		DBfree_result(tresult);
	}

	zbx_free(sql);
	zbx_free(gitems);
	zbx_free(chd_gitems);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_action_conditions                                       *
 *                                                                            *
 * Purpose: delete action conditions by condition type and id                 *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_action_conditions(int conditiontype, zbx_uint64_t elementid)
{
	DB_RESULT		result;
	DB_ROW			row;
	zbx_uint64_t		id;
	zbx_vector_uint64_t	actionids, conditionids;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;

	zbx_vector_uint64_create(&actionids);
	zbx_vector_uint64_create(&conditionids);

	/* disable actions */
	result = DBselect("select actionid,conditionid from conditions where conditiontype=%d and"
			" value='" ZBX_FS_UI64 "'", conditiontype, elementid);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(id, row[0]);
		zbx_vector_uint64_append(&actionids, id);

		ZBX_STR2UINT64(id, row[1]);
		zbx_vector_uint64_append(&conditionids, id);
	}

	DBfree_result(result);

	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	if (0 != actionids.values_num)
	{
		zbx_vector_uint64_sort(&actionids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
		zbx_vector_uint64_uniq(&actionids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, "update actions set status=%d where",
				ACTION_STATUS_DISABLED);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "actionid", actionids.values,
				actionids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	if (0 != conditionids.values_num)
	{
		zbx_vector_uint64_sort(&conditionids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from conditions where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "conditionid", conditionids.values,
				conditionids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	/* in ORACLE always present begin..end; */
	if (16 < sql_offset)
		DBexecute("%s", sql);

	zbx_free(sql);

	zbx_vector_uint64_destroy(&conditionids);
	zbx_vector_uint64_destroy(&actionids);
}

/******************************************************************************
 *                                                                            *
 * Function: DBadd_to_housekeeper                                             *
 *                                                                            *
 * Purpose:  adds table and field with specific id to housekeeper list        *
 *                                                                            *
 * Parameters: ids       - [IN] identificators for data removal               *
 *             field     - [IN] field name from table                         *
 *             tables_hk - [IN] table name to delete information from         *
 *             count     - [IN] number of tables in tables array              *
 *                                                                            *
 * Author: Eugene Grigorjev, Alexander Vladishev                              *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBadd_to_housekeeper(zbx_vector_uint64_t *ids, const char *field, const char **tables_hk, int count)
{
	int		i, j;
	zbx_uint64_t	housekeeperid;
	zbx_db_insert_t	db_insert;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, ids->values_num);

	if (0 == ids->values_num)
		goto out;

	housekeeperid = DBget_maxid_num("housekeeper", count * ids->values_num);

	zbx_db_insert_prepare(&db_insert, "housekeeper", "housekeeperid", "tablename", "field", "value", NULL);

	for (i = 0; i < ids->values_num; i++)
	{
		for (j = 0; j < count; j++)
			zbx_db_insert_add_values(&db_insert, housekeeperid++, tables_hk[j], field, ids->values[i]);
	}

	zbx_db_insert_execute(&db_insert);
	zbx_db_insert_clean(&db_insert);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_triggers                                                *
 *                                                                            *
 * Purpose: delete trigger from database                                      *
 *                                                                            *
 * Parameters: triggerids - [IN] trigger identificators from database         *
 *                                                                            *
 ******************************************************************************/
void	DBdelete_triggers(zbx_vector_uint64_t *triggerids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset;
	int			i;
	zbx_vector_uint64_t	selementids;
	const char		*event_tables[] = {"events"};

	if (0 == triggerids->values_num)
		return;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_vector_uint64_create(&selementids);

	DBremove_triggers_from_itservices(triggerids->values, triggerids->values_num);

	sql_offset = 0;
	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	DBget_sysmapelements_by_element_type_ids(&selementids, SYSMAP_ELEMENT_TYPE_TRIGGER, triggerids);
	if (0 != selementids.values_num)
	{
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from sysmaps_elements where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "selementid", selementids.values,
				selementids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	for (i = 0; i < triggerids->values_num; i++)
		DBdelete_action_conditions(CONDITION_TYPE_TRIGGER, triggerids->values[i]);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"delete from triggers"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "triggerid", triggerids->values, triggerids->values_num);
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, ";\n");

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	DBexecute("%s", sql);

	/* add housekeeper task to delete problems associated with trigger, this allows old events to be deleted */
	DBadd_to_housekeeper(triggerids, "triggerid", event_tables, ARRSIZE(event_tables));

	zbx_vector_uint64_destroy(&selementids);

	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_trigger_hierarchy                                       *
 *                                                                            *
 * Purpose: delete parent triggers and auto-created children from database    *
 *                                                                            *
 * Parameters: triggerids - [IN] trigger identificators from database         *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_trigger_hierarchy(zbx_vector_uint64_t *triggerids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset = 0;
	zbx_vector_uint64_t	children_triggerids;

	if (0 == triggerids->values_num)
		return;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_vector_uint64_create(&children_triggerids);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select distinct triggerid from trigger_discovery where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "parent_triggerid", triggerids->values,
			triggerids->values_num);

	DBselect_uint64(sql, &children_triggerids);
	zbx_vector_uint64_setdiff(triggerids, &children_triggerids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	DBdelete_triggers(&children_triggerids);
	DBdelete_triggers(triggerids);

	zbx_vector_uint64_destroy(&children_triggerids);

	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_triggers_by_itemids                                     *
 *                                                                            *
 * Purpose: delete triggers by itemid                                         *
 *                                                                            *
 * Parameters: itemids - [IN] item identificators from database               *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_triggers_by_itemids(zbx_vector_uint64_t *itemids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	triggerids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, itemids->values_num);

	if (0 == itemids->values_num)
		goto out;

	zbx_vector_uint64_create(&triggerids);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select distinct triggerid from functions where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "itemid", itemids->values, itemids->values_num);

	DBselect_uint64(sql, &triggerids);

	DBdelete_trigger_hierarchy(&triggerids);
	zbx_vector_uint64_destroy(&triggerids);
	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_graphs                                                  *
 *                                                                            *
 * Purpose: delete graph from database                                        *
 *                                                                            *
 * Parameters: graphids - [IN] array of graph id's from database              *
 *                                                                            *
 ******************************************************************************/
void	DBdelete_graphs(zbx_vector_uint64_t *graphids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset = 0;
	zbx_vector_uint64_t	profileids;
	const char		*profile_idx =  "web.favorite.graphids";

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, graphids->values_num);

	if (0 == graphids->values_num)
		goto out;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_vector_uint64_create(&profileids);

	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	/* delete from profiles */
	DBget_profiles_by_source_idxs_values(&profileids, "graphid", &profile_idx, 1, graphids);
	if (0 != profileids.values_num)
	{
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from profiles where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "profileid", profileids.values,
				profileids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	/* delete from graphs */
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from graphs where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "graphid", graphids->values, graphids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	DBexecute("%s", sql);

	zbx_vector_uint64_destroy(&profileids);

	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_graph_hierarchy                                         *
 *                                                                            *
 * Purpose: delete parent graphs and auto-created children from database      *
 *                                                                            *
 * Parameters: graphids - [IN] array of graph id's from database              *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_graph_hierarchy(zbx_vector_uint64_t *graphids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset = 0;
	zbx_vector_uint64_t	children_graphids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	if (0 == graphids->values_num)
		return;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_vector_uint64_create(&children_graphids);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select distinct gd.graphid, g.name, g.flags from "
			"graph_discovery gd, graphs g where g.graphid=gd.graphid and ");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "parent_graphid", graphids->values,
			graphids->values_num);

	DBselect_delete_for_graph(sql, &children_graphids);
	zbx_vector_uint64_setdiff(graphids, &children_graphids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	DBdelete_graphs(&children_graphids);
	DBdelete_graphs(graphids);

	zbx_vector_uint64_destroy(&children_graphids);

	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_graphs_by_itemids                                       *
 *                                                                            *
 * Parameters: itemids - [IN] item identificators from database               *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_graphs_by_itemids(zbx_vector_uint64_t *itemids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset;
	DB_RESULT		result;
	DB_ROW			row;
	zbx_uint64_t		graphid;
	zbx_vector_uint64_t	graphids;
	int			index;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, itemids->values_num);

	if (0 == itemids->values_num)
		goto out;

	sql = (char *)zbx_malloc(sql, sql_alloc);
	zbx_vector_uint64_create(&graphids);

	/* select all graphs with items */
	sql_offset = 0;
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, "select distinct graphid from graphs_items where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "itemid", itemids->values, itemids->values_num);

	DBselect_delete_for_graph(sql, &graphids);

	if (0 == graphids.values_num)
		goto clean;

	/* select graphs with other items */
	sql_offset = 0;
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct graphid"
			" from graphs_items"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "graphid", graphids.values, graphids.values_num);
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, " and not");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "itemid", itemids->values, itemids->values_num);
	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(graphid, row[0]);
		if (FAIL != (index = zbx_vector_uint64_bsearch(&graphids, graphid, ZBX_DEFAULT_UINT64_COMPARE_FUNC)))
			zbx_vector_uint64_remove(&graphids, index);
	}
	DBfree_result(result);

	DBdelete_graph_hierarchy(&graphids);
clean:
	zbx_vector_uint64_destroy(&graphids);
	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_items                                                   *
 *                                                                            *
 * Purpose: delete items from database                                        *
 *                                                                            *
 * Parameters: itemids - [IN] array of item identificators from database      *
 *                                                                            *
 ******************************************************************************/
void	DBdelete_items(zbx_vector_uint64_t *itemids, int resource_type)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset;
	zbx_vector_uint64_t	profileids;
	int			num;
	const char		*history_tables[] = {"history", "history_str", "history_uint", "history_log",
				"history_text", "trends", "trends_uint"};
	const char		*event_tables[] = {"events"};
	const char		*profile_idx = "web.favorite.graphids";

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, itemids->values_num);

	if (0 == itemids->values_num)
		goto out;

	sql = (char *)zbx_malloc(sql, sql_alloc);
	zbx_vector_uint64_create(&profileids);

	do	/* add child items (auto-created and prototypes) */
	{
		num = itemids->values_num;
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select distinct id.itemid,i.name,i.flags "
				"from item_discovery id, items i where id.itemid=i.itemid and ");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "parent_itemid",
				itemids->values, itemids->values_num);

		DBselect_delete_for_item(sql, itemids, resource_type);
		zbx_vector_uint64_uniq(itemids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
	}
	while (num != itemids->values_num);

	DBdelete_graphs_by_itemids(itemids);
	DBdelete_triggers_by_itemids(itemids);

	DBadd_to_housekeeper(itemids, "itemid", history_tables, ARRSIZE(history_tables));

	/* add housekeeper task to delete problems associated with item, this allows old events to be deleted */
	DBadd_to_housekeeper(itemids, "itemid", event_tables, ARRSIZE(event_tables));
	DBadd_to_housekeeper(itemids, "lldruleid", event_tables, ARRSIZE(event_tables));

	sql_offset = 0;
	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	/* delete from profiles */
	DBget_profiles_by_source_idxs_values(&profileids, "itemid", &profile_idx, 1, itemids);
	if (0 != profileids.values_num)
	{
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from profiles where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "profileid", profileids.values,
				profileids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	/* delete from items */
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from items where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "itemid", itemids->values, itemids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	DBexecute("%s", sql);

	zbx_vector_uint64_destroy(&profileids);

	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_httptests                                               *
 *                                                                            *
 * Purpose: delete web tests from database                                    *
 *                                                                            *
 * Parameters: httptestids - [IN] array of httptest id's from database        *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_httptests(zbx_vector_uint64_t *httptestids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset = 0;
	zbx_vector_uint64_t	itemids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, httptestids->values_num);

	if (0 == httptestids->values_num)
		goto out;

	sql = (char *)zbx_malloc(sql, sql_alloc);
	zbx_vector_uint64_create(&itemids);

	/* httpstepitem, httptestitem */
	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hsi.itemid,i.name,i.flags"
			" from httpstepitem hsi,httpstep hs,items i"
			" where hsi.httpstepid=hs.httpstepid and i.itemid=hsi.itemid"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hs.httptestid",
			httptestids->values, httptestids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			" union all "
			"select ht.itemid,i.name,i.flags"
			" from httptestitem ht,items i"
			" where ht.itemid=i.itemid and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "httptestid",
			httptestids->values, httptestids->values_num);

	DBselect_delete_for_item(sql, &itemids, AUDIT_RESOURCE_SCENARIO);
	DBdelete_items(&itemids, AUDIT_RESOURCE_SCENARIO);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from httptest where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "httptestid",
			httptestids->values, httptestids->values_num);
	DBexecute("%s", sql);

	zbx_vector_uint64_destroy(&itemids);
	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBgroup_prototypes_delete                                        *
 *                                                                            *
 * Parameters: del_group_prototypeids - [IN] list of group_prototypeids which *
 *                                      will be deleted                       *
 *                                                                            *
 ******************************************************************************/
static void	DBgroup_prototypes_delete(zbx_vector_uint64_t *del_group_prototypeids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset;
	zbx_vector_uint64_t	groupids;

	if (0 == del_group_prototypeids->values_num)
		return;

	zbx_vector_uint64_create(&groupids);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select groupid from group_discovery where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "parent_group_prototypeid",
			del_group_prototypeids->values, del_group_prototypeids->values_num);

	DBselect_uint64(sql, &groupids);

	DBdelete_groups(&groupids);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from group_prototype where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "group_prototypeid",
			del_group_prototypeids->values, del_group_prototypeids->values_num);

	DBexecute("%s", sql);

	zbx_vector_uint64_destroy(&groupids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_host_prototypes                                         *
 *                                                                            *
 * Purpose: deletes host prototypes from database                             *
 *                                                                            *
 * Parameters: host_prototypeids - [IN] list of host prototypes               *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_host_prototypes(zbx_vector_uint64_t *host_prototypeids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset;
	zbx_vector_uint64_t	hostids, group_prototypeids;

	if (0 == host_prototypeids->values_num)
		return;

	/* delete discovered hosts */

	zbx_vector_uint64_create(&hostids);
	zbx_vector_uint64_create(&group_prototypeids);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select hostid from host_discovery where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "parent_hostid",
			host_prototypeids->values, host_prototypeids->values_num);

	DBselect_uint64(sql, &hostids);

	if (0 != hostids.values_num)
		DBdelete_hosts(&hostids);

	/* delete group prototypes */

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "select group_prototypeid from group_prototype where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
			host_prototypeids->values, host_prototypeids->values_num);

	DBselect_uint64(sql, &group_prototypeids);

	DBgroup_prototypes_delete(&group_prototypeids);

	/* delete host prototypes */

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from hosts where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid",
			host_prototypeids->values, host_prototypeids->values_num);

	DBexecute("%s", sql);

	zbx_vector_uint64_destroy(&group_prototypeids);
	zbx_vector_uint64_destroy(&hostids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_template_httptests                                      *
 *                                                                            *
 * Purpose: delete template web scenatios from host                           *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_template_httptests(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	httptestids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&httptestids);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select h.httptestid"
			" from httptest h"
				" join httptest t"
					" on");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.hostid", templateids->values, templateids->values_num);
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
						" and t.httptestid=h.templateid"
			" where h.hostid=" ZBX_FS_UI64, hostid);

	DBselect_uint64(sql, &httptestids);

	DBdelete_httptests(&httptestids);

	zbx_vector_uint64_destroy(&httptestids);
	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_template_graphs                                         *
 *                                                                            *
 * Purpose: delete template graphs from host                                  *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_template_graphs(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	graphids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&graphids);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct gi.graphid, g.name, g.flags"
			" from graphs_items gi,items i,items ti, graphs g"
			" where gi.itemid=i.itemid"
				" and i.templateid=ti.itemid"
				" and g.graphid=gi.graphid"
				" and i.hostid=" ZBX_FS_UI64
				" and",
			hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "ti.hostid", templateids->values, templateids->values_num);

	DBselect_delete_for_graph(sql, &graphids);

	DBdelete_graph_hierarchy(&graphids);

	zbx_vector_uint64_destroy(&graphids);
	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_template_triggers                                       *
 *                                                                            *
 * Purpose: delete template triggers from host                                *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_template_triggers(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	triggerids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&triggerids);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct f.triggerid"
			" from functions f,items i,items ti"
			" where f.itemid=i.itemid"
				" and i.templateid=ti.itemid"
				" and i.hostid=" ZBX_FS_UI64
				" and",
			hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "ti.hostid", templateids->values, templateids->values_num);

	DBselect_uint64(sql, &triggerids);

	DBdelete_trigger_hierarchy(&triggerids);
	zbx_vector_uint64_destroy(&triggerids);
	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_template_host_prototypes                                *
 *                                                                            *
 * Purpose: delete template host prototypes from host                         *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_template_host_prototypes(zbx_uint64_t hostid, zbx_vector_uint64_t *templateids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	host_prototypeids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&host_prototypeids);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select hp.hostid"
			" from items hi,host_discovery hhd,hosts hp,host_discovery thd,items ti"
			" where hi.itemid=hhd.parent_itemid"
				" and hhd.hostid=hp.hostid"
				" and hp.templateid=thd.hostid"
				" and thd.parent_itemid=ti.itemid"
				" and hi.hostid=" ZBX_FS_UI64
				" and",
			hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "ti.hostid", templateids->values, templateids->values_num);

	DBselect_uint64(sql, &host_prototypeids);

	DBdelete_host_prototypes(&host_prototypeids);

	zbx_free(sql);

	zbx_vector_uint64_destroy(&host_prototypeids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_template_items                                          *
 *                                                                            *
 * Purpose: delete template items from host                                   *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_template_items(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	itemids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&itemids);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct i.itemid,i.name,i.flags"
			" from items i,items ti"
			" where i.templateid=ti.itemid"
				" and i.hostid=" ZBX_FS_UI64
				" and",
			hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "ti.hostid", templateids->values, templateids->values_num);

	DBselect_delete_for_item(sql, &itemids, AUDIT_RESOURCE_ITEM);
	DBdelete_items(&itemids, AUDIT_RESOURCE_ITEM);

	zbx_vector_uint64_destroy(&itemids);
	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_trigger_to_host                                           *
 *                                                                            *
 * Purpose: copy specified trigger to host                                    *
 *                                                                            *
 * Parameters: new_triggerid - [OUT] id of new trigger created based on       *
 *                                   template trigger                         *
 *             cur_triggerid - [OUT] id of existing trigger that was linked   *
 *                                   to the template trigger                  *
 *             hostid - host identificator from database                      *
 *             triggerid - trigger identificator from database                *
 *             description - trigger description                              *
 *             expression - trigger expression                                *
 *             recovery_expression - trigger recovery expression              *
 *             recovery_mode - trigger recovery mode                          *
 *             status - trigger status                                        *
 *             type - trigger type                                            *
 *             priority - trigger priority                                    *
 *             comments - trigger comments                                    *
 *             url - trigger url                                              *
 *                                                                            *
 * Return value: upon successful completion return SUCCEED                    *
 *                                                                            *
 ******************************************************************************/
static int	DBcopy_trigger_to_host(zbx_uint64_t *new_triggerid, zbx_uint64_t *cur_triggerid, zbx_uint64_t hostid,
		zbx_uint64_t triggerid, const char *description, const char *expression,
		const char *recovery_expression, unsigned char recovery_mode, unsigned char status, unsigned char type,
		unsigned char priority, const char *comments, const char *url, unsigned char flags,
		unsigned char correlation_mode, const char *correlation_tag, unsigned char manual_close,
		const char *opdata, unsigned char discover, const char *event_name)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		*sql = NULL;
	size_t		sql_alloc = 256, sql_offset = 0;
	zbx_uint64_t	itemid,	h_triggerid, functionid;
	char		*old_expression = NULL,
			*new_expression = NULL,
			*expression_esc = NULL,
			*new_recovery_expression = NULL,
			*recovery_expression_esc = NULL,
			search[MAX_ID_LEN + 3],
			replace[MAX_ID_LEN + 3],
			*description_esc = NULL,
			*comments_esc = NULL,
			*url_esc = NULL,
			*function_esc = NULL,
			*parameter_esc = NULL,
			*correlation_tag_esc,
			*opdata_esc, *event_name_esc;
	int		res = FAIL;

	sql = (char *)zbx_malloc(sql, sql_alloc);

	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	description_esc = DBdyn_escape_string(description);
	correlation_tag_esc = DBdyn_escape_string(correlation_tag);
	opdata_esc = DBdyn_escape_string(opdata);
	event_name_esc = DBdyn_escape_string(event_name);

	result = DBselect(
			"select distinct t.triggerid,t.expression,t.recovery_expression"
			" from triggers t,functions f,items i"
			" where t.triggerid=f.triggerid"
				" and f.itemid=i.itemid"
				" and t.templateid is null"
				" and i.hostid=" ZBX_FS_UI64
				" and t.description='%s'",
			hostid, description_esc);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(h_triggerid, row[0]);

		if (SUCCEED != DBcmp_triggers(triggerid, expression, recovery_expression,
				h_triggerid, row[1], row[2]))
			continue;

		/* link not linked trigger with same description and expression */
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"update triggers"
				" set templateid=" ZBX_FS_UI64
					",flags=%d"
					",recovery_mode=%d"
					",correlation_mode=%d"
					",correlation_tag='%s'"
					",manual_close=%d"
					",opdata='%s'"
					",discover=%d"
					",event_name='%s'"
				" where triggerid=" ZBX_FS_UI64 ";\n",
				triggerid, (int)flags, (int)recovery_mode, (int)correlation_mode, correlation_tag_esc,
				(int)manual_close, opdata_esc, (int)discover, event_name_esc, h_triggerid);

		*new_triggerid = 0;
		*cur_triggerid = h_triggerid;

		res = SUCCEED;
		break;
	}
	DBfree_result(result);

	/* create trigger if no updated triggers */
	if (SUCCEED != res)
	{
		res = SUCCEED;

		*new_triggerid = DBget_maxid("triggers");
		*cur_triggerid = 0;
		new_expression = zbx_strdup(NULL, expression);
		new_recovery_expression = zbx_strdup(NULL, recovery_expression);

		comments_esc = DBdyn_escape_string(comments);
		url_esc = DBdyn_escape_string(url);

		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"insert into triggers"
					" (triggerid,description,priority,status,"
						"comments,url,type,value,state,templateid,flags,recovery_mode,"
						"correlation_mode,correlation_tag,manual_close,opdata,discover,"
						"event_name)"
					" values (" ZBX_FS_UI64 ",'%s',%d,%d,"
						"'%s','%s',%d,%d,%d," ZBX_FS_UI64 ",%d,%d,"
						"%d,'%s',%d,'%s',%d,'%s');\n",
					*new_triggerid, description_esc, (int)priority, (int)status, comments_esc,
					url_esc, (int)type, TRIGGER_VALUE_OK, TRIGGER_STATE_NORMAL, triggerid,
					(int)flags, (int)recovery_mode, (int)correlation_mode, correlation_tag_esc,
					(int)manual_close, opdata_esc, (int)discover, event_name_esc);

		zbx_audit_triggers_create_entry(AUDIT_ACTION_ADD, *new_triggerid, description_esc, triggerid,
				recovery_mode, status, type, TRIGGER_VALUE_OK, TRIGGER_STATE_NORMAL, priority,
				comments_esc, url_esc, flags, correlation_mode, correlation_tag_esc, manual_close,
				opdata_esc, discover, event_name_esc);

		zbx_free(url_esc);
		zbx_free(comments_esc);

		/* Loop: functions */
		result = DBselect(
				"select hi.itemid,tf.functionid,tf.name,tf.parameter,ti.key_"
				" from functions tf,items ti"
				" left join items hi"
					" on hi.key_=ti.key_"
						" and hi.hostid=" ZBX_FS_UI64
				" where tf.itemid=ti.itemid"
					" and tf.triggerid=" ZBX_FS_UI64,
				hostid, triggerid);

		while (SUCCEED == res && NULL != (row = DBfetch(result)))
		{
			if (SUCCEED != DBis_null(row[0]))
			{
				ZBX_STR2UINT64(itemid, row[0]);

				functionid = DBget_maxid("functions");

				zbx_snprintf(search, sizeof(search), "{%s}", row[1]);
				zbx_snprintf(replace, sizeof(replace), "{" ZBX_FS_UI64 "}", functionid);

				function_esc = DBdyn_escape_string(row[2]);
				parameter_esc = DBdyn_escape_string(row[3]);

				zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
						"insert into functions"
						" (functionid,itemid,triggerid,name,parameter)"
						" values (" ZBX_FS_UI64 "," ZBX_FS_UI64 ","
							ZBX_FS_UI64 ",'%s','%s');\n",
						functionid, itemid, *new_triggerid,
						function_esc, parameter_esc);

				old_expression = new_expression;
				new_expression = string_replace(new_expression, search, replace);
				zbx_free(old_expression);

				old_expression = new_recovery_expression;
				new_recovery_expression = string_replace(new_recovery_expression, search, replace);
				zbx_free(old_expression);

				zbx_free(parameter_esc);
				zbx_free(function_esc);
			}
			else
			{
				zabbix_log(LOG_LEVEL_DEBUG, "Missing similar key '%s'"
						" for host [" ZBX_FS_UI64 "]",
						row[4], hostid);
				res = FAIL;
			}
		}
		DBfree_result(result);

		if (SUCCEED == res)
		{
			char	audit_key_expression[100];
			char	audit_key_recovery_expression[100];

			expression_esc = DBdyn_escape_field("triggers", "expression", new_expression);
			recovery_expression_esc = DBdyn_escape_field("triggers", "recovery_expression",
					new_recovery_expression);

			zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
					"update triggers"
						" set expression='%s',recovery_expression='%s'"
					" where triggerid=" ZBX_FS_UI64 ";\n",
					expression_esc, recovery_expression_esc, *new_triggerid);

			if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
			{
				zbx_snprintf(audit_key_expression, 100, "trigger.expression");
				zbx_snprintf(audit_key_recovery_expression, 100, "trigger.recovery_expression");
				zbx_audit_update_json_string(*new_triggerid, audit_key_expression,
						new_expression);
				zbx_audit_update_json_string(*new_triggerid, audit_key_recovery_expression,
						new_recovery_expression);
			}
			else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			{
				zbx_snprintf(audit_key_expression, 100, "triggerprototype.expression");
				zbx_snprintf(audit_key_recovery_expression, 100,
						"triggerprototype.recovery_expression");
				zbx_audit_update_json_string(*new_triggerid, audit_key_expression,
						new_expression);
				zbx_audit_update_json_string(*new_triggerid, audit_key_recovery_expression,
						new_recovery_expression);
			}

			zbx_free(recovery_expression_esc);
			zbx_free(expression_esc);
		}

		zbx_free(new_recovery_expression);
		zbx_free(new_expression);
	}

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	if (sql_offset > 16)	/* In ORACLE always present begin..end; */
		DBexecute("%s", sql);

	zbx_free(sql);
	zbx_free(correlation_tag_esc);
	zbx_free(event_name_esc);
	zbx_free(opdata_esc);
	zbx_free(description_esc);

	return res;
}

/******************************************************************************
 *                                                                            *
 * Function: DBresolve_template_trigger_dependencies                          *
 *                                                                            *
 * Purpose: resolves trigger dependencies for the specified triggers based on *
 *          host and linked templates                                         *
 *                                                                            *
 * Parameters: hostid    - [IN] host identificator from database              *
 *             trids     - [IN] array of trigger identifiers from database    *
 *             trids_num - [IN] trigger count in trids array                  *
 *             links     - [OUT] pairs of trigger dependencies  (down,up)     *
 *                                                                            *
 ******************************************************************************/
static void	DBresolve_template_trigger_dependencies(zbx_uint64_t hostid, const zbx_uint64_t *trids,
		int trids_num, zbx_vector_uint64_pair_t *links)
{
	DB_RESULT			result;
	DB_ROW				row;
	zbx_uint64_pair_t		map_id, dep_list_id;
	char				*sql = NULL;
	size_t				sql_alloc = 512, sql_offset;
	zbx_vector_uint64_pair_t	dep_list_ids, map_ids;
	zbx_vector_uint64_t		all_templ_ids;
	zbx_uint64_t			templateid_down, templateid_up,
					triggerid_down, triggerid_up,
					hst_triggerid, tpl_triggerid;
	int				i, j;

	zbx_vector_uint64_create(&all_templ_ids);
	zbx_vector_uint64_pair_create(&dep_list_ids);
	zbx_vector_uint64_pair_create(links);
	sql = (char *)zbx_malloc(sql, sql_alloc);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select td.triggerid_down,td.triggerid_up,t.triggerid,t.flags,td.triggerdepid"
			" from triggers t,trigger_depends td"
			" where t.templateid in (td.triggerid_up,td.triggerid_down) and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.triggerid", trids, trids_num);

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		int		iii, flags;
		zbx_uint64_t	triggerid;
		char		audit_key_dependencies[100];

		for (iii = 0; iii < dep_list_ids.values_num; iii++)
		{
			zbx_uint64_pair_t p = (zbx_uint64_pair_t)dep_list_ids.values[iii];

			if (!(p.first == dep_list_id.first && p.second == dep_list_id.second))
			{
				ZBX_STR2UINT64(dep_list_id.first, row[0]);
				ZBX_STR2UINT64(dep_list_id.second, row[1]);
				zbx_vector_uint64_pair_append(&dep_list_ids, dep_list_id);
				zbx_vector_uint64_append(&all_templ_ids, dep_list_id.first);
				zbx_vector_uint64_append(&all_templ_ids, dep_list_id.second);
			}
		}

		ZBX_STR2UINT64(flags, row[3]);
		ZBX_STR2UINT64(triggerid, row[2]);

		if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
		{
			zbx_snprintf(audit_key_dependencies, 100, "trigger.dependencies[%s]", row[4]);
			zbx_audit_update_json_string(triggerid, audit_key_dependencies,
					row[1]);
		}
		else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
		{
			zbx_snprintf(audit_key_dependencies, 100, "triggerprototype.dependencies[%s]", row[4]);
			zbx_audit_update_json_string(triggerid, audit_key_dependencies,
					row[1]);
		}
	}

	DBfree_result(result);

	if (0 == dep_list_ids.values_num)	/* not all trigger template have a dependency trigger */
	{
		zbx_vector_uint64_destroy(&all_templ_ids);
		zbx_vector_uint64_pair_destroy(&dep_list_ids);
		zbx_free(sql);
		return;
	}

	zbx_vector_uint64_pair_create(&map_ids);
	zbx_vector_uint64_sort(&all_templ_ids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
	zbx_vector_uint64_uniq(&all_templ_ids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	sql_offset = 0;
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select t.triggerid,t.templateid"
			" from triggers t,functions f,items i"
			" where t.triggerid=f.triggerid"
				" and f.itemid=i.itemid"
				" and i.hostid=" ZBX_FS_UI64
				" and",
				hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.templateid", all_templ_ids.values,
			all_templ_ids.values_num);

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(map_id.first, row[0]);
		ZBX_DBROW2UINT64(map_id.second, row[1]);
		zbx_vector_uint64_pair_append(&map_ids, map_id);
	}
	DBfree_result(result);

	zbx_free(sql);
	zbx_vector_uint64_destroy(&all_templ_ids);

	for (i = 0; i < dep_list_ids.values_num; i++)
	{
		templateid_down = dep_list_ids.values[i].first;
		templateid_up = dep_list_ids.values[i].second;

		/* Convert template ids to corresponding trigger ids.         */
		/* If template trigger depends on host trigger rather than    */
		/* template trigger then up id conversion will fail and the   */
		/* original value (host trigger id) will be used as intended. */
		triggerid_down = 0;
		triggerid_up = templateid_up;

		for (j = 0; j < map_ids.values_num; j++)
		{
			hst_triggerid = map_ids.values[j].first;
			tpl_triggerid = map_ids.values[j].second;

			if (tpl_triggerid == templateid_down)
				triggerid_down = hst_triggerid;

			if (tpl_triggerid == templateid_up)
				triggerid_up = hst_triggerid;
		}

		if (0 != triggerid_down)
		{
			zbx_uint64_pair_t	link = {triggerid_down, triggerid_up};

			zbx_vector_uint64_pair_append(links, link);
		}
	}

	zbx_vector_uint64_pair_destroy(&map_ids);
	zbx_vector_uint64_pair_destroy(&dep_list_ids);
}

/******************************************************************************
 *                                                                            *
 * Function: DBadd_template_dependencies_for_new_triggers                     *
 *                                                                            *
 * Purpose: update trigger dependencies for specified host                    *
 *                                                                            *
 * Parameters: hostid    - [IN] host identificator from database              *
 *             trids     - [IN] array of trigger identifiers from database    *
 *             trids_num - [IN] trigger count in trids array                  *
 *                                                                            *
 * Return value: upon successful completion return SUCCEED                    *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	DBadd_template_dependencies_for_new_triggers(zbx_uint64_t hostid, zbx_uint64_t *trids, int trids_num)
{
	int				i;
	zbx_uint64_t			triggerdepid;
	zbx_db_insert_t			db_insert;
	zbx_vector_uint64_pair_t	links;

	if (0 == trids_num)
		return SUCCEED;

	DBresolve_template_trigger_dependencies(hostid, trids, trids_num, &links);

	if (0 < links.values_num)
	{
		triggerdepid = DBget_maxid_num("trigger_depends", links.values_num);

		zbx_db_insert_prepare(&db_insert, "trigger_depends", "triggerdepid", "triggerid_down", "triggerid_up",
				NULL);

		for (i = 0; i < links.values_num; i++)
		{
			zbx_db_insert_add_values(&db_insert, triggerdepid++, links.values[i].first,
					links.values[i].second);
		}

		zbx_db_insert_execute(&db_insert);
		zbx_db_insert_clean(&db_insert);
	}

	zbx_vector_uint64_pair_destroy(&links);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_template_trigger_tags                                     *
 *                                                                            *
 * Purpose: copies tags from template triggers to created/linked triggers     *
 *                                                                            *
 * Parameters: new_triggerids - the created trigger ids                        *
 *             cur_triggerids - the linked trigfer ids                         *
 *                                                                            *
 * Return value: upon successful completion return SUCCEED                    *
 *                                                                            *
 ******************************************************************************/
static int	DBcopy_template_trigger_tags(const zbx_vector_uint64_t *new_triggerids,
		const zbx_vector_uint64_t *cur_triggerids)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	int			i;
	zbx_vector_uint64_t	triggerids;
	zbx_uint64_t		triggerid;
	zbx_db_insert_t		db_insert;

	if (0 == new_triggerids->values_num && 0 == cur_triggerids->values_num)
		return SUCCEED;

	zbx_vector_uint64_create(&triggerids);
	zbx_vector_uint64_reserve(&triggerids, new_triggerids->values_num + cur_triggerids->values_num);

	if (0 != cur_triggerids->values_num)
	{
		/* remove tags from host triggers that were linking to template triggers */
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from trigger_tag where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "triggerid", cur_triggerids->values,
				cur_triggerids->values_num);
		DBexecute("%s", sql);

		sql_offset = 0;

		for (i = 0; i < cur_triggerids->values_num; i++)
			zbx_vector_uint64_append(&triggerids, cur_triggerids->values[i]);
	}

	for (i = 0; i < new_triggerids->values_num; i++)
		zbx_vector_uint64_append(&triggerids, new_triggerids->values[i]);

	zbx_vector_uint64_sort(&triggerids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select t.triggerid,tt.tag,tt.value,t.flags,tt.triggertagid"
			" from trigger_tag tt,triggers t"
			" where tt.triggerid=t.templateid"
			" and");

	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.triggerid", triggerids.values, triggerids.values_num);

	result = DBselect("%s", sql);

	zbx_db_insert_prepare(&db_insert, "trigger_tag", "triggertagid", "triggerid", "tag", "value", NULL);

	while (NULL != (row = DBfetch(result)))
	{
		int	flags;
		char	audit_key_tags_tag[100], audit_key_tags_value[100];

		ZBX_STR2UINT64(triggerid, row[0]);
		ZBX_STR2UINT64(flags, row[3]);

		zbx_db_insert_add_values(&db_insert, __UINT64_C(0), triggerid, row[1], row[2]);

		if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
		{
			zbx_snprintf(audit_key_tags_tag, 100, "trigger.tags[%s].tag", row[4]);
			zbx_snprintf(audit_key_tags_value, 100, "trigger.tags[%s].value", row[4]);
			zbx_audit_update_json_string(triggerid, audit_key_tags_tag, row[1]);
			zbx_audit_update_json_string(triggerid, audit_key_tags_value, row[2]);
		}
		else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
		{
			zbx_snprintf(audit_key_tags_tag, 100, "triggerprototype.tags[%s].tag", row[4]);
			zbx_snprintf(audit_key_tags_value, 100, "triggerprototype.tags[%s].value", row[4]);
			zbx_audit_update_json_string(triggerid, audit_key_tags_tag, row[1]);
			zbx_audit_update_json_string(triggerid, audit_key_tags_value, row[2]);
		}
	}

	DBfree_result(result);

	zbx_free(sql);

	zbx_db_insert_autoincrement(&db_insert, "triggertagid");
	zbx_db_insert_execute(&db_insert);
	zbx_db_insert_clean(&db_insert);

	zbx_vector_uint64_destroy(&triggerids);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Function: get_templates_by_hostid                                          *
 *                                                                            *
 * Description: Retrieve already linked templates for specified host          *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 ******************************************************************************/
static void	get_templates_by_hostid(zbx_uint64_t hostid, zbx_vector_uint64_t *templateids)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	templateid;

	result = DBselect(
			"select templateid"
			" from hosts_templates"
			" where hostid=" ZBX_FS_UI64,
			hostid);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(templateid, row[0]);
		zbx_vector_uint64_append(templateids, templateid);
	}
	DBfree_result(result);

	zbx_vector_uint64_sort(templateids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_template_elements                                       *
 *                                                                            *
 * Purpose: delete template elements from host                                *
 *                                                                            *
 * Parameters: hostid          - [IN] host identificator from database        *
 *             del_templateids - [IN] array of template IDs                   *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
int	DBdelete_template_elements(zbx_uint64_t hostid, zbx_vector_uint64_t *del_templateids, char **error,
		char *recsetid_cuid)
{
	char			*sql = NULL, err[MAX_STRING_LEN];
	size_t			sql_alloc = 128, sql_offset = 0;
	zbx_vector_uint64_t	templateids;
	int			i, index, res = SUCCEED;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_audit_init();

	zbx_vector_uint64_create(&templateids);

	get_templates_by_hostid(hostid, &templateids);

	for (i = 0; i < del_templateids->values_num; i++)
	{
		if (FAIL == (index = zbx_vector_uint64_bsearch(&templateids, del_templateids->values[i],
				ZBX_DEFAULT_UINT64_COMPARE_FUNC)))
		{
			/* template already unlinked */
			zbx_vector_uint64_remove(del_templateids, i--);
		}
		else
			zbx_vector_uint64_remove(&templateids, index);
	}

	/* all templates already unlinked */
	if (0 == del_templateids->values_num)
		goto clean;

	if (SUCCEED != (res = validate_linked_templates(&templateids, err, sizeof(err))))
	{
		*error = zbx_strdup(NULL, err);
		goto clean;
	}

	DBdelete_template_httptests(hostid, del_templateids);
	DBdelete_template_graphs(hostid, del_templateids);
	DBdelete_template_triggers(hostid, del_templateids);
	DBdelete_template_host_prototypes(hostid, del_templateids);

	/* removing items will remove discovery rules related to them */
	DBdelete_template_items(hostid, del_templateids);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"delete from hosts_templates"
			" where hostid=" ZBX_FS_UI64
				" and",
			hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "templateid",
			del_templateids->values, del_templateids->values_num);
	DBexecute("%s", sql);

	zbx_free(sql);

	zbx_audit_flush(recsetid_cuid);
clean:
	zbx_vector_uint64_destroy(&templateids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(res));

	return res;
}

typedef struct
{
	zbx_uint64_t	group_prototypeid;
	zbx_uint64_t	groupid;
	zbx_uint64_t	templateid;	/* reference to parent group_prototypeid */
	char		*name;
}
zbx_group_prototype_t;

static void	DBgroup_prototype_clean(zbx_group_prototype_t *group_prototype)
{
	zbx_free(group_prototype->name);
	zbx_free(group_prototype);
}

static void	DBgroup_prototypes_clean(zbx_vector_ptr_t *group_prototypes)
{
	int	i;

	for (i = 0; i < group_prototypes->values_num; i++)
		DBgroup_prototype_clean((zbx_group_prototype_t *)group_prototypes->values[i]);
}

typedef struct
{
	zbx_uint64_t	hostmacroid;
	char		*macro;
	char		*value;
	char		*description;
	unsigned char	type;
#define ZBX_FLAG_HPMACRO_UPDATE_VALUE		__UINT64_C(0x00000001)
#define ZBX_FLAG_HPMACRO_UPDATE_DESCRIPTION	__UINT64_C(0x00000002)
#define ZBX_FLAG_HPMACRO_UPDATE_TYPE		__UINT64_C(0x00000004)
#define ZBX_FLAG_HPMACRO_UPDATE	\
		(ZBX_FLAG_HPMACRO_UPDATE_VALUE | ZBX_FLAG_HPMACRO_UPDATE_DESCRIPTION | ZBX_FLAG_HPMACRO_UPDATE_TYPE)
	zbx_uint64_t	flags;
}
zbx_macros_prototype_t;

ZBX_PTR_VECTOR_DECL(macros, zbx_macros_prototype_t *)
ZBX_PTR_VECTOR_IMPL(macros, zbx_macros_prototype_t *)

typedef struct
{
	char		*community;
	char		*securityname;
	char		*authpassphrase;
	char		*privpassphrase;
	char		*contextname;
	unsigned char	securitylevel;
	unsigned char	authprotocol;
	unsigned char	privprotocol;
	unsigned char	version;
	unsigned char	bulk;
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_TYPE		__UINT64_C(0x00000001)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_BULK		__UINT64_C(0x00000002)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_COMMUNITY	__UINT64_C(0x00000004)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECNAME	__UINT64_C(0x00000008)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECLEVEL	__UINT64_C(0x00000010)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPASS	__UINT64_C(0x00000020)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPASS	__UINT64_C(0x00000040)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPROTOCOL	__UINT64_C(0x00000080)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPROTOCOL	__UINT64_C(0x00000100)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_CONTEXT	__UINT64_C(0x00000200)
#define ZBX_FLAG_HPINTERFACE_SNMP_UPDATE									\
		(ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_TYPE | ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_BULK |		\
		ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_COMMUNITY | ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECNAME |		\
		ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECLEVEL | ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPASS |		\
		ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPASS | ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPROTOCOL |	\
		ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPROTOCOL | ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_CONTEXT)
#define ZBX_FLAG_HPINTERFACE_SNMP_CREATE		__UINT64_C(0x00000400)
	zbx_uint64_t	flags;
}
zbx_interface_prototype_snmp_t;

typedef struct
{
	zbx_uint64_t	interfaceid;
	unsigned char	main;
	unsigned char	type;
	unsigned char	useip;
	char		*ip;
	char		*dns;
	char		*port;
#define ZBX_FLAG_HPINTERFACE_UPDATE_MAIN	__UINT64_C(0x00000001)
#define ZBX_FLAG_HPINTERFACE_UPDATE_TYPE	__UINT64_C(0x00000002)
#define ZBX_FLAG_HPINTERFACE_UPDATE_USEIP	__UINT64_C(0x00000004)
#define ZBX_FLAG_HPINTERFACE_UPDATE_IP		__UINT64_C(0x00000008)
#define ZBX_FLAG_HPINTERFACE_UPDATE_DNS		__UINT64_C(0x00000010)
#define ZBX_FLAG_HPINTERFACE_UPDATE_PORT	__UINT64_C(0x00000020)
#define ZBX_FLAG_HPINTERFACE_UPDATE	\
		(ZBX_FLAG_HPINTERFACE_UPDATE_MAIN | ZBX_FLAG_HPINTERFACE_UPDATE_TYPE | \
		ZBX_FLAG_HPINTERFACE_UPDATE_USEIP | ZBX_FLAG_HPINTERFACE_UPDATE_IP | \
		ZBX_FLAG_HPINTERFACE_UPDATE_DNS | ZBX_FLAG_HPINTERFACE_UPDATE_PORT)
	zbx_uint64_t	flags;
	union _data
	{
		zbx_interface_prototype_snmp_t	*snmp;
	}
	data;
}
zbx_interfaces_prototype_t;

ZBX_PTR_VECTOR_DECL(interfaces, zbx_interfaces_prototype_t *)
ZBX_PTR_VECTOR_IMPL(interfaces, zbx_interfaces_prototype_t *)

typedef struct
{
	zbx_uint64_t		templateid;		/* link to parent template */
	zbx_uint64_t		hostid;
	zbx_uint64_t		itemid;			/* discovery rule id */
	zbx_vector_uint64_t	lnk_templateids;	/* list of templates which should be linked */
	zbx_vector_ptr_t	group_prototypes;	/* list of group prototypes */
	zbx_vector_macros_t	hostmacros;		/* list of user macros */
	zbx_vector_db_tag_ptr_t	tags;			/* list of host prototype tags */
	zbx_vector_interfaces_t	interfaces;		/* list of interfaces */
	char			*host;
	char			*name;
	unsigned char		status;
#define ZBX_FLAG_HPLINK_UPDATE_NAME			0x01
#define ZBX_FLAG_HPLINK_UPDATE_STATUS			0x02
#define ZBX_FLAG_HPLINK_UPDATE_DISCOVER			0x04
#define ZBX_FLAG_HPLINK_UPDATE_CUSTOM_INTERFACES	0x08
	unsigned char		flags;
	unsigned char		discover;
	unsigned char		custom_interfaces;
}
zbx_host_prototype_t;

static void	DBhost_macro_free(zbx_macros_prototype_t *hostmacro)
{
	zbx_free(hostmacro->macro);
	zbx_free(hostmacro->value);
	zbx_free(hostmacro->description);
	zbx_free(hostmacro);
}

static void	DBhost_interface_free(zbx_interfaces_prototype_t *interface)
{
	zbx_free(interface->ip);
	zbx_free(interface->dns);
	zbx_free(interface->port);

	if (INTERFACE_TYPE_SNMP == interface->type)
	{
		zbx_free(interface->data.snmp->community);
		zbx_free(interface->data.snmp->securityname);
		zbx_free(interface->data.snmp->authpassphrase);
		zbx_free(interface->data.snmp->privpassphrase);
		zbx_free(interface->data.snmp->contextname);
		zbx_free(interface->data.snmp);
	}

	zbx_free(interface);
}

static void	DBhost_prototype_clean(zbx_host_prototype_t *host_prototype)
{
	zbx_free(host_prototype->name);
	zbx_free(host_prototype->host);
	zbx_vector_macros_clear_ext(&host_prototype->hostmacros, DBhost_macro_free);
	zbx_vector_macros_destroy(&host_prototype->hostmacros);
	zbx_vector_db_tag_ptr_clear_ext(&host_prototype->tags, zbx_db_tag_free);
	zbx_vector_db_tag_ptr_destroy(&host_prototype->tags);
	zbx_vector_interfaces_clear_ext(&host_prototype->interfaces, DBhost_interface_free);
	zbx_vector_interfaces_destroy(&host_prototype->interfaces);
	DBgroup_prototypes_clean(&host_prototype->group_prototypes);
	zbx_vector_ptr_destroy(&host_prototype->group_prototypes);
	zbx_vector_uint64_destroy(&host_prototype->lnk_templateids);
	zbx_free(host_prototype);
}

static void	DBhost_prototypes_clean(zbx_vector_ptr_t *host_prototypes)
{
	int	i;

	for (i = 0; i < host_prototypes->values_num; i++)
		DBhost_prototype_clean((zbx_host_prototype_t *)host_prototypes->values[i]);
}

/******************************************************************************
 *                                                                            *
 * Function: DBis_regular_host                                                *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static int	DBis_regular_host(zbx_uint64_t hostid)
{
	DB_RESULT	result;
	DB_ROW		row;
	int		ret = FAIL;

	result = DBselect("select flags from hosts where hostid=" ZBX_FS_UI64, hostid);

	if (NULL != (row = DBfetch(result)))
	{
		if (0 == atoi(row[0]))
			ret = SUCCEED;
	}
	DBfree_result(result);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_make                                           *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_make(zbx_uint64_t hostid, zbx_vector_uint64_t *templateids,
		zbx_vector_ptr_t *host_prototypes)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	itemids;
	zbx_host_prototype_t	*host_prototype;

	zbx_vector_uint64_create(&itemids);

	/* selects host prototypes from templates */

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select hi.itemid,th.hostid,th.host,th.name,th.status,th.discover,th.custom_interfaces"
			" from items hi,items ti,host_discovery thd,hosts th"
			" where hi.templateid=ti.itemid"
				" and ti.itemid=thd.parent_itemid"
				" and thd.hostid=th.hostid"
				" and hi.hostid=" ZBX_FS_UI64
				" and",
			hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "ti.hostid", templateids->values, templateids->values_num);

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		host_prototype = (zbx_host_prototype_t *)zbx_malloc(NULL, sizeof(zbx_host_prototype_t));

		host_prototype->hostid = 0;
		ZBX_STR2UINT64(host_prototype->itemid, row[0]);
		ZBX_STR2UINT64(host_prototype->templateid, row[1]);
		zbx_vector_uint64_create(&host_prototype->lnk_templateids);
		zbx_vector_ptr_create(&host_prototype->group_prototypes);
		zbx_vector_macros_create(&host_prototype->hostmacros);
		zbx_vector_db_tag_ptr_create(&host_prototype->tags);
		zbx_vector_interfaces_create(&host_prototype->interfaces);
		host_prototype->host = zbx_strdup(NULL, row[2]);
		host_prototype->name = zbx_strdup(NULL, row[3]);
		ZBX_STR2UCHAR(host_prototype->status, row[4]);
		host_prototype->flags = 0;
		ZBX_STR2UCHAR(host_prototype->discover, row[5]);
		ZBX_STR2UCHAR(host_prototype->custom_interfaces, row[6]);

		zbx_vector_ptr_append(host_prototypes, host_prototype);
		zbx_vector_uint64_append(&itemids, host_prototype->itemid);
	}
	DBfree_result(result);

	if (0 != host_prototypes->values_num)
	{
		zbx_uint64_t	itemid;
		unsigned char	status;
		int		i;

		zbx_vector_uint64_sort(&itemids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
		zbx_vector_uint64_uniq(&itemids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		/* selects host prototypes from host */

		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select i.itemid,h.hostid,h.host,h.name,h.status,h.discover"
				" from items i,host_discovery hd,hosts h"
				" where i.itemid=hd.parent_itemid"
					" and hd.hostid=h.hostid"
					" and i.hostid=" ZBX_FS_UI64
					" and",
				hostid);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i.itemid", itemids.values, itemids.values_num);

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(itemid, row[0]);

			for (i = 0; i < host_prototypes->values_num; i++)
			{
				host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

				if (host_prototype->itemid == itemid && 0 == strcmp(host_prototype->host, row[2]))
				{
					ZBX_STR2UINT64(host_prototype->hostid, row[1]);
					if (0 != strcmp(host_prototype->name, row[3]))
						host_prototype->flags |= ZBX_FLAG_HPLINK_UPDATE_NAME;
					if (host_prototype->status != (status = (unsigned char)atoi(row[4])))
						host_prototype->flags |= ZBX_FLAG_HPLINK_UPDATE_STATUS;
					if (host_prototype->discover != (unsigned char)atoi(row[5]))
						host_prototype->flags |= ZBX_FLAG_HPLINK_UPDATE_DISCOVER;
					if (host_prototype->custom_interfaces != (unsigned char)atoi(row[6]))
						host_prototype->flags |= ZBX_FLAG_HPLINK_UPDATE_CUSTOM_INTERFACES;
					break;
				}
			}
		}
		DBfree_result(result);
	}

	zbx_free(sql);

	zbx_vector_uint64_destroy(&itemids);

	/* sort by templateid */
	zbx_vector_ptr_sort(host_prototypes, ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_templates_make                                 *
 *                                                                            *
 * Parameters: host_prototypes     - [IN/OUT] list of host prototypes         *
 *                                   should be sorted by templateid           *
 *             del_hosttemplateids - [OUT] list of hosttemplateids which      *
 *                                   should be deleted                        *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_templates_make(zbx_vector_ptr_t *host_prototypes,
		zbx_vector_uint64_t *del_hosttemplateids)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	hostids;
	zbx_uint64_t		hostid, templateid, hosttemplateid;
	zbx_host_prototype_t	*host_prototype;
	int			i;

	zbx_vector_uint64_create(&hostids);

	/* select list of templates which should be linked to host prototypes */

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		zbx_vector_uint64_append(&hostids, host_prototype->templateid);
	}

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hostid,templateid"
			" from hosts_templates"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hostid,templateid");

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(hostid, row[0]);
		ZBX_STR2UINT64(templateid, row[1]);

		if (FAIL == (i = zbx_vector_ptr_bsearch(host_prototypes, &hostid, ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
		{
			THIS_SHOULD_NEVER_HAPPEN;
			continue;
		}

		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		zbx_vector_uint64_append(&host_prototype->lnk_templateids, templateid);
	}
	DBfree_result(result);

	/* select list of templates which already linked to host prototypes */

	zbx_vector_uint64_clear(&hostids);

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		if (0 == host_prototype->hostid)
			continue;

		zbx_vector_uint64_append(&hostids, host_prototype->hostid);
	}

	if (0 != hostids.values_num)
	{
		zbx_vector_uint64_sort(&hostids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select hostid,templateid,hosttemplateid"
				" from hosts_templates"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hosttemplateid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(hostid, row[0]);
			ZBX_STR2UINT64(templateid, row[1]);

			for (i = 0; i < host_prototypes->values_num; i++)
			{
				host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

				if (host_prototype->hostid == hostid)
				{
					if (FAIL == (i = zbx_vector_uint64_bsearch(&host_prototype->lnk_templateids,
							templateid, ZBX_DEFAULT_UINT64_COMPARE_FUNC)))
					{
						ZBX_STR2UINT64(hosttemplateid, row[2]);
						zbx_vector_uint64_append(del_hosttemplateids, hosttemplateid);
					}
					else
						zbx_vector_uint64_remove(&host_prototype->lnk_templateids, i);

					break;
				}
			}

			if (i == host_prototypes->values_num)
				THIS_SHOULD_NEVER_HAPPEN;
		}
		DBfree_result(result);
	}

	zbx_vector_uint64_destroy(&hostids);

	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_groups_make                                    *
 *                                                                            *
 * Parameters: host_prototypes        - [IN/OUT] list of host prototypes      *
 *                                      should be sorted by templateid        *
 *             del_group_prototypeids - [OUT] sorted list of                  *
 *                                      group_prototypeid which should be     *
 *                                      deleted                               *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_groups_make(zbx_vector_ptr_t *host_prototypes,
		zbx_vector_uint64_t *del_group_prototypeids)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	hostids;
	zbx_uint64_t		hostid, groupid, group_prototypeid;
	zbx_host_prototype_t	*host_prototype;
	zbx_group_prototype_t	*group_prototype;
	int			i;

	zbx_vector_uint64_create(&hostids);

	/* select list of groups which should be linked to host prototypes */

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		zbx_vector_uint64_append(&hostids, host_prototype->templateid);
	}

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hostid,name,groupid,group_prototypeid"
			" from group_prototype"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hostid");

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(hostid, row[0]);

		if (FAIL == (i = zbx_vector_ptr_bsearch(host_prototypes, &hostid, ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
		{
			THIS_SHOULD_NEVER_HAPPEN;
			continue;
		}

		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		group_prototype = (zbx_group_prototype_t *)zbx_malloc(NULL, sizeof(zbx_group_prototype_t));
		group_prototype->group_prototypeid = 0;
		group_prototype->name = zbx_strdup(NULL, row[1]);
		ZBX_DBROW2UINT64(group_prototype->groupid, row[2]);
		ZBX_STR2UINT64(group_prototype->templateid, row[3]);

		zbx_vector_ptr_append(&host_prototype->group_prototypes, group_prototype);
	}
	DBfree_result(result);

	/* select list of group prototypes which already linked to host prototypes */

	zbx_vector_uint64_clear(&hostids);

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		if (0 == host_prototype->hostid)
			continue;

		zbx_vector_uint64_append(&hostids, host_prototype->hostid);
	}

	if (0 != hostids.values_num)
	{
		zbx_vector_uint64_sort(&hostids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select hostid,group_prototypeid,groupid,name from group_prototype where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by group_prototypeid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(hostid, row[0]);

			for (i = 0; i < host_prototypes->values_num; i++)
			{
				host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

				if (host_prototype->hostid == hostid)
				{
					int	k;

					ZBX_STR2UINT64(group_prototypeid, row[1]);
					ZBX_DBROW2UINT64(groupid, row[2]);

					for (k = 0; k < host_prototype->group_prototypes.values_num; k++)
					{
						group_prototype = (zbx_group_prototype_t *)
								host_prototype->group_prototypes.values[k];

						if (0 != group_prototype->group_prototypeid)
							continue;

						if (group_prototype->groupid == groupid &&
								0 == strcmp(group_prototype->name, row[3]))
						{
							group_prototype->group_prototypeid = group_prototypeid;
							break;
						}
					}

					if (k == host_prototype->group_prototypes.values_num)
						zbx_vector_uint64_append(del_group_prototypeids, group_prototypeid);

					break;
				}
			}

			if (i == host_prototypes->values_num)
				THIS_SHOULD_NEVER_HAPPEN;
		}
		DBfree_result(result);
	}

	zbx_vector_uint64_sort(del_group_prototypeids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	zbx_vector_uint64_destroy(&hostids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_macro_make                                     *
 *                                                                            *
 * Purpose: validate hostmacros value changes                                 *
 *                                                                            *
 * Parameters: hostmacros  - [IN/OUT] list of hostmacros                      *
 *             hostmacroid - [IN] hostmacro id                                *
 *             macro       - [IN] hostmacro key                               *
 *             value       - [IN] hostmacro value                             *
 *             description - [IN] hostmacro description                       *
 *             type        - [IN] hostmacro type                              *
 *                                                                            *
 * Return value: SUCCEED - the host macro was found                           *
 *               FAIL    - in the other case                                  *
 ******************************************************************************/
static int	DBhost_prototypes_macro_make(zbx_vector_macros_t *hostmacros, zbx_uint64_t hostmacroid,
		const char *macro, const char *value, const char *description, unsigned char type)
{
	zbx_macros_prototype_t	*hostmacro;
	int			i;

	for (i = 0; i < hostmacros->values_num; i++)
	{
		hostmacro = hostmacros->values[i];

		/* check if host macro has already been added */
		if (0 == hostmacro->hostmacroid && 0 == strcmp(hostmacro->macro, macro))
		{
			hostmacro->hostmacroid = hostmacroid;

			if (0 != strcmp(hostmacro->value, value))
				hostmacro->flags |= ZBX_FLAG_HPMACRO_UPDATE_VALUE;

			if (0 != strcmp(hostmacro->description, description))
				hostmacro->flags |= ZBX_FLAG_HPMACRO_UPDATE_DESCRIPTION;

			if (hostmacro->type != type)
				hostmacro->flags |= ZBX_FLAG_HPMACRO_UPDATE_TYPE;

			return SUCCEED;
		}
	}

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_interface_make                                 *
 *                                                                            *
 * Purpose: fill empty value in interfaces with input parameters              *
 *                                                                            *
 * Parameters: interfaces     - [IN/OUT] list of host interfaces              *
 *             interfaceid    - [IN] interface id                             *
 *             ifmain         - [IN] interface main                           *
 *             type           - [IN] interface type                           *
 *             useip          - [IN] interface useip                          *
 *             ip             - [IN] interface ip                             *
 *             dns            - [IN] interface dns                            *
 *             port           - [IN] interface port                           *
 *             snmp_type      - [IN] interface_snmp version                   *
 *             bulk           - [IN] interface_snmp bulk                      *
 *             community      - [IN] interface_snmp community                 *
 *             securityname   - [IN] interface_snmp securityname              *
 *             securitylevel  - [IN] interface_snmp securitylevel             *
 *             authpassphrase - [IN] interface_snmp authpassphrase            *
 *             privpassphrase - [IN] interface_snmp privpassphrase            *
 *             authprotocol   - [IN] interface_snmp authprotocol              *
 *             privprotocol   - [IN] interface_snmp privprotocol              *
 *             contextname    - [IN] interface_snmp contextname               *
 *                                                                            *
 * Return value: SUCCEED - the host interface was found                       *
 *               FAIL    - in the other case                                  *
 ******************************************************************************/
static int	DBhost_prototypes_interface_make(zbx_vector_interfaces_t *interfaces, zbx_uint64_t interfaceid,
		unsigned char ifmain, unsigned char type, unsigned char useip, const char *ip, const char *dns,
		const char *port, unsigned char snmp_type, unsigned char bulk, const char *community,
		const char *securityname, unsigned char securitylevel, const char *authpassphrase,
		const char *privpassphrase, unsigned char authprotocol, unsigned char privprotocol,
		const char *contextname)
{
	zbx_interfaces_prototype_t	*interface;
	int				i;

	for (i = 0; i < interfaces->values_num; i++)
	{
		interface = interfaces->values[i];

		if (0 == interface->interfaceid)
		{
			interface->interfaceid = interfaceid;

			if (interface->main != ifmain)
				interface->flags |= ZBX_FLAG_HPINTERFACE_UPDATE_MAIN;

			if (interface->type != type)
				interface->flags |= ZBX_FLAG_HPINTERFACE_UPDATE_TYPE;

			if (interface->useip != useip)
				interface->flags |= ZBX_FLAG_HPINTERFACE_UPDATE_USEIP;

			if (0 != strcmp(interface->ip, ip))
				interface->flags |= ZBX_FLAG_HPINTERFACE_UPDATE_IP;

			if (0 != strcmp(interface->dns, dns))
				interface->flags |= ZBX_FLAG_HPINTERFACE_UPDATE_DNS;

			if (0 != strcmp(interface->port, port))
				interface->flags |= ZBX_FLAG_HPINTERFACE_UPDATE_PORT;

			if (INTERFACE_TYPE_SNMP == interface->type)
			{
				zbx_interface_prototype_snmp_t *snmp = interface->data.snmp;

				if (snmp->version != snmp_type)
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_TYPE;
				if (snmp->bulk != bulk)
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_BULK;
				if (0 != strcmp(snmp->community, community))
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_COMMUNITY;
				if (0 != strcmp(snmp->securityname, securityname))
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECNAME;
				if (snmp->securitylevel != securitylevel)
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECLEVEL;
				if (0 != strcmp(snmp->authpassphrase, authpassphrase))
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPASS;
				if (0 != strcmp(snmp->privpassphrase, privpassphrase))
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPASS;
				if (snmp->authprotocol != authprotocol)
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPROTOCOL;
				if (snmp->privprotocol != privprotocol)
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPROTOCOL;
				if (0 != strcmp(snmp->contextname, contextname))
					snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_CONTEXT;
			}

			return SUCCEED;
		}
	}

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_macros_make                                    *
 *                                                                            *
 * Parameters: host_prototypes - [IN/OUT] list of host prototypes             *
 *                                   should be sorted by templateid           *
 *             del_macroids    - [OUT] sorted list of host macroids which     *
 *                                   should be deleted                        *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_macros_make(zbx_vector_ptr_t *host_prototypes, zbx_vector_uint64_t *del_macroids)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	hostids;
	zbx_uint64_t		hostid, hostmacroid;
	zbx_host_prototype_t	*host_prototype;
	zbx_macros_prototype_t	*hostmacro;
	int			i;

	zbx_vector_uint64_create(&hostids);

	/* select list of macros prototypes which should be linked to host prototypes */

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		zbx_vector_uint64_append(&hostids, host_prototype->templateid);
	}

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hostid,macro,value,description,type"
			" from hostmacro"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hostid");

	result = DBselect("%s", sql);
	host_prototype = NULL;

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(hostid, row[0]);

		if (NULL == host_prototype || host_prototype->templateid != hostid)
		{
			if (FAIL == (i = zbx_vector_ptr_bsearch(host_prototypes, &hostid,
					ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
			{
				THIS_SHOULD_NEVER_HAPPEN;
				continue;
			}

			host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];
		}

		hostmacro = (zbx_macros_prototype_t *)zbx_malloc(NULL, sizeof(zbx_macros_prototype_t));
		hostmacro->hostmacroid = 0;
		hostmacro->macro = zbx_strdup(NULL, row[1]);
		hostmacro->value = zbx_strdup(NULL, row[2]);
		hostmacro->description = zbx_strdup(NULL, row[3]);
		ZBX_STR2UCHAR(hostmacro->type, row[4]);
		hostmacro->flags = 0;

		zbx_vector_macros_append(&host_prototype->hostmacros, hostmacro);
	}
	DBfree_result(result);

	/* select list of macros prototypes which already linked to host prototypes */

	zbx_vector_uint64_clear(&hostids);

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		if (0 == host_prototype->hostid)
			continue;

		zbx_vector_uint64_append(&hostids, host_prototype->hostid);
	}

	if (0 != hostids.values_num)
	{
		zbx_vector_uint64_sort(&hostids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select hostmacroid,hostid,macro,value,description,type from hostmacro where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hostid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(hostid, row[1]);

			for (i = 0; i < host_prototypes->values_num; i++)
			{
				host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

				if (host_prototype->hostid == hostid)
				{
					unsigned char	type;

					ZBX_STR2UINT64(hostmacroid, row[0]);
					ZBX_STR2UCHAR(type, row[5]);

					if (FAIL == DBhost_prototypes_macro_make(&host_prototype->hostmacros,
							hostmacroid, row[2], row[3], row[4], type))
					{
						zbx_vector_uint64_append(del_macroids, hostmacroid);
					}

					break;
				}
			}

			if (i == host_prototypes->values_num)
				THIS_SHOULD_NEVER_HAPPEN;
		}
		DBfree_result(result);
	}

	zbx_vector_uint64_sort(del_macroids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	zbx_vector_uint64_destroy(&hostids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_tags_make                                      *
 *                                                                            *
 * Parameters: host_prototypes - [IN/OUT] list of host prototypes             *
 *                                   should be sorted by templateid           *
 *             del_tagids    - [OUT] list of host tagids which                *
 *                                   should be deleted                        *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_tags_make(zbx_vector_ptr_t *host_prototypes, zbx_vector_uint64_t *del_tagids)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	hostids;
	zbx_uint64_t		hostid, tagid;
	zbx_host_prototype_t	*host_prototype;
	zbx_db_tag_t		*tag;
	int			i;

	zbx_vector_uint64_create(&hostids);

	/* get template host prototype tags that must be added to host prototypes */

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		zbx_vector_uint64_append(&hostids, host_prototype->templateid);
	}

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hostid,tag,value"
			" from host_tag"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hostid");

	result = DBselect("%s", sql);
	host_prototype = NULL;

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(hostid, row[0]);

		if (NULL == host_prototype || host_prototype->templateid != hostid)
		{
			if (FAIL == (i = zbx_vector_ptr_bsearch(host_prototypes, &hostid,
					ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
			{
				THIS_SHOULD_NEVER_HAPPEN;
				continue;
			}

			host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];
		}

		tag = (zbx_db_tag_t *)zbx_malloc(NULL, sizeof(zbx_db_tag_t));
		tag->tagid = 0;
		tag->flags = 0;
		tag->tag = zbx_strdup(NULL, row[1]);
		tag->value = zbx_strdup(NULL, row[2]);

		zbx_vector_db_tag_ptr_append(&host_prototype->tags, tag);
	}
	DBfree_result(result);

	/* get tags of existing host prototypes */

	zbx_vector_uint64_clear(&hostids);

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		if (0 == host_prototype->hostid)
			continue;

		zbx_vector_uint64_append(&hostids, host_prototype->hostid);
	}

	/* replace existing tags with the new tags */
	if (0 != hostids.values_num)
	{
		int	tag_index;

		zbx_vector_uint64_sort(&hostids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select hosttagid,hostid,tag,value from host_tag where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids.values, hostids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hostid");
		result = DBselect("%s", sql);

		host_prototype = NULL;

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(tagid, row[0]);
			ZBX_STR2UINT64(hostid, row[1]);

			if (NULL == host_prototype || host_prototype->hostid != hostid)
			{
				tag_index = 0;

				for (i = 0; i < host_prototypes->values_num; i++)
				{
					host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

					if (host_prototype->hostid == hostid)
						break;
				}

				if (host_prototype->hostid != hostid)
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}
			}

			if (tag_index < host_prototype->tags.values_num)
			{
				host_prototype->tags.values[tag_index]->tagid = tagid;
				host_prototype->tags.values[tag_index]->flags |= ZBX_FLAG_DB_TAG_UPDATE_TAG |
						ZBX_FLAG_DB_TAG_UPDATE_VALUE;
			}
			else
				zbx_vector_uint64_append(del_tagids, tagid);

			tag_index++;
		}
		DBfree_result(result);
	}

	zbx_vector_uint64_sort(del_tagids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	zbx_vector_uint64_destroy(&hostids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_save                                           *
 *                                                                            *
 * Function: DBhost_prototypes_interfaces_make                                *
 *                                                                            *
 * Purpose: prepare interfaces to be added, updated or removed from DB        *
 * Parameters: host_prototypes       - [IN/OUT] list of host prototypes       *
 *                                         should be sorted by templateid     *
 *             del_interfaceids      - [OUT] sorted list of host interface    *
 *                                         ids which should be deleted        *
 *             del_snmp_interfaceids - [OUT] sorted list of host snmp         *
 *                                         interface ids which should be      *
 *                                         deleted                            *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_host_prototypes()         *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_interfaces_make(zbx_vector_ptr_t *host_prototypes,
		zbx_vector_uint64_t *del_interfaceids, zbx_vector_uint64_t *del_snmp_interfaceids)
{
	DB_RESULT			result;
	DB_ROW				row;
	char				*sql = NULL;
	size_t				sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t		hostids;
	zbx_uint64_t			hostid;
	zbx_host_prototype_t		*host_prototype;
	zbx_interfaces_prototype_t	*interface;
	int				i;

	zbx_vector_uint64_create(&hostids);

	/* select list of interfaces which should be linked to host prototypes */

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		zbx_vector_uint64_append(&hostids, host_prototype->templateid);
	}

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hi.hostid,hi.main,hi.type,hi.useip,hi.ip,hi.dns,hi.port,s.version,s.bulk,s.community,"
				"s.securityname,s.securitylevel,s.authpassphrase,s.privpassphrase,s.authprotocol,"
				"s.privprotocol,s.contextname"
			" from interface hi"
				" left join interface_snmp s"
					" on hi.interfaceid=s.interfaceid"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hi.hostid", hostids.values, hostids.values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hi.hostid");

	result = DBselect("%s", sql);
	host_prototype = NULL;

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(hostid, row[0]);

		if (NULL == host_prototype || host_prototype->templateid != hostid)
		{
			if (FAIL == (i = zbx_vector_ptr_bsearch(host_prototypes, &hostid,
					ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
			{
				THIS_SHOULD_NEVER_HAPPEN;
				continue;
			}

			host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];
		}

		interface = (zbx_interfaces_prototype_t *)zbx_malloc(NULL, sizeof(zbx_interfaces_prototype_t));
		interface->interfaceid = 0;
		ZBX_STR2UCHAR(interface->main, row[1]);
		ZBX_STR2UCHAR(interface->type, row[2]);
		ZBX_STR2UCHAR(interface->useip, row[3]);
		interface->ip = zbx_strdup(NULL, row[4]);
		interface->dns = zbx_strdup(NULL, row[5]);
		interface->port = zbx_strdup(NULL, row[6]);

		if (INTERFACE_TYPE_SNMP == interface->type)
		{
			zbx_interface_prototype_snmp_t	*snmp;

			snmp = (zbx_interface_prototype_snmp_t *)zbx_malloc(NULL,
					sizeof(zbx_interface_prototype_snmp_t));
			ZBX_STR2UCHAR(snmp->version, row[7]);
			ZBX_STR2UCHAR(snmp->bulk, row[8]);
			snmp->community = zbx_strdup(NULL, row[9]);
			snmp->securityname = zbx_strdup(NULL, row[10]);
			ZBX_STR2UCHAR(snmp->securitylevel, row[11]);
			snmp->authpassphrase = zbx_strdup(NULL, row[12]);
			snmp->privpassphrase = zbx_strdup(NULL, row[13]);
			ZBX_STR2UCHAR(snmp->authprotocol, row[14]);
			ZBX_STR2UCHAR(snmp->privprotocol, row[15]);
			snmp->contextname = zbx_strdup(NULL, row[16]);
			interface->data.snmp = snmp;
		}
		else
			interface->data.snmp = NULL;

		zbx_vector_interfaces_append(&host_prototype->interfaces, interface);
	}
	DBfree_result(result);

	/* select list of interfaces which are already linked to host prototypes */

	zbx_vector_uint64_clear(&hostids);

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		/* host prototype is not saved yet */
		if (0 == host_prototype->hostid)
			continue;

		zbx_vector_uint64_append(&hostids, host_prototype->hostid);
	}

	if (0 != hostids.values_num)
	{
		zbx_vector_uint64_sort(&hostids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select hi.interfaceid,hi.hostid,hi.main,hi.type,hi.useip,hi.ip,hi.dns,hi.port,"
					"s.version,s.bulk,s.community,s.securityname,s.securitylevel,s.authpassphrase,"
					"s.privpassphrase,s.authprotocol,s.privprotocol,s.contextname"
				" from interface hi"
					" left join interface_snmp s"
						" on hi.interfaceid=s.interfaceid"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hi.hostid", hostids.values, hostids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by hi.hostid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(hostid, row[1]);

			for (i = 0; i < host_prototypes->values_num; i++)
			{
				host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

				if (host_prototype->hostid == hostid)
				{
					unsigned char	type;
					uint64_t	interfaceid;

					ZBX_STR2UINT64(interfaceid, row[0]);
					ZBX_STR2UINT64(type, row[3]);

					if (INTERFACE_TYPE_SNMP == type)
					{
						if (FAIL == DBhost_prototypes_interface_make(
								&host_prototype->interfaces, interfaceid,
								(unsigned char)atoi(row[2]),	/* main */
								type,
								(unsigned char)atoi(row[4]),	/* useip */
								row[5],				/* ip */
								row[6],				/* dns */
								row[7],				/* port */
								(unsigned char)atoi(row[8]),	/* version */
								(unsigned char)atoi(row[9]),	/* bulk */
								row[10],			/* community */
								row[11],			/* securityname */
								(unsigned char)atoi(row[12]),	/* securitylevel */
								row[13],			/* authpassphrase */
								row[14],			/* privpassphrase */
								(unsigned char)atoi(row[15]),	/* authprotocol */
								(unsigned char)atoi(row[16]),	/* privprotocol */
								row[17]))			/* contextname */
						{
							zbx_vector_uint64_append(del_snmp_interfaceids, interfaceid);
						}
					}
					else
					{
						if (FAIL == DBhost_prototypes_interface_make(
								&host_prototype->interfaces, interfaceid,
								(unsigned char)atoi(row[2]),	/* main */
								type,
								(unsigned char)atoi(row[4]),	/* useip */
								row[5],				/* ip */
								row[6],				/* dns */
								row[7],				/* port */
								0, 0, NULL, NULL, 0, NULL, NULL, 0, 0, NULL))
						{
							zbx_vector_uint64_append(del_interfaceids, interfaceid);
						}
					}

					break;
				}
			}

			/* no interfaces found for this host prototype, but there must be at least one */
			if (i == host_prototypes->values_num)
				THIS_SHOULD_NEVER_HAPPEN;
		}
		DBfree_result(result);
	}

	zbx_vector_uint64_sort(del_interfaceids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
	zbx_vector_uint64_sort(del_snmp_interfaceids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	zbx_vector_uint64_destroy(&hostids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_interface_snmp_prepare_sql                     *
 *                                                                            *
 * Purpose: prepare sql for update record of interface_snmp table             *
 *                                                                            *
 * Parameters: interfaceid - [IN] snmp interface id;                          *
 *             snmp        - [IN] snmp interface prototypes for update        *
 *             sql         - [IN/OUT] sql string                              *
 *             sql_alloc   - [IN/OUT] size of sql string                      *
 *             sql_offset  - [IN/OUT] offset in sql string                    *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_interface_snmp_prepare_sql(const zbx_uint64_t interfaceid,
		const zbx_interface_prototype_snmp_t *snmp, char **sql, size_t *sql_alloc, size_t *sql_offset)
{
	const char	*d = "";
	char		*esc;

	zbx_strcpy_alloc(sql, sql_alloc, sql_offset, "update interface_snmp set ");

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_TYPE))
	{
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "version=%d", (int)snmp->version);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_BULK))
	{
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%sbulk=%d", d, (int)snmp->bulk);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_COMMUNITY))
	{
		esc = DBdyn_escape_string(snmp->community);
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%scommunity='%s'", d, esc);
		zbx_free(esc);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECNAME))
	{
		esc = DBdyn_escape_string(snmp->securityname);
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%ssecurityname='%s'", d, esc);
		zbx_free(esc);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_SECLEVEL))
	{
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%ssecuritylevel=%d", d, (int)snmp->securitylevel);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPASS))
	{
		esc = DBdyn_escape_string(snmp->authpassphrase);
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%sauthpassphrase='%s'", d, esc);
		zbx_free(esc);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPASS))
	{
		esc = DBdyn_escape_string(snmp->privpassphrase);
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%sprivpassphrase='%s'", d, esc);
		zbx_free(esc);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_AUTHPROTOCOL))
	{
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%sauthprotocol=%d", d, (int)snmp->authprotocol);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_PRIVPROTOCOL))
	{
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%sprivprotocol=%d", d, (int)snmp->privprotocol);
		d = ",";
	}

	if (0 != (snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE_CONTEXT))
	{
		esc = DBdyn_escape_string(snmp->contextname);
		zbx_snprintf_alloc(sql, sql_alloc, sql_offset, "%scontextname='%s'", d, esc);
		zbx_free(esc);
	}

	zbx_snprintf_alloc(sql, sql_alloc, sql_offset, " where interfaceid=" ZBX_FS_UI64 ";\n", interfaceid);
}

/******************************************************************************
 *                                                                            *
 * Function: DBhost_prototypes_save                                           *
 *                                                                            *
 * Purpose: auxiliary function for DBcopy_template_host_prototypes()          *
 *                                                                            *
 * Parameters: host_prototypes      - [IN] vector of host prototypes          *
 *             del_hosttemplateids  - [IN] host template ids for delete       *
 *             del_hostmacroids     - [IN] host macro ids for delete           *
 *             del_tagids           - [IN] tag ids for delete                 *
 *             del_interfaceids     - [IN] interface ids for delete           *
 *             del_snmpids          - [IN] SNMP interface ids for delete      *
 *                                                                            *
 ******************************************************************************/
static void	DBhost_prototypes_save(zbx_vector_ptr_t *host_prototypes, zbx_vector_uint64_t *del_hosttemplateids,
		zbx_vector_uint64_t *del_hostmacroids, zbx_vector_uint64_t *del_tagids,
		zbx_vector_uint64_t *del_interfaceids ,zbx_vector_uint64_t *del_snmpids)
{
	char				*sql1 = NULL, *sql2 = NULL, *name_esc, *value_esc;
	size_t				sql1_alloc = ZBX_KIBIBYTE, sql1_offset = 0,
					sql2_alloc = ZBX_KIBIBYTE, sql2_offset = 0;
	zbx_host_prototype_t		*host_prototype;
	zbx_group_prototype_t		*group_prototype;
	zbx_macros_prototype_t		*hostmacro;
	zbx_db_tag_t			*tag;
	zbx_interfaces_prototype_t	*interface;
	zbx_uint64_t			hostid = 0, hosttemplateid = 0, group_prototypeid = 0, hostmacroid = 0,
					interfaceid = 0;
	int				i, j, new_hosts = 0, new_hosts_templates = 0, new_group_prototypes = 0,
					upd_group_prototypes = 0, new_hostmacros = 0, upd_hostmacros = 0,
					new_tags = 0, new_interfaces = 0, upd_interfaces = 0, new_snmp = 0,
					upd_snmp = 0;
	zbx_db_insert_t			db_insert, db_insert_hdiscovery, db_insert_htemplates, db_insert_gproto,
					db_insert_hmacro, db_insert_tag, db_insert_iface, db_insert_snmp;
	zbx_vector_db_tag_ptr_t		upd_tags;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_db_tag_ptr_create(&upd_tags);

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		if (0 == host_prototype->hostid)
			new_hosts++;

		new_hosts_templates += host_prototype->lnk_templateids.values_num;

		for (j = 0; j < host_prototype->group_prototypes.values_num; j++)
		{
			group_prototype = (zbx_group_prototype_t *)host_prototype->group_prototypes.values[j];

			if (0 == group_prototype->group_prototypeid)
				new_group_prototypes++;
			else
				upd_group_prototypes++;
		}

		for (j = 0; j < host_prototype->hostmacros.values_num; j++)
		{
			hostmacro = host_prototype->hostmacros.values[j];

			if (0 == hostmacro->hostmacroid)
				new_hostmacros++;
			else if (0 != (hostmacro->flags & ZBX_FLAG_HPMACRO_UPDATE))
				upd_hostmacros++;
		}

		for (j = 0; j < host_prototype->tags.values_num; j++)
		{
			tag = host_prototype->tags.values[j];

			if (0 == tag->tagid)
				new_tags++;
			else if (0 != (tag->flags & ZBX_FLAG_DB_TAG_UPDATE))
				zbx_vector_db_tag_ptr_append(&upd_tags, tag);
		}

		for (j = 0; j < host_prototype->interfaces.values_num; j++)
		{
			interface = host_prototype->interfaces.values[j];

			if (0 == interface->interfaceid)
				new_interfaces++;
			else if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE))
				upd_interfaces++;

			if (INTERFACE_TYPE_SNMP == interface->type)
			{
				if (0 == interface->interfaceid)
					interface->data.snmp->flags |= ZBX_FLAG_HPINTERFACE_SNMP_CREATE;

				if (0 != (interface->data.snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_CREATE))
					new_snmp++;
				else if (0 != (interface->data.snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE))
					upd_snmp++;
			}
		}
	}

	if (0 != new_hosts)
	{
		hostid = DBget_maxid_num("hosts", new_hosts);

		zbx_db_insert_prepare(&db_insert, "hosts", "hostid", "host", "name", "status", "flags", "templateid",
				"discover", "custom_interfaces", NULL);

		zbx_db_insert_prepare(&db_insert_hdiscovery, "host_discovery", "hostid", "parent_itemid", NULL);
	}

	if (new_hosts != host_prototypes->values_num || 0 != upd_group_prototypes || 0 != upd_hostmacros ||
			0 != upd_tags.values_num)
	{
		sql1 = (char *)zbx_malloc(sql1, sql1_alloc);
		DBbegin_multiple_update(&sql1, &sql1_alloc, &sql1_offset);
	}

	if (0 != new_hosts_templates)
	{
		hosttemplateid = DBget_maxid_num("hosts_templates", new_hosts_templates);

		zbx_db_insert_prepare(&db_insert_htemplates, "hosts_templates",  "hosttemplateid", "hostid",
				"templateid", NULL);
	}

	if (0 != del_hosttemplateids->values_num)
	{
		sql2 = (char *)zbx_malloc(sql2, sql2_alloc);
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, "delete from hosts_templates where");
		DBadd_condition_alloc(&sql2, &sql2_alloc, &sql2_offset, "hosttemplateid",
				del_hosttemplateids->values, del_hosttemplateids->values_num);
	}

	if (0 != del_hostmacroids->values_num)
	{
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, "delete from hostmacro where");
		DBadd_condition_alloc(&sql2, &sql2_alloc, &sql2_offset, "hostmacroid",
				del_hostmacroids->values, del_hostmacroids->values_num);
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, ";\n");
	}

	if (0 != del_tagids->values_num)
	{
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, "delete from host_tag where");
		DBadd_condition_alloc(&sql2, &sql2_alloc, &sql2_offset, "hosttagid", del_tagids->values,
				del_tagids->values_num);
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, ";\n");
	}

	if (0 != del_snmpids->values_num)
	{
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, "delete from interface_snmp where");
		DBadd_condition_alloc(&sql2, &sql2_alloc, &sql2_offset, "interfaceid",
				del_snmpids->values, del_snmpids->values_num);
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, ";\n");
	}

	if (0 != del_interfaceids->values_num)
	{
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, "delete from interface where");
		DBadd_condition_alloc(&sql2, &sql2_alloc, &sql2_offset, "interfaceid",
				del_interfaceids->values, del_interfaceids->values_num);
		zbx_strcpy_alloc(&sql2, &sql2_alloc, &sql2_offset, ";\n");
	}

	if (0 != new_group_prototypes)
	{
		group_prototypeid = DBget_maxid_num("group_prototype", new_group_prototypes);

		zbx_db_insert_prepare(&db_insert_gproto, "group_prototype", "group_prototypeid", "hostid", "name",
				"groupid", "templateid", NULL);
	}

	if (0 != new_hostmacros)
	{
		hostmacroid = DBget_maxid_num("hostmacro", new_hostmacros);

		zbx_db_insert_prepare(&db_insert_hmacro, "hostmacro", "hostmacroid", "hostid", "macro", "value",
				"description", "type", NULL);
	}

	if (0 != new_tags)
		zbx_db_insert_prepare(&db_insert_tag, "host_tag", "hosttagid", "hostid", "tag", "value", NULL);

	if (0 != new_interfaces)
	{
		interfaceid = DBget_maxid_num("interface", new_interfaces);

		zbx_db_insert_prepare(&db_insert_iface, "interface", "interfaceid", "hostid", "main", "type",
				"useip", "ip", "dns", "port", NULL);
	}

	if (0 != new_snmp)
	{
		zbx_db_insert_prepare(&db_insert_snmp, "interface_snmp", "interfaceid", "version", "bulk", "community",
				"securityname", "securitylevel", "authpassphrase", "privpassphrase", "authprotocol",
				"privprotocol", "contextname", NULL);
	}

	for (i = 0; i < host_prototypes->values_num; i++)
	{
		host_prototype = (zbx_host_prototype_t *)host_prototypes->values[i];

		if (0 == host_prototype->hostid)
		{
			host_prototype->hostid = hostid++;

			zbx_db_insert_add_values(&db_insert, host_prototype->hostid, host_prototype->host,
					host_prototype->name, (int)host_prototype->status,
					(int)ZBX_FLAG_DISCOVERY_PROTOTYPE, host_prototype->templateid,
					(int)host_prototype->discover, (int)host_prototype->custom_interfaces);

			zabbix_log(LOG_LEVEL_INFORMATION, "HOST_PROTOTYPE_CREATE_ENTRY ADD: hostid ->%lu<-",
					host_prototype->hostid);
			zbx_audit_host_prototypes_create_entry(AUDIT_ACTION_ADD, host_prototype->hostid,
					host_prototype->name, host_prototype->status, host_prototype->templateid,
					host_prototype->discover, host_prototype->custom_interfaces);

			zbx_db_insert_add_values(&db_insert_hdiscovery, host_prototype->hostid, host_prototype->itemid);
		}
		else
		{
			zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "update hosts set templateid=" ZBX_FS_UI64,
					host_prototype->templateid);
			if (0 != (host_prototype->flags & ZBX_FLAG_HPLINK_UPDATE_NAME))
			{
				name_esc = DBdyn_escape_string(host_prototype->name);
				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, ",name='%s'", name_esc);
				zbx_free(name_esc);
			}
			if (0 != (host_prototype->flags & ZBX_FLAG_HPLINK_UPDATE_STATUS))
			{
				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, ",status=%d",
						host_prototype->status);
			}
			if (0 != (host_prototype->flags & ZBX_FLAG_HPLINK_UPDATE_DISCOVER))
			{
				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, ",discover=%d",
						host_prototype->discover);
			}
			if (0 != (host_prototype->flags & ZBX_FLAG_HPLINK_UPDATE_CUSTOM_INTERFACES))
			{
				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, ",custom_interfaces=%d",
						host_prototype->custom_interfaces);
			}
			zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, " where hostid=" ZBX_FS_UI64 ";\n",
					host_prototype->hostid);

			zabbix_log(LOG_LEVEL_INFORMATION, "HOST_PROTOTYPE_CREATE_ENTRY UPDATE: hostid ->%lu<-",
					host_prototype->hostid);

			zbx_audit_host_prototypes_create_entry(AUDIT_ACTION_UPDATE, host_prototype->hostid,
				host_prototype->name, host_prototype->status, host_prototype->templateid,
				host_prototype->discover, host_prototype->custom_interfaces);
		}

		DBexecute_overflowed_sql(&sql1, &sql1_alloc, &sql1_offset);

		for (j = 0; j < host_prototype->lnk_templateids.values_num; j++)
		{
			zbx_db_insert_add_values(&db_insert_htemplates, hosttemplateid++, host_prototype->hostid,
					host_prototype->lnk_templateids.values[j]);
		}

		for (j = 0; j < host_prototype->group_prototypes.values_num; j++)
		{
			char audit_key_operator[100];
			group_prototype = (zbx_group_prototype_t *)host_prototype->group_prototypes.values[j];

			zabbix_log(LOG_LEVEL_INFORMATION, "HHH, hostid: %lu, name: %s, groupid: %lu, templateid: %lu",
					host_prototype->hostid, group_prototype->name, group_prototype->groupid,
					group_prototype->templateid);

			if (0 == group_prototype->group_prototypeid)
			{
				zabbix_log(LOG_LEVEL_INFORMATION, "HUE_999: group_prototype is 0");

				zbx_db_insert_add_values(&db_insert_gproto, group_prototypeid++, host_prototype->hostid,
						group_prototype->name, group_prototype->groupid,
						group_prototype->templateid);
			}
			else
			{
				zabbix_log(LOG_LEVEL_INFORMATION, "HUE_999: group_prototype is NOT 0");

				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset,
						"update group_prototype"
						" set templateid=" ZBX_FS_UI64
						" where group_prototypeid=" ZBX_FS_UI64 ";\n",
						group_prototype->templateid, group_prototype->group_prototypeid);
			}

			if (0 != strlen(group_prototype->name))
			{
				zabbix_log(LOG_LEVEL_INFORMATION, "HUE2: name is not empty");
				zbx_snprintf(audit_key_operator, 100, "hostprototype.groupPrototypes[%s]",
						group_prototype->name);
				zbx_audit_update_json_uint64(host_prototype->hostid, audit_key_operator,
						group_prototype->templateid);

			}
			else if (0 != group_prototype->groupid)
			{
				zabbix_log(LOG_LEVEL_INFORMATION, "HUE3: groupid is not null");
				zbx_snprintf(audit_key_operator, 100, "hostprototype.groupLinks[%lu]",
						group_prototype->groupid);
				zbx_audit_update_json_uint64(host_prototype->hostid, audit_key_operator,
						group_prototype->templateid);
			}
		}

		for (j = 0; j < host_prototype->hostmacros.values_num; j++)
		{
			hostmacro = host_prototype->hostmacros.values[j];

			if (0 == hostmacro->hostmacroid)
			{
				zbx_db_insert_add_values(&db_insert_hmacro, hostmacroid++, host_prototype->hostid,
						hostmacro->macro, hostmacro->value, hostmacro->description,
						(int)hostmacro->type);
			}
			else if (0 != (hostmacro->flags & ZBX_FLAG_HPMACRO_UPDATE))
			{
				const char	*d = "";

				zbx_strcpy_alloc(&sql1, &sql1_alloc, &sql1_offset, "update hostmacro set ");

				if (0 != (hostmacro->flags & ZBX_FLAG_HPMACRO_UPDATE_VALUE))
				{
					value_esc = DBdyn_escape_string(hostmacro->value);
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "value='%s'", value_esc);
					zbx_free(value_esc);
					d = ",";
				}

				if (0 != (hostmacro->flags & ZBX_FLAG_HPMACRO_UPDATE_DESCRIPTION))
				{
					value_esc = DBdyn_escape_string(hostmacro->description);
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%sdescription='%s'",
							d, value_esc);
					zbx_free(value_esc);
					d = ",";
				}

				if (0 != (hostmacro->flags & ZBX_FLAG_HPMACRO_UPDATE_TYPE))
				{
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%stype=%d",
							d, hostmacro->type);
				}

				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset,
						" where hostmacroid=" ZBX_FS_UI64 ";\n", hostmacro->hostmacroid);
			}
		}

		for (j = 0; j < host_prototype->tags.values_num; j++)
		{
			tag = host_prototype->tags.values[j];

			if (0 == tag->tagid)
			{
				zbx_db_insert_add_values(&db_insert_tag, __UINT64_C(0), host_prototype->hostid,
						tag->tag, tag->value);
			}
		}

		for (j = 0; j < host_prototype->interfaces.values_num; j++)
		{
			interface = host_prototype->interfaces.values[j];

			if (0 == interface->interfaceid)
			{
				interface->interfaceid = interfaceid++;
				zbx_db_insert_add_values(&db_insert_iface, interface->interfaceid, host_prototype->hostid,
						(int)interface->main, (int)interface->type, (int)interface->useip,
						interface->ip, interface->dns, interface->port);
			}
			else if (0 != (interface->flags & ZBX_FLAG_HPMACRO_UPDATE))
			{
				const char	*d = "";

				zbx_strcpy_alloc(&sql1, &sql1_alloc, &sql1_offset, "update interface set ");

				if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE_MAIN))
				{
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%smain=%d", d,
							interface->main);
					d = ",";
				}

				if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE_TYPE))
				{
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%stype=%d", d,
							interface->type);
					d = ",";
				}

				if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE_USEIP))
				{
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%suseip=%d", d,
							interface->useip);
					d = ",";
				}

				if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE_IP))
				{
					value_esc = DBdyn_escape_string(interface->ip);
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%sip='%s'", d, value_esc);
					zbx_free(value_esc);
					d = ",";
				}

				if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE_DNS))
				{
					value_esc = DBdyn_escape_string(interface->dns);
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%sdns='%s'", d,
							value_esc);
					zbx_free(value_esc);
					d = ",";
				}

				if (0 != (interface->flags & ZBX_FLAG_HPINTERFACE_UPDATE_PORT))
				{
					value_esc = DBdyn_escape_string(interface->port);
					zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%sport='%s'", d,
							value_esc);
					zbx_free(value_esc);
				}

				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset,
						" where interfaceid=" ZBX_FS_UI64 ";\n", interface->interfaceid);
			}

			if (INTERFACE_TYPE_SNMP == interface->type)
			{
				if (0 != (interface->data.snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_CREATE))
				{
					zbx_db_insert_add_values(&db_insert_snmp, interface->interfaceid,
							(int)interface->data.snmp->version,
							(int)interface->data.snmp->bulk,
							interface->data.snmp->community,
							interface->data.snmp->securityname,
							(int)interface->data.snmp->securitylevel,
							interface->data.snmp->authpassphrase,
							interface->data.snmp->privpassphrase,
							(int)interface->data.snmp->authprotocol,
							(int)interface->data.snmp->privprotocol,
							interface->data.snmp->contextname);
				}
				else if (0 != (interface->data.snmp->flags & ZBX_FLAG_HPINTERFACE_SNMP_UPDATE))
				{
					DBhost_prototypes_interface_snmp_prepare_sql(interface->interfaceid,
							interface->data.snmp, &sql1, &sql1_alloc, &sql1_offset);
				}
			}
		}

		DBexecute_overflowed_sql(&sql1, &sql1_alloc, &sql1_offset);
	}

	if (0 != upd_tags.values_num)
	{
		zbx_vector_db_tag_ptr_sort(&upd_tags, ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC);

		for (i = 0; i < upd_tags.values_num; i++)
		{
			char	delim = ' ';

			tag = upd_tags.values[i];

			zbx_strcpy_alloc(&sql1, &sql1_alloc, &sql1_offset, "update host_tag set");

			if (0 != (tag->flags & ZBX_FLAG_DB_TAG_UPDATE_TAG))
			{
				value_esc = DBdyn_escape_string(tag->tag);
				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%ctag='%s'", delim,
						value_esc);
				zbx_free(value_esc);
				delim = ',';
			}

			if (0 != (tag->flags & ZBX_FLAG_DB_TAG_UPDATE_VALUE))
			{
				value_esc = DBdyn_escape_string(tag->value);
				zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset, "%cvalue='%s'", delim,
						value_esc);
				zbx_free(value_esc);
			}

			zbx_snprintf_alloc(&sql1, &sql1_alloc, &sql1_offset,
					" where hosttagid=" ZBX_FS_UI64 ";\n", tag->tagid);
		}
	}

	if (0 != new_hosts)
	{
		zbx_db_insert_execute(&db_insert);
		zbx_db_insert_clean(&db_insert);

		zbx_db_insert_execute(&db_insert_hdiscovery);
		zbx_db_insert_clean(&db_insert_hdiscovery);
	}

	if (0 != new_hosts_templates)
	{
		zbx_db_insert_execute(&db_insert_htemplates);
		zbx_db_insert_clean(&db_insert_htemplates);
	}

	if (0 != new_group_prototypes)
	{
		zbx_db_insert_execute(&db_insert_gproto);
		zbx_db_insert_clean(&db_insert_gproto);
	}

	if (0 != new_hostmacros)
	{
		zbx_db_insert_execute(&db_insert_hmacro);
		zbx_db_insert_clean(&db_insert_hmacro);
	}

	if (0 != new_tags)
	{
		zbx_db_insert_autoincrement(&db_insert_tag, "hosttagid");
		zbx_db_insert_execute(&db_insert_tag);
		zbx_db_insert_clean(&db_insert_tag);
	}

	if (0 != new_interfaces)
	{
		zbx_db_insert_execute(&db_insert_iface);
		zbx_db_insert_clean(&db_insert_iface);
	}

	if (0 != new_snmp)
	{
		zbx_db_insert_execute(&db_insert_snmp);
		zbx_db_insert_clean(&db_insert_snmp);
	}

	if (NULL != sql1 || new_hosts != host_prototypes->values_num || 0 != upd_group_prototypes ||
			0 != upd_hostmacros || 0 != upd_interfaces || 0 != upd_snmp)
	{
		DBend_multiple_update(&sql1, &sql1_alloc, &sql1_offset);

		/* in ORACLE always present begin..end; */
		if (16 < sql1_offset)
			DBexecute("%s", sql1);
		zbx_free(sql1);
	}

	if (0 != del_hosttemplateids->values_num || 0 != del_hostmacroids->values_num || 0 != del_tagids->values_num ||
			0 != del_interfaceids->values_num || 0 != del_snmpids->values_num)
	{
		DBexecute("%s", sql2);
		zbx_free(sql2);
	}

	zbx_vector_db_tag_ptr_destroy(&upd_tags);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_template_host_prototypes                                  *
 *                                                                            *
 * Purpose: copy host prototypes from templates and create links between      *
 *          them and discovery rules                                          *
 *                                                                            *
 * Comments: auxiliary function for DBcopy_template_elements()                *
 *                                                                            *
 ******************************************************************************/
static void	DBcopy_template_host_prototypes(zbx_uint64_t hostid, zbx_vector_uint64_t *templateids)
{
	zbx_vector_ptr_t	host_prototypes;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	/* only regular hosts can have host prototypes */
	if (SUCCEED != DBis_regular_host(hostid))
		return;

	zbx_vector_ptr_create(&host_prototypes);

	DBhost_prototypes_make(hostid, templateids, &host_prototypes);

	if (0 != host_prototypes.values_num)
	{
		zbx_vector_uint64_t	del_hosttemplateids, del_group_prototypeids, del_macroids, del_tagids,
					del_interfaceids, del_snmp_interfaceids;

		zbx_vector_uint64_create(&del_hosttemplateids);
		zbx_vector_uint64_create(&del_group_prototypeids);
		zbx_vector_uint64_create(&del_macroids);
		zbx_vector_uint64_create(&del_tagids);
		zbx_vector_uint64_create(&del_interfaceids);
		zbx_vector_uint64_create(&del_snmp_interfaceids);

		DBhost_prototypes_templates_make(&host_prototypes, &del_hosttemplateids);
		DBhost_prototypes_groups_make(&host_prototypes, &del_group_prototypeids);
		DBhost_prototypes_macros_make(&host_prototypes, &del_macroids);
		DBhost_prototypes_tags_make(&host_prototypes, &del_tagids);
		DBhost_prototypes_interfaces_make(&host_prototypes, &del_interfaceids, &del_snmp_interfaceids);
		DBhost_prototypes_save(&host_prototypes, &del_hosttemplateids, &del_macroids, &del_tagids,
				&del_interfaceids, &del_snmp_interfaceids);
		DBgroup_prototypes_delete(&del_group_prototypeids);

		zbx_vector_uint64_destroy(&del_tagids);
		zbx_vector_uint64_destroy(&del_macroids);
		zbx_vector_uint64_destroy(&del_group_prototypeids);
		zbx_vector_uint64_destroy(&del_snmp_interfaceids);
		zbx_vector_uint64_destroy(&del_interfaceids);
		zbx_vector_uint64_destroy(&del_hosttemplateids);
	}

	DBhost_prototypes_clean(&host_prototypes);
	zbx_vector_ptr_destroy(&host_prototypes);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_template_triggers                                         *
 *                                                                            *
 * Purpose: Copy template triggers to host                                    *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Return value: upon successful completion return SUCCEED                    *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static int	DBcopy_template_triggers(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 512, sql_offset = 0;
	DB_RESULT		result;
	DB_ROW			row;
	zbx_uint64_t		triggerid, new_triggerid, cur_triggerid;
	int			res = SUCCEED;
	zbx_vector_uint64_t	new_triggerids, cur_triggerids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&new_triggerids);
	zbx_vector_uint64_create(&cur_triggerids);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct t.triggerid,t.description,t.expression,t.status,"
				"t.type,t.priority,t.comments,t.url,t.flags,t.recovery_expression,t.recovery_mode,"
				"t.correlation_mode,t.correlation_tag,t.manual_close,t.opdata,t.discover,t.event_name"
			" from triggers t,functions f,items i"
			" where t.triggerid=f.triggerid"
				" and f.itemid=i.itemid"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i.hostid", templateids->values, templateids->values_num);

	result = DBselect("%s", sql);

	zbx_free(sql);

	while (SUCCEED == res && NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(triggerid, row[0]);

		res = DBcopy_trigger_to_host(&new_triggerid, &cur_triggerid, hostid, triggerid,
				row[1],				/* description */
				row[2],				/* expression */
				row[9],				/* recovery_expression */
				(unsigned char)atoi(row[10]),	/* recovery_mode */
				(unsigned char)atoi(row[3]),	/* status */
				(unsigned char)atoi(row[4]),	/* type */
				(unsigned char)atoi(row[5]),	/* priority */
				row[6],				/* comments */
				row[7],				/* url */
				(unsigned char)atoi(row[8]),	/* flags */
				(unsigned char)atoi(row[11]),	/* correlation_mode */
				row[12],			/* correlation_tag */
				(unsigned char)atoi(row[13]),	/* manual_close */
				row[14],			/* opdata */
				(unsigned char)atoi(row[15]),	/* discover */
				row[16]);			/* event_name */

		if (0 != new_triggerid)				/* new trigger added */
			zbx_vector_uint64_append(&new_triggerids, new_triggerid);
		else
			zbx_vector_uint64_append(&cur_triggerids, cur_triggerid);
	}
	DBfree_result(result);

	if (SUCCEED == res)
		res = DBadd_template_dependencies_for_new_triggers(hostid, new_triggerids.values, new_triggerids.values_num);

	if (SUCCEED == res)
		res = DBcopy_template_trigger_tags(&new_triggerids, &cur_triggerids);

	zbx_vector_uint64_destroy(&cur_triggerids);
	zbx_vector_uint64_destroy(&new_triggerids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(res));

	return res;
}

/******************************************************************************
 *                                                                            *
 * Function: DBget_same_itemid                                                *
 *                                                                            *
 * Purpose: get same itemid for selected host by itemid from template         *
 *                                                                            *
 * Parameters: hostid - host identificator from database                      *
 *             itemid - item identificator from database (from template)      *
 *                                                                            *
 * Return value: new item identificator or zero if item not found             *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static zbx_uint64_t	DBget_same_itemid(zbx_uint64_t hostid, zbx_uint64_t titemid)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	itemid = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() hostid:" ZBX_FS_UI64
			" titemid:" ZBX_FS_UI64,
			__func__, hostid, titemid);

	result = DBselect(
			"select hi.itemid"
			" from items hi,items ti"
			" where hi.key_=ti.key_"
				" and hi.hostid=" ZBX_FS_UI64
				" and ti.itemid=" ZBX_FS_UI64,
			hostid, titemid);

	if (NULL != (row = DBfetch(result)))
		ZBX_STR2UINT64(itemid, row[0]);
	DBfree_result(result);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():" ZBX_FS_UI64, __func__, itemid);

	return itemid;
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_graph_to_host                                             *
 *                                                                            *
 * Purpose: copy specified graph to host                                      *
 *                                                                            *
 * Parameters: graphid - graph identificator from database                    *
 *             hostid - host identificator from database                      *
 *                                                                            *
 * Author: Eugene Grigorjev, Alexander Vladishev                              *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBcopy_graph_to_host(zbx_uint64_t hostid, zbx_uint64_t graphid,
		const char *name, int width, int height, double yaxismin,
		double yaxismax, unsigned char show_work_period,
		unsigned char show_triggers, unsigned char graphtype,
		unsigned char show_legend, unsigned char show_3d,
		double percent_left, double percent_right,
		unsigned char ymin_type, unsigned char ymax_type,
		zbx_uint64_t ymin_itemid, zbx_uint64_t ymax_itemid,
		unsigned char flags, unsigned char discover)
{
	DB_RESULT	result;
	DB_ROW		row;
	ZBX_GRAPH_ITEMS *gitems = NULL, *chd_gitems = NULL;
	size_t		gitems_alloc = 0, gitems_num = 0,
			chd_gitems_alloc = 0, chd_gitems_num = 0;
	zbx_uint64_t	hst_graphid, hst_gitemid;
	char		*sql = NULL, *name_esc, *color_esc;
	size_t		sql_alloc = ZBX_KIBIBYTE, sql_offset, i;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	sql = (char *)zbx_malloc(sql, sql_alloc * sizeof(char));

	name_esc = DBdyn_escape_string(name);

	sql_offset = 0;
	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select 0,dst.itemid,dst.key_,gi.drawtype,gi.sortorder,gi.color,gi.yaxisside,gi.calc_fnc,"
				"gi.type,i.flags"
			" from graphs_items gi,items i,items dst"
			" where gi.itemid=i.itemid"
				" and i.key_=dst.key_"
				" and gi.graphid=" ZBX_FS_UI64
				" and dst.hostid=" ZBX_FS_UI64
			" order by dst.key_",
			graphid, hostid);

	DBget_graphitems(sql, &gitems, &gitems_alloc, &gitems_num);

	result = DBselect(
			"select distinct g.graphid"
			" from graphs g,graphs_items gi,items i"
			" where g.graphid=gi.graphid"
				" and gi.itemid=i.itemid"
				" and i.hostid=" ZBX_FS_UI64
				" and g.name='%s'"
				" and g.templateid is null",
			hostid, name_esc);

	/* compare graphs */
	hst_graphid = 0;
	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(hst_graphid, row[0]);

		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select gi.gitemid,i.itemid,i.key_,gi.drawtype,gi.sortorder,gi.color,gi.yaxisside,"
					"gi.calc_fnc,gi.type,i.flags"
				" from graphs_items gi,items i"
				" where gi.itemid=i.itemid"
					" and gi.graphid=" ZBX_FS_UI64
				" order by i.key_",
				hst_graphid);

		DBget_graphitems(sql, &chd_gitems, &chd_gitems_alloc, &chd_gitems_num);

		if (SUCCEED == DBcmp_graphitems(gitems, gitems_num, chd_gitems, chd_gitems_num))
			break;	/* found equal graph */

		hst_graphid = 0;
	}
	DBfree_result(result);

	sql_offset = 0;
	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	if (GRAPH_YAXIS_TYPE_ITEM_VALUE == ymin_type)
		ymin_itemid = DBget_same_itemid(hostid, ymin_itemid);
	else
		ymin_itemid = 0;

	if (GRAPH_YAXIS_TYPE_ITEM_VALUE == ymax_type)
		ymax_itemid = DBget_same_itemid(hostid, ymax_itemid);
	else
		ymax_itemid = 0;

	if (0 != hst_graphid)
	{
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"update graphs"
				" set name='%s',"
					"width=%d,"
					"height=%d,"
					"yaxismin=" ZBX_FS_DBL64_SQL ","
					"yaxismax=" ZBX_FS_DBL64_SQL ","
					"templateid=" ZBX_FS_UI64 ","
					"show_work_period=%d,"
					"show_triggers=%d,"
					"graphtype=%d,"
					"show_legend=%d,"
					"show_3d=%d,"
					"percent_left=" ZBX_FS_DBL64_SQL ","
					"percent_right=" ZBX_FS_DBL64_SQL ","
					"ymin_type=%d,"
					"ymax_type=%d,"
					"ymin_itemid=%s,"
					"ymax_itemid=%s,"
					"flags=%d,"
					"discover=%d"
				" where graphid=" ZBX_FS_UI64 ";\n",
				name_esc, width, height, yaxismin, yaxismax,
				graphid, (int)show_work_period, (int)show_triggers,
				(int)graphtype, (int)show_legend, (int)show_3d,
				percent_left, percent_right, (int)ymin_type, (int)ymax_type,
				DBsql_id_ins(ymin_itemid), DBsql_id_ins(ymax_itemid), (int)flags, (int)discover,
				hst_graphid);

		zbx_audit_graphs_create_entry(AUDIT_ACTION_UPDATE, hst_graphid, name_esc, width, height, yaxismin,
				yaxismax, graphid, show_work_period, show_triggers, graphtype, show_legend, show_3d,
				percent_left, percent_right, ymin_type, ymax_type, ymin_itemid, ymax_itemid, flags,
				discover);

		for (i = 0; i < gitems_num; i++)
		{
			char audit_key_drawtype[100];
			char audit_key_sortorder[100];
			char audit_key_color[100];
			char audit_key_yaxisside[100];
			char audit_key_calc_fnc[100];
			char audit_key_type[100];

			if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
			{
				zbx_snprintf(audit_key_drawtype, 100, "graph.gitems[%lu].drawtype",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_sortorder, 100, "graph.gitems[%lu].sortorder",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_color, 100, "graph.gitems[%lu].color", chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_yaxisside, 100, "graph.gitems[%lu].yaxisside",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_calc_fnc, 100, "graph.gitems[%lu].calc_fnc",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_type, 100, "graph.gitems[%lu].type", chd_gitems[i].gitemid);
			}
			else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			{
				zbx_snprintf(audit_key_drawtype, 100, "graphprototype.gitems[%lu].drawtype",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_sortorder, 100, "graphprototype.gitems[%lu].sortorder",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_color, 100, "graphprototype.gitems[%lu].color",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_yaxisside, 100, "graphprototype.gitems[%lu].yaxisside",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_calc_fnc, 100, "graphprototype.gitems[%lu].calc_fnc",
						chd_gitems[i].gitemid);
				zbx_snprintf(audit_key_type, 100, "graphprototype.gitems[%lu].type",
						chd_gitems[i].gitemid);
			}
			color_esc = DBdyn_escape_string(gitems[i].color);

			zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
					"update graphs_items"
					" set drawtype=%d,"
						"sortorder=%d,"
						"color='%s',"
						"yaxisside=%d,"
						"calc_fnc=%d,"
						"type=%d"
					" where gitemid=" ZBX_FS_UI64 ";\n",
					gitems[i].drawtype,
					gitems[i].sortorder,
					color_esc,
					gitems[i].yaxisside,
					gitems[i].calc_fnc,
					gitems[i].type,
					chd_gitems[i].gitemid);

			if (ZBX_FLAG_DISCOVERY_NORMAL == flags || ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			{
				zbx_audit_update_json_uint64(hst_graphid, audit_key_drawtype, gitems[i].drawtype);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_sortorder, gitems[i].sortorder);
				zbx_audit_update_json_string(hst_graphid, audit_key_color, color_esc);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_yaxisside, gitems[i].yaxisside);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_calc_fnc, gitems[i].calc_fnc);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_type, gitems[i].type);
			}

			zbx_free(color_esc);
		}
	}
	else
	{
		hst_graphid = DBget_maxid("graphs");

		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"insert into graphs"
				" (graphid,name,width,height,yaxismin,yaxismax,templateid,"
				"show_work_period,show_triggers,graphtype,show_legend,"
				"show_3d,percent_left,percent_right,ymin_type,ymax_type,"
				"ymin_itemid,ymax_itemid,flags,discover)"
				" values (" ZBX_FS_UI64 ",'%s',%d,%d," ZBX_FS_DBL64_SQL ","
				ZBX_FS_DBL64_SQL "," ZBX_FS_UI64 ",%d,%d,%d,%d,%d," ZBX_FS_DBL64_SQL ","
				ZBX_FS_DBL64_SQL ",%d,%d,%s,%s,%d,%d);\n",
				hst_graphid, name_esc, width, height, yaxismin, yaxismax,
				graphid, (int)show_work_period, (int)show_triggers,
				(int)graphtype, (int)show_legend, (int)show_3d,
				percent_left, percent_right, (int)ymin_type, (int)ymax_type,
				DBsql_id_ins(ymin_itemid), DBsql_id_ins(ymax_itemid), (int)flags, (int)discover);

		zbx_audit_graphs_create_entry(AUDIT_ACTION_ADD, hst_graphid, name_esc, width, height, yaxismin,
				yaxismax, graphid, show_work_period, show_triggers, graphtype, show_legend, show_3d,
				percent_left, percent_right, ymin_type, ymax_type, ymin_itemid, ymax_itemid, flags,
				discover);

		hst_gitemid = DBget_maxid_num("graphs_items", gitems_num);

		for (i = 0; i < gitems_num; i++)
		{
			char audit_key_drawtype[100];
			char audit_key_sortorder[100];
			char audit_key_color[100];
			char audit_key_yaxisside[100];
			char audit_key_calc_fnc[100];
			char audit_key_type[100];

			if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
			{
				zbx_snprintf(audit_key_drawtype, 100, "graph.gitems[%lu].drawtype", hst_gitemid);
				zbx_snprintf(audit_key_sortorder, 100, "graph.gitems[%lu].sortorder", hst_gitemid);
				zbx_snprintf(audit_key_color, 100, "graph.gitems[%lu].color", hst_gitemid);
				zbx_snprintf(audit_key_yaxisside, 100, "graph.gitems[%lu].yaxisside", hst_gitemid);
				zbx_snprintf(audit_key_calc_fnc, 100, "graph.gitems[%lu].calc_fnc", hst_gitemid);
				zbx_snprintf(audit_key_type, 100, "graph.gitems[%lu].type", hst_gitemid);
			}
			else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			{
				zbx_snprintf(audit_key_drawtype, 100, "graphprototype.gitems[%lu].drawtype",
						hst_gitemid);
				zbx_snprintf(audit_key_sortorder, 100, "graphprototype.gitems[%lu].sortorder",
						hst_gitemid);
				zbx_snprintf(audit_key_color, 100, "graphprototype.gitems[%lu].color", hst_gitemid);
				zbx_snprintf(audit_key_yaxisside, 100, "graphprototype.gitems[%lu].yaxisside",
						hst_gitemid);
				zbx_snprintf(audit_key_calc_fnc, 100, "graphprototype.gitems[%lu].calc_fnc",
						hst_gitemid);
				zbx_snprintf(audit_key_type, 100, "graphprototype.gitems[%lu].type", hst_gitemid);
			}

			color_esc = DBdyn_escape_string(gitems[i].color);

			zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
					"insert into graphs_items (gitemid,graphid,itemid,drawtype,"
					"sortorder,color,yaxisside,calc_fnc,type)"
					" values (" ZBX_FS_UI64 "," ZBX_FS_UI64 "," ZBX_FS_UI64
					",%d,%d,'%s',%d,%d,%d);\n",
					hst_gitemid, hst_graphid, gitems[i].itemid,
					gitems[i].drawtype, gitems[i].sortorder, color_esc,
					gitems[i].yaxisside, gitems[i].calc_fnc, gitems[i].type);

			if (ZBX_FLAG_DISCOVERY_NORMAL == flags || ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			{
				zbx_audit_update_json_uint64(hst_graphid, audit_key_drawtype, gitems[i].drawtype);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_sortorder, gitems[i].sortorder);
				zbx_audit_update_json_string(hst_graphid, audit_key_color, color_esc);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_yaxisside, gitems[i].yaxisside);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_calc_fnc, gitems[i].calc_fnc);
				zbx_audit_update_json_uint64(hst_graphid, audit_key_type, gitems[i].type);
			}

			hst_gitemid++;

			zbx_free(color_esc);
		}
	}

	zbx_free(name_esc);

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	if (sql_offset > 16)	/* In ORACLE always present begin..end; */
		DBexecute("%s", sql);

	zbx_free(gitems);
	zbx_free(chd_gitems);
	zbx_free(sql);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_template_graphs                                           *
 *                                                                            *
 * Purpose: copy graphs from template to host                                 *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 * Author: Eugene Grigorjev                                                   *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *                                                                            *
 ******************************************************************************/
static void	DBcopy_template_graphs(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	char		*sql = NULL;
	size_t		sql_alloc = 512, sql_offset = 0;
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	graphid, ymin_itemid, ymax_itemid;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select distinct g.graphid,g.name,g.width,g.height,g.yaxismin,"
				"g.yaxismax,g.show_work_period,g.show_triggers,"
				"g.graphtype,g.show_legend,g.show_3d,g.percent_left,"
				"g.percent_right,g.ymin_type,g.ymax_type,g.ymin_itemid,"
				"g.ymax_itemid,g.flags,g.discover"
			" from graphs g,graphs_items gi,items i"
			" where g.graphid=gi.graphid"
				" and gi.itemid=i.itemid"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i.hostid", templateids->values, templateids->values_num);

	result = DBselect("%s", sql);

	zbx_free(sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(graphid, row[0]);
		ZBX_DBROW2UINT64(ymin_itemid, row[15]);
		ZBX_DBROW2UINT64(ymax_itemid, row[16]);

		DBcopy_graph_to_host(hostid, graphid,
				row[1],				/* name */
				atoi(row[2]),			/* width */
				atoi(row[3]),			/* height */
				atof(row[4]),			/* yaxismin */
				atof(row[5]),			/* yaxismax */
				(unsigned char)atoi(row[6]),	/* show_work_period */
				(unsigned char)atoi(row[7]),	/* show_triggers */
				(unsigned char)atoi(row[8]),	/* graphtype */
				(unsigned char)atoi(row[9]),	/* show_legend */
				(unsigned char)atoi(row[10]),	/* show_3d */
				atof(row[11]),			/* percent_left */
				atof(row[12]),			/* percent_right */
				(unsigned char)atoi(row[13]),	/* ymin_type */
				(unsigned char)atoi(row[14]),	/* ymax_type */
				ymin_itemid,
				ymax_itemid,
				(unsigned char)atoi(row[17]),	/* flags */
				(unsigned char)atoi(row[18]));	/* discover */
	}
	DBfree_result(result);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

typedef struct
{
	zbx_uint64_t		t_itemid;
	zbx_uint64_t		h_itemid;
	unsigned char		type;
}
httpstepitem_t;

typedef struct
{
	zbx_uint64_t		httpstepid;
	char			*name;
	char			*url;
	char			*posts;
	char			*required;
	char			*status_codes;
	zbx_vector_ptr_t	httpstepitems;
	zbx_vector_ptr_t	fields;
	char			*timeout;
	int			no;
	int			follow_redirects;
	int			retrieve_mode;
	int			post_type;
}
httpstep_t;

typedef struct
{
	zbx_uint64_t		httptesttagid;
	char			*tag;
	char			*value;
}
httptesttag_t;

typedef struct
{
	zbx_uint64_t		t_itemid;
	zbx_uint64_t		h_itemid;
	unsigned char		type;
}
httptestitem_t;

typedef struct
{
	zbx_uint64_t		templateid;
	zbx_uint64_t		httptestid;
	char			*name;
	char			*delay;
	zbx_vector_ptr_t	fields;
	char			*agent;
	char			*http_user;
	char			*http_password;
	char			*http_proxy;
	zbx_vector_ptr_t	httpsteps;
	zbx_vector_ptr_t	httptestitems;
	zbx_vector_ptr_t	httptesttags;
	int			retries;
	unsigned char		status;
	unsigned char		authentication;
}
httptest_t;

typedef struct
{
	int			type;
	char			*name;
	char			*value;
}
httpfield_t;

/******************************************************************************
 *                                                                            *
 * Function: DBget_httptests                                                  *
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
static void	DBget_httptests(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids, zbx_vector_ptr_t *httptests)
{
	char			*sql = NULL;
	size_t			sql_alloc = 512, sql_offset = 0;
	DB_RESULT		result;
	DB_ROW			row;
	httptest_t		*httptest;
	httpstep_t		*httpstep;
	httptesttag_t		*httptesttag;
	httpfield_t		*httpfield;
	httptestitem_t		*httptestitem;
	httpstepitem_t		*httpstepitem;
	zbx_vector_uint64_t	httptestids;	/* the list of web scenarios which should be added to a host */
	zbx_vector_uint64_t	items;
	zbx_uint64_t		httptestid, httpstepid, itemid;
	int			i, j, k;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&httptestids);
	zbx_vector_uint64_create(&items);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select t.httptestid,t.name,t.delay,t.status,t.agent,t.authentication,"
				"t.http_user,t.http_password,t.http_proxy,t.retries,h.httptestid"
			" from httptest t"
				" left join httptest h"
					" on h.hostid=" ZBX_FS_UI64
						" and h.name=t.name"
			" where", hostid);
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.hostid", templateids->values, templateids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by t.httptestid");

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		httptest = (httptest_t *)zbx_calloc(NULL, 1, sizeof(httptest_t));

		ZBX_STR2UINT64(httptest->templateid, row[0]);
		ZBX_DBROW2UINT64(httptest->httptestid, row[11]);
		zbx_vector_ptr_create(&httptest->httpsteps);
		zbx_vector_ptr_create(&httptest->httptestitems);
		zbx_vector_ptr_create(&httptest->fields);
		zbx_vector_ptr_create(&httptest->httptesttags);

		zbx_vector_ptr_append(httptests, httptest);

		if (0 == httptest->httptestid)
		{
			httptest->name = zbx_strdup(NULL, row[1]);
			httptest->delay = zbx_strdup(NULL, row[2]);
			httptest->status = (unsigned char)atoi(row[3]);
			httptest->agent = zbx_strdup(NULL, row[4]);
			httptest->authentication = (unsigned char)atoi(row[5]);
			httptest->http_user = zbx_strdup(NULL, row[6]);
			httptest->http_password = zbx_strdup(NULL, row[7]);
			httptest->http_proxy = zbx_strdup(NULL, row[8]);
			httptest->retries = atoi(row[9]);

			zbx_vector_uint64_append(&httptestids, httptest->templateid);
		}
	}
	DBfree_result(result);

	if (0 != httptestids.values_num)
	{
		httptest = NULL;

		/* web scenario fields */
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select httptestid,type,name,value"
				" from httptest_field"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "httptestid",
				httptestids.values, httptestids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by httptestid,httptest_fieldid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(httptestid, row[0]);

			if (NULL == httptest || httptest->templateid != httptestid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(httptests, &httptestid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httptest = (httptest_t *)httptests->values[i];
			}

			httpfield = (httpfield_t *)zbx_malloc(NULL, sizeof(httpfield_t));

			httpfield->type = atoi(row[1]);
			httpfield->name = zbx_strdup(NULL, row[2]);
			httpfield->value = zbx_strdup(NULL, row[3]);

			zbx_vector_ptr_append(&httptest->fields, httpfield);
		}
		DBfree_result(result);

		/* web scenario steps */
		httptest = NULL;

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select httpstepid,httptestid,name,no,url,timeout,posts,required,status_codes,"
					"follow_redirects,retrieve_mode,post_type"
				" from httpstep"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "httptestid",
				httptestids.values, httptestids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by httptestid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(httptestid, row[1]);

			if (NULL == httptest || httptest->templateid != httptestid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(httptests, &httptestid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httptest = (httptest_t *)httptests->values[i];
			}

			httpstep = (httpstep_t *)zbx_malloc(NULL, sizeof(httptest_t));

			ZBX_STR2UINT64(httpstep->httpstepid, row[0]);
			httpstep->name = zbx_strdup(NULL, row[2]);
			httpstep->no = atoi(row[3]);
			httpstep->url = zbx_strdup(NULL, row[4]);
			httpstep->timeout = zbx_strdup(NULL, row[5]);
			httpstep->posts = zbx_strdup(NULL, row[6]);
			httpstep->required = zbx_strdup(NULL, row[7]);
			httpstep->status_codes = zbx_strdup(NULL, row[8]);
			httpstep->follow_redirects = atoi(row[9]);
			httpstep->retrieve_mode = atoi(row[10]);
			httpstep->post_type = atoi(row[11]);
			zbx_vector_ptr_create(&httpstep->httpstepitems);
			zbx_vector_ptr_create(&httpstep->fields);

			zbx_vector_ptr_append(&httptest->httpsteps, httpstep);
		}
		DBfree_result(result);

		for (i = 0; i < httptests->values_num; i++)
		{
			httptest = (httptest_t *)httptests->values[i];
			zbx_vector_ptr_sort(&httptest->httpsteps, ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC);
		}

		/* web scenario step fields */
		httptest = NULL;

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select s.httptestid,f.httpstepid,f.type,f.name,f.value"
				" from httpstep_field f"
					" join httpstep s"
						" on f.httpstepid=s.httpstepid"
							" and");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "s.httptestid",
				httptestids.values, httptestids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				" order by s.httptestid,f.httpstepid,f.httpstep_fieldid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(httptestid, row[0]);
			ZBX_STR2UINT64(httpstepid, row[1]);

			if (NULL == httptest || httptest->templateid != httptestid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(httptests, &httptestid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httptest = (httptest_t *)httptests->values[i];
				httpstep = NULL;
			}

			if (NULL == httpstep || httpstep->httpstepid != httpstepid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(&httptest->httpsteps, &httpstepid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httpstep = (httpstep_t *)httptest->httpsteps.values[i];
			}

			httpfield = (httpfield_t *)zbx_malloc(NULL, sizeof(httpfield_t));

			httpfield->type = atoi(row[2]);
			httpfield->name = zbx_strdup(NULL, row[3]);
			httpfield->value = zbx_strdup(NULL, row[4]);

			zbx_vector_ptr_append(&httpstep->fields, httpfield);
		}
		DBfree_result(result);

		/* web scenario tags */
		httptest = NULL;

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select httptesttagid,httptestid,tag,value"
				" from httptest_tag"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "httptestid",
				httptestids.values, httptestids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, " order by httptestid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(httptestid, row[1]);

			if (NULL == httptest || httptest->templateid != httptestid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(httptests, &httptestid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httptest = (httptest_t *)httptests->values[i];
			}

			httptesttag = (httptesttag_t *)zbx_malloc(NULL, sizeof(httptesttag_t));

			ZBX_STR2UINT64(httptesttag->httptesttagid, row[0]);
			httptesttag->tag = zbx_strdup(NULL, row[2]);
			httptesttag->value = zbx_strdup(NULL, row[3]);

			zbx_vector_ptr_append(&httptest->httptesttags, httptesttag);
		}
		DBfree_result(result);

		for (i = 0; i < httptests->values_num; i++)
		{
			httptest = (httptest_t *)httptests->values[i];
			zbx_vector_ptr_sort(&httptest->httptesttags, ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC);
		}
	}

	/* web scenario items */
	if (0 != httptestids.values_num)
	{
		httptest = NULL;

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select httptestid,itemid,type"
				" from httptestitem"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "httptestid",
				httptestids.values, httptestids.values_num);

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(httptestid, row[0]);

			if (NULL == httptest || httptest->templateid != httptestid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(httptests, &httptestid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httptest = (httptest_t *)httptests->values[i];
			}

			httptestitem = (httptestitem_t *)zbx_calloc(NULL, 1, sizeof(httptestitem_t));

			ZBX_STR2UINT64(httptestitem->t_itemid, row[1]);
			httptestitem->type = (unsigned char)atoi(row[2]);

			zbx_vector_ptr_append(&httptest->httptestitems, httptestitem);

			zbx_vector_uint64_append(&items, httptestitem->t_itemid);
		}
		DBfree_result(result);
	}

	/* web scenario step items */
	if (0 != httptestids.values_num)
	{
		httptest = NULL;
		httpstep = NULL;

		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select hs.httptestid,hsi.httpstepid,hsi.itemid,hsi.type"
				" from httpstepitem hsi"
					" join httpstep hs"
						" on");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hs.httptestid",
				httptestids.values, httptestids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
							" and hs.httpstepid=hsi.httpstepid"
				" order by hs.httptestid,hsi.httpstepid");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(httptestid, row[0]);
			ZBX_STR2UINT64(httpstepid, row[1]);

			if (NULL == httptest || httptest->templateid != httptestid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(httptests, &httptestid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httptest = (httptest_t *)httptests->values[i];
				httpstep = NULL;
			}

			if (NULL == httpstep || httpstep->httpstepid != httpstepid)
			{
				if (FAIL == (i = zbx_vector_ptr_bsearch(&httptest->httpsteps, &httpstepid,
						ZBX_DEFAULT_UINT64_PTR_COMPARE_FUNC)))
				{
					THIS_SHOULD_NEVER_HAPPEN;
					continue;
				}

				httpstep = (httpstep_t *)httptest->httpsteps.values[i];
			}

			httpstepitem = (httpstepitem_t *)zbx_calloc(NULL, 1, sizeof(httpstepitem_t));

			ZBX_STR2UINT64(httpstepitem->t_itemid, row[2]);
			httpstepitem->type = (unsigned char)atoi(row[3]);

			zbx_vector_ptr_append(&httpstep->httpstepitems, httpstepitem);

			zbx_vector_uint64_append(&items, httpstepitem->t_itemid);
		}
		DBfree_result(result);
	}

	/* items */
	if (0 != items.values_num)
	{
		zbx_vector_uint64_sort(&items, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

		sql_offset = 0;
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				"select t.itemid,h.itemid"
				" from items t"
					" join items h"
						" on h.hostid=" ZBX_FS_UI64
							" and h.key_=t.key_"
				" where", hostid);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "t.itemid",
				items.values, items.values_num);

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(itemid, row[0]);

			for (i = 0; i < httptests->values_num; i++)
			{
				httptest = (httptest_t *)httptests->values[i];

				for (j = 0; j < httptest->httptestitems.values_num; j++)
				{
					httptestitem = (httptestitem_t *)httptest->httptestitems.values[j];

					if (httptestitem->t_itemid == itemid)
						ZBX_STR2UINT64(httptestitem->h_itemid, row[1]);
				}

				for (j = 0; j < httptest->httpsteps.values_num; j++)
				{
					httpstep = (httpstep_t *)httptest->httpsteps.values[j];

					for (k = 0; k < httpstep->httpstepitems.values_num; k++)
					{
						httpstepitem = (httpstepitem_t *)httpstep->httpstepitems.values[k];

						if (httpstepitem->t_itemid == itemid)
							ZBX_STR2UINT64(httpstepitem->h_itemid, row[1]);
					}
				}
			}
		}
		DBfree_result(result);
	}

	zbx_free(sql);

	zbx_vector_uint64_destroy(&items);
	zbx_vector_uint64_destroy(&httptestids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBsave_httptests                                                 *
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
static void	DBsave_httptests(zbx_uint64_t hostid, zbx_vector_ptr_t *httptests)
{
	char		*sql = NULL;
	size_t		sql_alloc = 512, sql_offset = 0;
	httptest_t	*httptest;
	httpfield_t	*httpfield;
	httpstep_t	*httpstep;
	httptestitem_t	*httptestitem;
	httpstepitem_t	*httpstepitem;
	httptesttag_t	*httptesttag;
	zbx_uint64_t	httptestid = 0, httpstepid = 0, httptestitemid = 0, httpstepitemid = 0, httptestfieldid = 0,
			httpstepfieldid = 0, httptesttagid = 0;
	int		i, j, k, num_httptests = 0, num_httpsteps = 0, num_httptestitems = 0, num_httpstepitems = 0,
			num_httptestfields = 0, num_httpstepfields = 0, num_httptesttags = 0;
	zbx_db_insert_t	db_insert_htest, db_insert_hstep, db_insert_htitem, db_insert_hsitem, db_insert_tfield,
			db_insert_sfield, db_insert_httag;

	if (0 == httptests->values_num)
		return;

	for (i = 0; i < httptests->values_num; i++)
	{
		httptest = (httptest_t *)httptests->values[i];

		if (0 == httptest->httptestid)
		{
			num_httptests++;
			num_httpsteps += httptest->httpsteps.values_num;
			num_httptestitems += httptest->httptestitems.values_num;
			num_httptestfields += httptest->fields.values_num;
			num_httptesttags += httptest->httptesttags.values_num;

			for (j = 0; j < httptest->httpsteps.values_num; j++)
			{
				httpstep = (httpstep_t *)httptest->httpsteps.values[j];

				num_httpstepfields += httpstep->fields.values_num;
				num_httpstepitems += httpstep->httpstepitems.values_num;
			}
		}
	}

	if (0 != num_httptests)
	{
		httptestid = DBget_maxid_num("httptest", num_httptests);

		zbx_db_insert_prepare(&db_insert_htest, "httptest", "httptestid", "name", "delay", "status", "agent",
				"authentication", "http_user", "http_password", "http_proxy", "retries", "hostid",
				"templateid", NULL);
	}

	if (httptests->values_num != num_httptests)
		sql = (char *)zbx_malloc(sql, sql_alloc);

	if (0 != num_httptestfields)
	{
		httptestfieldid = DBget_maxid_num("httptest_field", num_httptestfields);

		zbx_db_insert_prepare(&db_insert_tfield, "httptest_field", "httptest_fieldid", "httptestid", "type",
				"name", "value", NULL);
	}

	if (0 != num_httpsteps)
	{
		httpstepid = DBget_maxid_num("httpstep", num_httpsteps);

		zbx_db_insert_prepare(&db_insert_hstep, "httpstep", "httpstepid", "httptestid", "name", "no", "url",
				"timeout", "posts", "required", "status_codes", "follow_redirects", "retrieve_mode",
				"post_type", NULL);
	}

	if (0 != num_httptestitems)
	{
		httptestitemid = DBget_maxid_num("httptestitem", num_httptestitems);

		zbx_db_insert_prepare(&db_insert_htitem, "httptestitem", "httptestitemid", "httptestid", "itemid",
				"type", NULL);
	}

	if (0 != num_httpstepitems)
	{
		httpstepitemid = DBget_maxid_num("httpstepitem", num_httpstepitems);

		zbx_db_insert_prepare(&db_insert_hsitem, "httpstepitem", "httpstepitemid", "httpstepid", "itemid",
				"type", NULL);
	}

	if (0 != num_httpstepfields)
	{
		httpstepfieldid = DBget_maxid_num("httpstep_field", num_httpstepfields);

		zbx_db_insert_prepare(&db_insert_sfield, "httpstep_field", "httpstep_fieldid", "httpstepid", "type",
				"name", "value", NULL);
	}

	if (0 != num_httptesttags)
	{
		httptesttagid = DBget_maxid_num("httptest_tag", num_httptesttags);

		zbx_db_insert_prepare(&db_insert_httag, "httptest_tag", "httptesttagid", "httptestid", "tag", "value",
				NULL);
	}

	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	for (i = 0; i < httptests->values_num; i++)
	{
		httptest = (httptest_t *)httptests->values[i];

		if (0 == httptest->httptestid)
		{
			httptest->httptestid = httptestid++;

			zbx_db_insert_add_values(&db_insert_htest, httptest->httptestid, httptest->name,
					httptest->delay, (int)httptest->status, httptest->agent,
					(int)httptest->authentication, httptest->http_user, httptest->http_password,
					httptest->http_proxy, httptest->retries, hostid, httptest->templateid);

			zbx_audit_httptests_create_entry_add(httptest->httptestid, httptest->name,
					httptest->delay, httptest->status, httptest->agent,
					httptest->authentication, httptest->http_user, httptest->http_password,
					httptest->http_proxy, httptest->retries, hostid, httptest->templateid);

			for (j = 0; j < httptest->fields.values_num; j++)
			{
				char audit_key_name[100];
				char audit_key_value[100];

				httpfield = (httpfield_t *)httptest->fields.values[j];

				zbx_db_insert_add_values(&db_insert_tfield, httptestfieldid, httptest->httptestid,
						httpfield->type, httpfield->name, httpfield->value);

				if (ZBX_HTTPFIELD_HEADER == httpfield->type)
				{
					zbx_snprintf(audit_key_name, 100, "httptest.headers[%lu].name",
							httpstepid);
					zbx_snprintf(audit_key_value, 100, "httptest.headers[%lu].value",
							httpstepid);
				}
				else if (ZBX_HTTPFIELD_VARIABLE == httpfield->type)
				{
					zbx_snprintf(audit_key_name, 100, "httptest.variables[%lu].name",
							httpstepid);
					zbx_snprintf(audit_key_value, 100, "httptest.variables[%lu].value",
							httpstepid);

				}
				zbx_audit_update_json_string(httptest->httptestid, audit_key_name,
						httpfield->name);
				zbx_audit_update_json_string(httptest->httptestid, audit_key_value,
						httpfield->value);

				httptestfieldid++;
			}

			for (j = 0; j < httptest->httpsteps.values_num; j++)
			{
				char audit_key_name[100];
				char audit_key_url[100];
				char audit_key_timeout[100];
				char audit_key_posts[100];
				char audit_key_required[100];
				char audit_key_status_codes[100];
				char audit_key_follow_redirects[100];
				char audit_key_retrieve_mode[100];

				httpstep = (httpstep_t *)httptest->httpsteps.values[j];

				zbx_db_insert_add_values(&db_insert_hstep, httpstepid, httptest->httptestid,
						httpstep->name, httpstep->no, httpstep->url, httpstep->timeout,
						httpstep->posts, httpstep->required, httpstep->status_codes,
						httpstep->follow_redirects, httpstep->retrieve_mode,
						httpstep->post_type);

				zbx_snprintf(audit_key_name, 100, "httptest.steps[%lu].no[%d].name",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_url, 100, "httptest.steps[%lu].no[%d].url",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_timeout, 100, "httptest.steps[%lu].no[%d].timeout",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_posts, 100, "httptest.steps[%lu].no[%d].posts",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_required, 100, "httptest.steps[%lu].no[%d].required",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_status_codes, 100, "httptest.steps[%lu].no[%d].status_codes",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_follow_redirects, 100,
						"httptest.steps[%lu].no[%d].follow_redirects",
						httpstepid, httpstep->no);
				zbx_snprintf(audit_key_retrieve_mode, 100, "httptest.steps[%lu].no[%d].retrieve_mode",
						httpstepid, httpstep->no);

				zbx_audit_update_json_string(httptest->httptestid, audit_key_name, httpstep->name);
				zbx_audit_update_json_string(httptest->httptestid, audit_key_url, httpstep->url);
				zbx_audit_update_json_string(httptest->httptestid, audit_key_timeout,
						httpstep->timeout);
				zbx_audit_update_json_string(httptest->httptestid, audit_key_posts, httpstep->posts);
				zbx_audit_update_json_string(httptest->httptestid, audit_key_required,
						httpstep->required);
				zbx_audit_update_json_string(httptest->httptestid, audit_key_status_codes,
						httpstep->status_codes);
				zbx_audit_update_json_uint64(httptest->httptestid, audit_key_follow_redirects,
						httpstep->follow_redirects);
				zbx_audit_update_json_uint64(httptest->httptestid, audit_key_retrieve_mode,
						httpstep->retrieve_mode);

				for (k = 0; k < httpstep->fields.values_num; k++)
				{
					char audit_step_key_name[100];
					char audit_step_key_value[100];

					httpfield = (httpfield_t *)httpstep->fields.values[k];

					zbx_db_insert_add_values(&db_insert_sfield, httpstepfieldid, httpstepid,
							httpfield->type, httpfield->name, httpfield->value);

					if (ZBX_HTTPFIELD_HEADER == httpfield->type)
					{
						zbx_snprintf(audit_step_key_name, 100,
								"httptest.steps[].headers[%lu].name", httpstepid);
						zbx_snprintf(audit_step_key_value, 100,
								"httptest.steps[].headers[%lu].value", httpstepid);
					}
					else if (ZBX_HTTPFIELD_VARIABLE == httpfield->type)
					{
						zbx_snprintf(audit_step_key_name, 100,
								"httptest.steps[].variables[%lu].name", httpstepid);
						zbx_snprintf(audit_step_key_value, 100,
								"httptest.steps[].variables[%lu].value", httpstepid);
					}
					else if (ZBX_HTTPFIELD_POST_FIELD == httpfield->type)
					{
						zbx_snprintf(audit_step_key_name, 100,
								"httptest.steps[].posts[%lu].name", httpstepid);
						zbx_snprintf(audit_step_key_value, 100,
								"httptest.steps[].posts[%lu].value", httpstepid);
					}
					else if (ZBX_HTTPFIELD_QUERY_FIELD == httpfield->type)
					{
						zbx_snprintf(audit_step_key_name, 100,
								"httptest.steps[].query_fields[%lu].name", httpstepid);
						zbx_snprintf(audit_step_key_value, 100,
								"httptest.steps[].query_fields[%lu].value", httpstepid);
					}
					else
					{
						THIS_SHOULD_NEVER_HAPPEN;
					}

					zbx_audit_update_json_string(httptest->httptestid, audit_step_key_name,
						httpfield->name);
					zbx_audit_update_json_string(httptest->httptestid, audit_step_key_value,
						httpfield->value);

					httpstepfieldid++;
				}

				for (k = 0; k < httpstep->httpstepitems.values_num; k++)
				{
					httpstepitem = (httpstepitem_t *)httpstep->httpstepitems.values[k];

					zbx_db_insert_add_values(&db_insert_hsitem,  httpstepitemid, httpstepid,
							httpstepitem->h_itemid, (int)httpstepitem->type);

					httpstepitemid++;
				}

				httpstepid++;
			}

			for (j = 0; j < httptest->httptestitems.values_num; j++)
			{
				httptestitem = (httptestitem_t *)httptest->httptestitems.values[j];

				zbx_db_insert_add_values(&db_insert_htitem, httptestitemid, httptest->httptestid,
						httptestitem->h_itemid, (int)httptestitem->type);

				httptestitemid++;
			}

			for (j = 0; j < httptest->httptesttags.values_num; j++)
			{
				httptesttag = (httptesttag_t *)httptest->httptesttags.values[j];

				zbx_db_insert_add_values(&db_insert_httag, httptesttagid, httptest->httptestid,
						httptesttag->tag, httptesttag->value);

				httptesttagid++;
			}
		}
		else
		{
			zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
					"update httptest"
					" set templateid=" ZBX_FS_UI64
					" where httptestid=" ZBX_FS_UI64 ";\n",
					httptest->templateid, httptest->httptestid);

			zbx_audit_httptests_create_entry_update(httptest->httptestid, httptest->name,
					httptest->templateid);

		}
	}

	if (0 != num_httptests)
	{
		zbx_db_insert_execute(&db_insert_htest);
		zbx_db_insert_clean(&db_insert_htest);
	}

	if (0 != num_httpsteps)
	{
		zbx_db_insert_execute(&db_insert_hstep);
		zbx_db_insert_clean(&db_insert_hstep);
	}

	if (0 != num_httptestitems)
	{
		zbx_db_insert_execute(&db_insert_htitem);
		zbx_db_insert_clean(&db_insert_htitem);
	}

	if (0 != num_httpstepitems)
	{
		zbx_db_insert_execute(&db_insert_hsitem);
		zbx_db_insert_clean(&db_insert_hsitem);
	}

	if (0 != num_httptestfields)
	{
		zbx_db_insert_execute(&db_insert_tfield);
		zbx_db_insert_clean(&db_insert_tfield);
	}

	if (0 != num_httpstepfields)
	{
		zbx_db_insert_execute(&db_insert_sfield);
		zbx_db_insert_clean(&db_insert_sfield);
	}

	if (0 != num_httptesttags)
	{
		zbx_db_insert_execute(&db_insert_httag);
		zbx_db_insert_clean(&db_insert_httag);
	}

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	if (16 < sql_offset)
		DBexecute("%s", sql);

	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: clean_httptests                                                  *
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
static void	clean_httptests(zbx_vector_ptr_t *httptests)
{
	httptest_t	*httptest;
	httpfield_t	*httpfield;
	httpstep_t	*httpstep;
	httptesttag_t	*httptesttag;
	int		i, j, k;

	for (i = 0; i < httptests->values_num; i++)
	{
		httptest = (httptest_t *)httptests->values[i];

		zbx_free(httptest->http_proxy);
		zbx_free(httptest->http_password);
		zbx_free(httptest->http_user);
		zbx_free(httptest->agent);
		zbx_free(httptest->delay);
		zbx_free(httptest->name);

		for (j = 0; j < httptest->fields.values_num; j++)
		{
			httpfield = (httpfield_t *)httptest->fields.values[j];

			zbx_free(httpfield->name);
			zbx_free(httpfield->value);

			zbx_free(httpfield);
		}

		zbx_vector_ptr_destroy(&httptest->fields);

		for (j = 0; j < httptest->httptesttags.values_num; j++)
		{
			httptesttag = (httptesttag_t *)httptest->httptesttags.values[j];

			zbx_free(httptesttag->tag);
			zbx_free(httptesttag->value);

			zbx_free(httptesttag);
		}

		zbx_vector_ptr_destroy(&httptest->httptesttags);

		for (j = 0; j < httptest->httpsteps.values_num; j++)
		{
			httpstep = (httpstep_t *)httptest->httpsteps.values[j];

			zbx_free(httpstep->status_codes);
			zbx_free(httpstep->required);
			zbx_free(httpstep->posts);
			zbx_free(httpstep->timeout);
			zbx_free(httpstep->url);
			zbx_free(httpstep->name);

			for (k = 0; k < httpstep->fields.values_num; k++)
			{
				httpfield = (httpfield_t *)httpstep->fields.values[k];

				zbx_free(httpfield->name);
				zbx_free(httpfield->value);

				zbx_free(httpfield);
			}

			zbx_vector_ptr_destroy(&httpstep->fields);

			for (k = 0; k < httpstep->httpstepitems.values_num; k++)
				zbx_free(httpstep->httpstepitems.values[k]);

			zbx_vector_ptr_destroy(&httpstep->httpstepitems);

			zbx_free(httpstep);
		}

		zbx_vector_ptr_destroy(&httptest->httpsteps);

		for (j = 0; j < httptest->httptestitems.values_num; j++)
			zbx_free(httptest->httptestitems.values[j]);

		zbx_vector_ptr_destroy(&httptest->httptestitems);

		zbx_free(httptest);
	}
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_template_httptests                                        *
 *                                                                            *
 * Purpose: copy web scenarios from template to host                          *
 *                                                                            *
 * Parameters: hostid      - [IN] host identificator from database            *
 *             templateids - [IN] array of template IDs                       *
 *                                                                            *
 ******************************************************************************/
static void	DBcopy_template_httptests(zbx_uint64_t hostid, const zbx_vector_uint64_t *templateids)
{
	zbx_vector_ptr_t	httptests;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_ptr_create(&httptests);

	DBget_httptests(hostid, templateids, &httptests);
	DBsave_httptests(hostid, &httptests);

	clean_httptests(&httptests);
	zbx_vector_ptr_destroy(&httptests);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBcopy_template_elements                                         *
 *                                                                            *
 * Purpose: copy elements from specified template                             *
 *                                                                            *
 * Parameters: hostid          - [IN] host identificator from database        *
 *             lnk_templateids - [IN] array of template IDs                   *
 *                                                                            *
 * Return value: upon successful completion return SUCCEED                    *
 *                                                                            *
 ******************************************************************************/
int	DBcopy_template_elements(zbx_uint64_t hostid, zbx_vector_uint64_t *lnk_templateids, char **error, char *recsetid_cuid)
{
	zbx_vector_uint64_t	templateids;
	zbx_uint64_t		hosttemplateid;
	int			i, res = SUCCEED;
	char			*template_names, err[MAX_STRING_LEN];

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_audit_init();

	zbx_vector_uint64_create(&templateids);

	get_templates_by_hostid(hostid, &templateids);

	for (i = 0; i < lnk_templateids->values_num; i++)
	{
		if (FAIL != zbx_vector_uint64_search(&templateids, lnk_templateids->values[i],
				ZBX_DEFAULT_UINT64_COMPARE_FUNC))
		{
			/* template already linked */
			zbx_vector_uint64_remove(lnk_templateids, i--);
		}
		else
			zbx_vector_uint64_append(&templateids, lnk_templateids->values[i]);
	}

	/* all templates already linked */
	if (0 == lnk_templateids->values_num)
		goto clean;

	zbx_vector_uint64_sort(&templateids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	if (SUCCEED != (res = validate_linked_templates(&templateids, err, sizeof(err))))
	{
		template_names = get_template_names(lnk_templateids);

		*error = zbx_dsprintf(NULL, "%s to host \"%s\": %s", template_names, zbx_host_string(hostid), err);

		zbx_free(template_names);
		goto clean;
	}

	if (SUCCEED != (res = validate_host(hostid, lnk_templateids, err, sizeof(err))))
	{
		template_names = get_template_names(lnk_templateids);

		*error = zbx_dsprintf(NULL, "%s to host \"%s\": %s", template_names, zbx_host_string(hostid), err);

		zbx_free(template_names);
		goto clean;
	}

	hosttemplateid = DBget_maxid_num("hosts_templates", lnk_templateids->values_num);

	for (i = 0; i < lnk_templateids->values_num; i++)
	{
		DBexecute("insert into hosts_templates (hosttemplateid,hostid,templateid)"
				" values (" ZBX_FS_UI64 "," ZBX_FS_UI64 "," ZBX_FS_UI64 ")",
				hosttemplateid++, hostid, lnk_templateids->values[i]);
	}

	DBcopy_template_items(hostid, lnk_templateids);
	DBcopy_template_host_prototypes(hostid, lnk_templateids);
	if (SUCCEED == (res = DBcopy_template_triggers(hostid, lnk_templateids)))
	{
		DBcopy_template_graphs(hostid, lnk_templateids);
		DBcopy_template_httptests(hostid, lnk_templateids);
	}

	zbx_audit_flush(recsetid_cuid);
clean:
	zbx_vector_uint64_destroy(&templateids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(res));

	return res;
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_hosts                                                   *
 *                                                                            *
 * Purpose: delete hosts from database with all elements                      *
 *                                                                            *
 * Parameters: hostids - [IN] host identificators from database               *
 *                                                                            *
 ******************************************************************************/
void	DBdelete_hosts(zbx_vector_uint64_t *hostids)
{
	zbx_vector_uint64_t	itemids, httptestids, selementids;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset;
	int			i;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	if (SUCCEED != DBlock_hostids(hostids))
		goto out;

	zbx_vector_uint64_create(&httptestids);
	zbx_vector_uint64_create(&selementids);

	/* delete web tests */

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select httptestid"
			" from httptest"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids->values, hostids->values_num);

	DBselect_uint64(sql, &httptestids);

	DBdelete_httptests(&httptestids);

	zbx_vector_uint64_destroy(&httptestids);

	/* delete items -> triggers -> graphs */

	zbx_vector_uint64_create(&itemids);

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select itemid"
			" from items"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids->values, hostids->values_num);

	DBselect_delete_for_item(sql, &itemids, AUDIT_RESOURCE_ITEM);
	DBdelete_items(&itemids, AUDIT_RESOURCE_ITEM);

	zbx_vector_uint64_destroy(&itemids);

	sql_offset = 0;
	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	/* delete host from maps */
	DBget_sysmapelements_by_element_type_ids(&selementids, SYSMAP_ELEMENT_TYPE_HOST, hostids);
	if (0 != selementids.values_num)
	{
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from sysmaps_elements where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "selementid", selementids.values,
				selementids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	/* delete action conditions */
	for (i = 0; i < hostids->values_num; i++)
		DBdelete_action_conditions(CONDITION_TYPE_HOST, hostids->values[i]);

	/* delete host */
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from hosts where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hostid", hostids->values, hostids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	DBexecute("%s", sql);

	zbx_free(sql);

	zbx_vector_uint64_destroy(&selementids);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_hosts_with_prototypes                                   *
 *                                                                            *
 * Purpose: delete hosts from database, check if there are any host           *
 *          prototypes and delete them first                                  *
 *                                                                            *
 * Parameters: hostids - [IN] host identificators from database               *
 *                                                                            *
 ******************************************************************************/
void	DBdelete_hosts_with_prototypes(zbx_vector_uint64_t *hostids, char *recsetid_cuid)
{
	zbx_vector_uint64_t	host_prototypeids;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_audit_init();

	zbx_vector_uint64_create(&host_prototypeids);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hd.hostid"
			" from items i,host_discovery hd"
			" where i.itemid=hd.parent_itemid"
				" and");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "i.hostid", hostids->values, hostids->values_num);

	DBselect_uint64(sql, &host_prototypeids);

	DBdelete_host_prototypes(&host_prototypeids);

	zbx_free(sql);
	zbx_vector_uint64_destroy(&host_prototypeids);

	DBdelete_hosts(hostids);

	zbx_audit_flush(recsetid_cuid);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBadd_interface                                                  *
 *                                                                            *
 * Purpose: add new interface to specified host                               *
 *                                                                            *
 * Parameters: hostid - [IN] host identificator from database                 *
 *             type   - [IN] new interface type                               *
 *             useip  - [IN] how to connect to the host 0/1 - DNS/IP          *
 *             ip     - [IN] IP address                                       *
 *             dns    - [IN] DNS address                                      *
 *             port   - [IN] port                                             *
 *             flags  - [IN] the used connection type                         *
 *                                                                            *
 * Return value: upon successful completion return interface identificator    *
 *                                                                            *
 * Author: Alexander Vladishev                                                *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
zbx_uint64_t	DBadd_interface(zbx_uint64_t hostid, unsigned char type, unsigned char useip, const char *ip,
		const char *dns, unsigned short port, zbx_conn_flags_t flags)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		*ip_esc, *dns_esc, *tmp = NULL;
	zbx_uint64_t	interfaceid = 0;
	unsigned char	main_ = 1, db_main, db_useip;
	unsigned short	db_port;
	const char	*db_ip, *db_dns;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	result = DBselect(
			"select interfaceid,useip,ip,dns,port,main"
			" from interface"
			" where hostid=" ZBX_FS_UI64
				" and type=%d",
			hostid, (int)type);

	while (NULL != (row = DBfetch(result)))
	{
		db_useip = (unsigned char)atoi(row[1]);
		db_ip = row[2];
		db_dns = row[3];
		db_main = (unsigned char)atoi(row[5]);
		if (1 == db_main)
			main_ = 0;

		if (ZBX_CONN_DEFAULT == flags)
		{
			if (db_useip != useip)
				continue;
			if (useip && 0 != strcmp(db_ip, ip))
				continue;

			if (!useip && 0 != strcmp(db_dns, dns))
				continue;

			zbx_free(tmp);
			tmp = strdup(row[4]);
			substitute_simple_macros(NULL, NULL, NULL, NULL, &hostid, NULL, NULL, NULL, NULL, NULL,
					&tmp, MACRO_TYPE_COMMON, NULL, 0);
			if (FAIL == is_ushort(tmp, &db_port) || db_port != port)
				continue;

			ZBX_STR2UINT64(interfaceid, row[0]);
			break;
		}

		/* update main interface if explicit connection flags were passed (flags != ZBX_CONN_DEFAULT) */
		if (1 == db_main)
		{
			char	*update = NULL, delim = ' ';
			size_t	update_alloc = 0, update_offset = 0;

			ZBX_STR2UINT64(interfaceid, row[0]);

			if (db_useip != useip)
			{
				zbx_snprintf_alloc(&update, &update_alloc, &update_offset, "%cuseip=%d", delim, useip);
				delim = ',';
			}

			if (ZBX_CONN_IP == flags && 0 != strcmp(db_ip, ip))
			{
				ip_esc = DBdyn_escape_field("interface", "ip", ip);
				zbx_snprintf_alloc(&update, &update_alloc, &update_offset, "%cip='%s'", delim, ip_esc);
				zbx_free(ip_esc);
				delim = ',';
			}

			if (ZBX_CONN_DNS == flags && 0 != strcmp(db_dns, dns))
			{
				dns_esc = DBdyn_escape_field("interface", "dns", dns);
				zbx_snprintf_alloc(&update, &update_alloc, &update_offset, "%cdns='%s'", delim,
						dns_esc);
				zbx_free(dns_esc);
				delim = ',';
			}

			if (FAIL == is_ushort(row[4], &db_port) || db_port != port)
				zbx_snprintf_alloc(&update, &update_alloc, &update_offset, "%cport=%u", delim, port);

			if (0 != update_alloc)
			{
				DBexecute("update interface set%s where interfaceid=" ZBX_FS_UI64, update,
						interfaceid);
				zbx_free(update);
			}
			break;
		}
	}
	DBfree_result(result);

	zbx_free(tmp);

	if (0 != interfaceid)
		goto out;

	ip_esc = DBdyn_escape_field("interface", "ip", ip);
	dns_esc = DBdyn_escape_field("interface", "dns", dns);

	interfaceid = DBget_maxid("interface");

	DBexecute("insert into interface"
			" (interfaceid,hostid,main,type,useip,ip,dns,port)"
		" values"
			" (" ZBX_FS_UI64 "," ZBX_FS_UI64 ",%d,%d,%d,'%s','%s',%d)",
		interfaceid, hostid, (int)main_, (int)type, (int)useip, ip_esc, dns_esc, (int)port);

	zbx_free(dns_esc);
	zbx_free(ip_esc);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():" ZBX_FS_UI64, __func__, interfaceid);

	return interfaceid;
}

/******************************************************************************
 *                                                                            *
 * Function: DBadd_interface_snmp                                             *
 *                                                                            *
 * Purpose: add new or update interface options to specified interface        *
 *                                                                            *
 * Parameters: interfaceid    - [IN] interface id from database               *
 *             version        - [IN] snmp version                             *
 *             bulk           - [IN] snmp bulk options                        *
 *             community      - [IN] snmp community name                      *
 *             securityname   - [IN] snmp v3 security name                    *
 *             securitylevel  - [IN] snmp v3 security level                   *
 *             authpassphrase - [IN] snmp v3 authentication passphrase        *
 *             privpassphrase - [IN] snmp v3 private passphrase               *
 *             authprotocol   - [IN] snmp v3 authentication protocol          *
 *             privprotocol   - [IN] snmp v3 private protocol                 *
 *             contextname    - [IN] snmp v3 context name                     *
 *                                                                            *
 ******************************************************************************/
void	DBadd_interface_snmp(const zbx_uint64_t interfaceid, const unsigned char version, const unsigned char bulk,
		const char *community, const char *securityname, const unsigned char securitylevel,
		const char *authpassphrase, const char *privpassphrase, const unsigned char authprotocol,
		const unsigned char privprotocol, const char *contextname)
{
	char		*community_esc, *securityname_esc, *authpassphrase_esc, *privpassphrase_esc, *contextname_esc;
	DB_RESULT	result;
	DB_ROW		row;

	result = DBselect(
			"select version,bulk,community,securityname,securitylevel,authpassphrase,privpassphrase,"
			"authprotocol,privprotocol,contextname"
			" from interface_snmp"
			" where interfaceid=" ZBX_FS_UI64,
			interfaceid);

	while (NULL != (row = DBfetch(result)))
	{
		unsigned char	db_version, db_bulk, db_securitylevel, db_authprotocol, db_privprotocol;

		ZBX_STR2UCHAR(db_version, row[0]);

		if (version != db_version)
			break;

		ZBX_STR2UCHAR(db_bulk, row[1]);

		if (bulk != db_bulk)
			break;

		if (0 != strcmp(community, row[2]))
			break;

		if (0 != strcmp(securityname, row[3]))
			break;

		ZBX_STR2UCHAR(db_securitylevel, row[4]);

		if (securitylevel != db_securitylevel)
			break;

		if (0 != strcmp(authpassphrase, row[5]))
			break;

		if (0 != strcmp(privpassphrase, row[6]))
			break;

		ZBX_STR2UCHAR(db_authprotocol, row[7]);

		if (authprotocol != db_authprotocol)
			break;

		ZBX_STR2UCHAR(db_privprotocol, row[8]);

		if (privprotocol != db_privprotocol)
			break;

		if (0 != strcmp(contextname, row[9]))
			break;

		goto out;
	}

	community_esc = DBdyn_escape_field("interface_snmp", "community", community);
	securityname_esc = DBdyn_escape_field("interface_snmp", "securityname", securityname);
	authpassphrase_esc = DBdyn_escape_field("interface_snmp", "authpassphrase", authpassphrase);
	privpassphrase_esc = DBdyn_escape_field("interface_snmp", "privpassphrase", privpassphrase);
	contextname_esc = DBdyn_escape_field("interface_snmp", "contextname", contextname);

	if (NULL == row)
	{
		DBexecute("insert into interface_snmp"
				" (interfaceid,version,bulk,community,securityname,securitylevel,authpassphrase,"
				" privpassphrase,authprotocol,privprotocol,contextname)"
			" values"
				" (" ZBX_FS_UI64 ",%d,%d,'%s','%s',%d,'%s','%s',%d,%d,'%s')",
			interfaceid, (int)version, (int)bulk, community_esc, securityname_esc, (int)securitylevel,
			authpassphrase_esc, privpassphrase_esc, (int)authprotocol, (int)privprotocol, contextname_esc);
	}
	else
	{
		DBexecute(
			"update interface_snmp"
			" set version=%d"
				",bulk=%d"
				",community='%s'"
				",securityname='%s'"
				",securitylevel=%d"
				",authpassphrase='%s'"
				",privpassphrase='%s'"
				",authprotocol=%d"
				",privprotocol=%d"
				",contextname='%s'"
			" where interfaceid=" ZBX_FS_UI64,
			(int)version, (int)bulk, community_esc, securityname_esc, (int)securitylevel,
			authpassphrase_esc, privpassphrase_esc, (int)authprotocol, (int)privprotocol, contextname_esc,
			interfaceid);
	}

	zbx_free(community_esc);
	zbx_free(securityname_esc);
	zbx_free(authpassphrase_esc);
	zbx_free(privpassphrase_esc);
	zbx_free(contextname_esc);
out:
	DBfree_result(result);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_groups_validate                                         *
 *                                                                            *
 * Purpose: removes the groupids from the list which cannot be deleted        *
 *          (host or template can remain without groups or it's an internal   *
 *          group or it's used by a host prototype)                           *
 *                                                                            *
 ******************************************************************************/
static void	DBdelete_groups_validate(zbx_vector_uint64_t *groupids)
{
	DB_RESULT		result;
	DB_ROW			row;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_vector_uint64_t	hostids;
	zbx_uint64_t		groupid;
	int			index, internal;

	if (0 == groupids->values_num)
		return;

	zbx_vector_uint64_create(&hostids);

	/* select of the list of hosts which remain without groups */

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select hg.hostid"
			" from hosts_groups hg"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hg.groupid", groupids->values, groupids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			" and not exists ("
				"select null"
				" from hosts_groups hg2"
				" where hg.hostid=hg2.hostid"
					" and not");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hg2.groupid", groupids->values, groupids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			")");

	DBselect_uint64(sql, &hostids);

	/* select of the list of groups which cannot be deleted */

	sql_offset = 0;
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select g.groupid,g.internal,g.name"
			" from hstgrp g"
			" where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "g.groupid", groupids->values, groupids->values_num);
	if (0 < hostids.values_num)
	{
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
				" and (g.internal=%d"
					" or exists ("
						"select null"
						" from hosts_groups hg"
						" where g.groupid=hg.groupid"
							" and",
				ZBX_INTERNAL_GROUP);
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "hg.hostid", hostids.values, hostids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "))");
	}
	else
		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, " and g.internal=%d", ZBX_INTERNAL_GROUP);

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(groupid, row[0]);
		internal = atoi(row[1]);

		if (FAIL != (index = zbx_vector_uint64_bsearch(groupids, groupid, ZBX_DEFAULT_UINT64_COMPARE_FUNC)))
			zbx_vector_uint64_remove(groupids, index);

		if (ZBX_INTERNAL_GROUP == internal)
		{
			zabbix_log(LOG_LEVEL_WARNING, "host group \"%s\" is internal and cannot be deleted", row[2]);
		}
		else
		{
			zabbix_log(LOG_LEVEL_WARNING, "host group \"%s\" cannot be deleted,"
					" because some hosts or templates depend on it", row[2]);
		}
	}
	DBfree_result(result);

	/* check if groups is used in the groups prototypes */

	if (0 != groupids->values_num)
	{
		sql_offset = 0;
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
				"select g.groupid,g.name"
				" from hstgrp g"
				" where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "g.groupid",
				groupids->values, groupids->values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
					" and exists ("
						"select null"
						" from group_prototype gp"
						" where g.groupid=gp.groupid"
					")");

		result = DBselect("%s", sql);

		while (NULL != (row = DBfetch(result)))
		{
			ZBX_STR2UINT64(groupid, row[0]);

			if (FAIL != (index = zbx_vector_uint64_bsearch(groupids, groupid,
					ZBX_DEFAULT_UINT64_COMPARE_FUNC)))
			{
				zbx_vector_uint64_remove(groupids, index);
			}

			zabbix_log(LOG_LEVEL_WARNING, "host group \"%s\" cannot be deleted,"
					" because it is used by a host prototype", row[1]);
		}
		DBfree_result(result);
	}

	zbx_vector_uint64_destroy(&hostids);
	zbx_free(sql);
}

/******************************************************************************
 *                                                                            *
 * Function: DBdelete_groups                                                  *
 *                                                                            *
 * Purpose: delete host groups from database                                  *
 *                                                                            *
 * Parameters: groupids - [IN] array of group identificators from database    *
 *                                                                            *
 ******************************************************************************/
void	DBdelete_groups(zbx_vector_uint64_t *groupids)
{
	char			*sql = NULL;
	size_t			sql_alloc = 256, sql_offset = 0;
	int			i;
	zbx_vector_uint64_t	selementids;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() values_num:%d", __func__, groupids->values_num);

	DBdelete_groups_validate(groupids);

	if (0 == groupids->values_num)
		goto out;

	for (i = 0; i < groupids->values_num; i++)
		DBdelete_action_conditions(CONDITION_TYPE_HOST_GROUP, groupids->values[i]);

	sql = (char *)zbx_malloc(sql, sql_alloc);

	zbx_vector_uint64_create(&selementids);

	DBbegin_multiple_update(&sql, &sql_alloc, &sql_offset);

	/* delete sysmaps_elements */
	DBget_sysmapelements_by_element_type_ids(&selementids, SYSMAP_ELEMENT_TYPE_HOST_GROUP, groupids);
	if (0 != selementids.values_num)
	{
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from sysmaps_elements where");
		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "selementid", selementids.values,
				selementids.values_num);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");
	}

	/* groups */
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "delete from hstgrp where");
	DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "groupid", groupids->values, groupids->values_num);
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ";\n");

	DBend_multiple_update(&sql, &sql_alloc, &sql_offset);

	DBexecute("%s", sql);

	zbx_vector_uint64_destroy(&selementids);

	zbx_free(sql);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Function: DBadd_host_inventory                                             *
 *                                                                            *
 * Purpose: adds host inventory to the host                                   *
 *                                                                            *
 * Parameters: hostid         - [IN] host identifier                          *
 *             inventory_mode - [IN] the host inventory mode                  *
 *                                                                            *
 ******************************************************************************/
void	DBadd_host_inventory(zbx_uint64_t hostid, int inventory_mode)
{
	zbx_db_insert_t	db_insert;

	zbx_db_insert_prepare(&db_insert, "host_inventory", "hostid", "inventory_mode", NULL);
	zbx_db_insert_add_values(&db_insert, hostid, inventory_mode);
	zbx_db_insert_execute(&db_insert);
	zbx_db_insert_clean(&db_insert);
}

/******************************************************************************
 *                                                                            *
 * Function: DBset_host_inventory                                             *
 *                                                                            *
 * Purpose: sets host inventory mode for the specified host                   *
 *                                                                            *
 * Parameters: hostid         - [IN] host identifier                          *
 *             inventory_mode - [IN] the host inventory mode                  *
 *                                                                            *
 * Comments: The host_inventory table record is created if absent.            *
 *                                                                            *
 *           This function does not allow disabling host inventory - only     *
 *           setting manual or automatic host inventory mode is supported.    *
 *                                                                            *
 ******************************************************************************/
void	DBset_host_inventory(zbx_uint64_t hostid, int inventory_mode)
{
	DB_RESULT	result;
	DB_ROW		row;

	result = DBselect("select inventory_mode from host_inventory where hostid=" ZBX_FS_UI64, hostid);

	if (NULL == (row = DBfetch(result)))
	{
		DBadd_host_inventory(hostid, inventory_mode);
	}
	else if (inventory_mode != atoi(row[0]))
	{
		DBexecute("update host_inventory set inventory_mode=%d where hostid=" ZBX_FS_UI64, inventory_mode,
				hostid);
	}

	DBfree_result(result);
}
