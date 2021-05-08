#include "audit.h"
#include "zbxjson.h"
#include "db.h"
#include "log.h"

#define	AUDIT_DETAILS_KEY_LEN	100

zbx_hashset_t zbx_audit;

typedef struct zbx_audit_entry
{
	zbx_uint64_t	id;
	char		*name;
	struct zbx_json	details_json;
	int		audit_action;
	int		resource_type;
} zbx_audit_entry_t;

static unsigned	zbx_audit_hash_func(const void *data)
{
	const zbx_audit_entry_t	* const *audit_entry = (const zbx_audit_entry_t * const *)data;

	return ZBX_DEFAULT_UINT64_HASH_ALGO(&((*audit_entry)->id), sizeof((*audit_entry)->id),
			ZBX_DEFAULT_HASH_SEED);
}

static int	zbx_audit_compare_func(const void *d1, const void *d2)
{
	const zbx_audit_entry_t	* const *audit_entry_1 = (const zbx_audit_entry_t * const *)d1;
	const zbx_audit_entry_t	* const *audit_entry_2 = (const zbx_audit_entry_t * const *)d2;

	ZBX_RETURN_IF_NOT_EQUAL((*audit_entry_1)->id, (*audit_entry_2)->id);

	return 0;
}

static void	zbx_audit_clean(void)
{
	zbx_hashset_iter_t	iter;
	zbx_audit_entry_t	**audit_entry;

	zbx_hashset_iter_reset(&zbx_audit, &iter);

	while (NULL != (audit_entry = (zbx_audit_entry_t **)zbx_hashset_iter_next(&iter)))
	{
		zbx_free((*audit_entry)->name);
		zbx_free(*audit_entry);
	}

	zbx_hashset_destroy(&zbx_audit);
}

void	zbx_audit_init(void)
{
#define	AUDIT_HASHSET_DEF_SIZE  100
	zbx_hashset_create(&zbx_audit, AUDIT_HASHSET_DEF_SIZE, zbx_audit_hash_func, zbx_audit_compare_func);
}

void	zbx_audit_flush(void)
{
	char			audit_cuid[CUID_LEN], recsetid_cuid[CUID_LEN];
	zbx_hashset_iter_t	iter;
	zbx_audit_entry_t	**audit_entry;
	zbx_db_insert_t		db_insert_audit;

	zbx_new_cuid(recsetid_cuid);
	zbx_hashset_iter_reset(&zbx_audit, &iter);

	zbx_db_insert_prepare(&db_insert_audit, "auditlog2", "auditid", "userid", "clock", "action", "ip",
			"resourceid","resourcename","resourcetype","recsetid","details", NULL);

	while (NULL != (audit_entry = (zbx_audit_entry_t **)zbx_hashset_iter_next(&iter)))
	{
		zbx_new_cuid(audit_cuid);

		if (AUDIT_ACTION_UPDATE != (*audit_entry)->audit_action ||
				0 != strcmp((*audit_entry)->details_json.buffer, "{}"))
		{
			zbx_db_insert_add_values(&db_insert_audit, audit_cuid, USER_TYPE_SUPER_ADMIN,
					(int)time(NULL), (*audit_entry)->audit_action, "", (*audit_entry)->id,
					(*audit_entry)->name, (*audit_entry)->resource_type,
					recsetid_cuid, (*audit_entry)->details_json.buffer);
		}
	}

	zbx_db_insert_execute(&db_insert_audit);
	zbx_db_insert_clean(&db_insert_audit);

	zbx_audit_clean();
}

static int	item_flag_to_resource_type(int flag)
{
	if (ZBX_FLAG_DISCOVERY_NORMAL == flag || ZBX_FLAG_DISCOVERY_CREATED == flag)
	{
		return AUDIT_RESOURCE_ITEM;
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flag)
	{
		return AUDIT_RESOURCE_ITEM_PROTOTYPE;
	}
	else if (ZBX_FLAG_DISCOVERY_RULE == flag)
	{
		return AUDIT_RESOURCE_DISCOVERY_RULE;
	}
	else
	{
		zabbix_log(LOG_LEVEL_DEBUG, "unexpected audit detected: ->%d<-", flag);
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}
}

static void	zbx_audit_create_entry_for_delete(zbx_uint64_t id, char *name, int resource_type)
{
	zbx_audit_entry_t	*local_audit_entry = (zbx_audit_entry_t*)zbx_malloc(NULL,
			sizeof(zbx_audit_entry_t));

	local_audit_entry->id = id;
	local_audit_entry->name = zbx_strdup(NULL, name);
	local_audit_entry->audit_action = AUDIT_ACTION_DELETE;
	local_audit_entry->resource_type = resource_type;

	zbx_json_init(&(local_audit_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	/* trigger prototypes may overlap in 2 selects - regular and templates, code there handles this by filtering */
	/* unique entries in place, but here we can simply check if an entry is already present in the hashset */
	if (NULL == zbx_hashset_search(&zbx_audit, &local_audit_entry))
	{
		zbx_hashset_insert(&zbx_audit, &local_audit_entry, sizeof(local_audit_entry));
	}
	else
	{
		zbx_free(local_audit_entry->name);
		zbx_free(local_audit_entry);
	}
}

/******************************************************************************
 *                                                                            *
 * Function: DBselect_for_item                                                *
 *                                                                            *
 * Parameters: sql - [IN] sql statement                                       *
 *             ids - [OUT] sorted list of selected uint64 values              *
 *                                                                            *
 ******************************************************************************/
void	DBselect_delete_for_item(const char *sql, zbx_vector_uint64_t *ids)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	id, flags;

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(id, row[0]);
		zbx_vector_uint64_append(ids, id);
		ZBX_STR2UINT64(flags, row[2]);
		zbx_audit_create_entry_for_delete(id, row[1], item_flag_to_resource_type(flags));
	}
	DBfree_result(result);

	zbx_vector_uint64_sort(ids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

void	DBselect_delete_for_http_test(const char *sql, zbx_vector_uint64_t *ids)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	id;

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(id, row[0]);
		zbx_vector_uint64_append(ids, id);
		zbx_audit_create_entry_for_delete(id, row[1], AUDIT_RESOURCE_SCENARIO);
	}
	DBfree_result(result);

	zbx_vector_uint64_sort(ids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

void	DBselect_delete_for_trigger(const char *sql, zbx_vector_uint64_t *ids)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	id, flags;

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(id, row[0]);
		zbx_vector_uint64_append(ids, id);
		ZBX_STR2UINT64(flags, row[2]);

		if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
			zbx_audit_create_entry_for_delete(id, row[1], AUDIT_RESOURCE_TRIGGER);
		else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			zbx_audit_create_entry_for_delete(id, row[1], AUDIT_RESOURCE_TRIGGER_PROTOTYPE);
	}
	DBfree_result(result);

	zbx_vector_uint64_sort(ids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

void	DBselect_delete_for_graph(const char *sql, zbx_vector_uint64_t *ids)
{
	DB_RESULT	result;
	DB_ROW		row;
	zbx_uint64_t	id, flags;

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		ZBX_STR2UINT64(id, row[0]);
		zbx_vector_uint64_append(ids, id);
		ZBX_STR2UINT64(flags, row[2]);

		if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
			zbx_audit_create_entry_for_delete(id, row[1], AUDIT_RESOURCE_GRAPH);
		else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
			zbx_audit_create_entry_for_delete(id, row[1], AUDIT_RESOURCE_GRAPH_PROTOTYPE);
	}
	DBfree_result(result);

	zbx_vector_uint64_sort(ids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
}

const char	*zbx_audit_items_get_type_json_identifier(int flag)
{
	if (ZBX_FLAG_DISCOVERY_NORMAL == flag || ZBX_FLAG_DISCOVERY_CREATED == flag)
	{
		return "item";
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flag)
	{
		return "itemprototype";
	}
	else if (ZBX_FLAG_DISCOVERY_RULE == flag)
	{
		return "discoveryrule";
	}
	else
	{
		zabbix_log(LOG_LEVEL_DEBUG, "unexpected audit flag detected: ->%d<-", flag);
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}
}

void	zbx_audit_items_create_entry(const zbx_template_item_t *item, const zbx_uint64_t hostid, const int audit_action)
{
	zbx_audit_entry_t	*local_audit_items_entry;

	local_audit_items_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_items_entry->id = item->itemid;
	local_audit_items_entry->name = zbx_strdup(NULL, item->name);
	local_audit_items_entry->audit_action = audit_action;
	local_audit_items_entry->resource_type = item_flag_to_resource_type(item->flags);

#define ONLY_ITEM (AUDIT_RESOURCE_ITEM == local_audit_items_entry->resource_type)
#define ONLY_ITEM_PROTOTYPE (AUDIT_RESOURCE_ITEM_PROTOTYPE == local_audit_items_entry->resource_type)
#define ONLY_LLD_RULE (AUDIT_RESOURCE_DISCOVERY_RULE == local_audit_items_entry->resource_type)
#define ONLY_ITEM_AND_ITEM_PROTOTYPE (AUDIT_RESOURCE_ITEM == local_audit_items_entry->resource_type || \
		AUDIT_RESOURCE_ITEM_PROTOTYPE == local_audit_items_entry->resource_type)

#define IT_OR_ITP(s) ONLY_ITEM ? "item."#s :				\
	(ONLY_ITEM_PROTOTYPE ? "itemprototype."#s : "discoveryrule."#s)
#define ADD_JSON_S(x)	zbx_json_addstring(&local_audit_items_entry->details_json, IT_OR_ITP(x), item->x, \
	ZBX_JSON_TYPE_STRING)
#define ADD_JSON_UI(x)	zbx_json_adduint64(&local_audit_items_entry->details_json, IT_OR_ITP(x), item->x)

	zbx_json_init(&(local_audit_items_entry->details_json), ZBX_JSON_STAT_BUF_LEN);
	ADD_JSON_UI(itemid);
	ADD_JSON_S(delay);
	zbx_json_adduint64(&local_audit_items_entry->details_json, IT_OR_ITP(hostid), hostid);
	/* ruleid is REQUIRED for item prototype */
	ADD_JSON_UI(interfaceid);
	ADD_JSON_S(key); // API HAS 'key_' , but SQL 'key'
	ADD_JSON_S(name);
	ADD_JSON_UI(type);
	ADD_JSON_S(url);
	if ONLY_ITEM_AND_ITEM_PROTOTYPE ADD_JSON_UI(value_type);
	ADD_JSON_UI(allow_traps);
	ADD_JSON_UI(authtype);
	ADD_JSON_S(description);
	/* error - only for item and LLD RULE */
	if ONLY_ITEM ADD_JSON_UI(flags);
	ADD_JSON_UI(follow_redirects);
	ADD_JSON_S(headers);
	if ONLY_ITEM_AND_ITEM_PROTOTYPE ADD_JSON_S(history);
	ADD_JSON_S(http_proxy);
	if ONLY_ITEM ADD_JSON_UI(inventory_link);
	ADD_JSON_S(ipmi_sensor);
	ADD_JSON_S(jmx_endpoint);
	if ONLY_LLD_RULE ADD_JSON_S(lifetime);
	/* lastclock - only for item */
	/* last ns - only for item */
	/* lastvalue - only for item */
	if ONLY_ITEM_AND_ITEM_PROTOTYPE ADD_JSON_S(logtimefmt);
	ADD_JSON_UI(master_itemid);
	ADD_JSON_UI(output_format);
	ADD_JSON_S(params);
	/* parameters , handled later - for both item and item prototype and LLD RULE */
	ADD_JSON_S(password);
	ADD_JSON_UI(post_type);
	ADD_JSON_S(posts);
	/* prevvalue - only for item */
	ADD_JSON_S(privatekey);
	ADD_JSON_S(publickey);
	ADD_JSON_S(query_fields);
	ADD_JSON_UI(request_method);
	ADD_JSON_UI(retrieve_mode);
	ADD_JSON_S(snmp_oid);
	ADD_JSON_S(ssl_cert_file);
	ADD_JSON_S(ssl_key_file);
	ADD_JSON_S(ssl_key_password);
	/* state - only for item and LLD RULE */
	ADD_JSON_UI(status);
	ADD_JSON_S(status_codes);
	ADD_JSON_UI(templateid);
	ADD_JSON_S(timeout);
	ADD_JSON_S(trapper_hosts);
	if ONLY_ITEM_AND_ITEM_PROTOTYPE ADD_JSON_S(trends);
	if ONLY_ITEM_AND_ITEM_PROTOTYPE ADD_JSON_S(units);
	ADD_JSON_S(username);
	if ONLY_ITEM_AND_ITEM_PROTOTYPE ADD_JSON_UI(valuemapid);
	ADD_JSON_UI(verify_host);
	ADD_JSON_UI(verify_peer);
	/* discover - only for item */
	/* ITEM API FINISHED */

	/* application - handled later
	preprocessing - handled later */

	if ONLY_LLD_RULE
	{
		ADD_JSON_S(formula);
		ADD_JSON_UI(evaltype);
		ADD_JSON_UI(discover);
	}

	zbx_hashset_insert(&zbx_audit, &local_audit_items_entry, sizeof(local_audit_items_entry));
}

void	zbx_audit_host_update_parent_template(const char *audit_details_action, zbx_uint64_t hostid,
		zbx_uint64_t templateid)
{
	char	audit_key_parent_templates[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_parent_templates, AUDIT_DETAILS_KEY_LEN, "host.parentTemplates[%lu]", templateid);
	zbx_audit_update_json_string(hostid, audit_key_parent_templates, audit_details_action);
}

void	zbx_audit_host_delete_parent_templates(zbx_uint64_t hostid, const char *hostname,
		zbx_vector_uint64_t *del_templateids)
{
	int	i;

	zbx_audit_host_create_entry(AUDIT_ACTION_UPDATE, hostid, hostname);

	for (i = 0; i < del_templateids->values_num; i++)
		zbx_audit_host_update_parent_template(AUDIT_DETAILS_ACTION_DELETE, hostid, del_templateids->values[i]);
}

void	zbx_audit_host_prototypes_create_entry(const int audit_action, zbx_uint64_t hostid, char *name,
		unsigned char status, zbx_uint64_t templateid, unsigned char discover, unsigned char custom_interfaces)
{
	zbx_audit_entry_t	*local_audit_host_prototype_entry;

	local_audit_host_prototype_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));

	local_audit_host_prototype_entry->id = hostid;
	local_audit_host_prototype_entry->name = zbx_strdup(NULL, name);
	local_audit_host_prototype_entry->audit_action = audit_action;
	local_audit_host_prototype_entry->resource_type = AUDIT_RESOURCE_HOST_PROTOTYPE;

	zbx_json_init(&(local_audit_host_prototype_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	zbx_json_adduint64(&local_audit_host_prototype_entry->details_json, "hostprototype.status", status);
	zbx_json_adduint64(&local_audit_host_prototype_entry->details_json, "hostprototype.templateid", templateid);
	zbx_json_adduint64(&local_audit_host_prototype_entry->details_json, "hostprototype.discover", discover);
	zbx_json_adduint64(&local_audit_host_prototype_entry->details_json, "hostprototype.custom_interfaces",
			custom_interfaces);

	zbx_hashset_insert(&zbx_audit, &local_audit_host_prototype_entry, sizeof(local_audit_host_prototype_entry));
}

void	zbx_audit_host_prototypes_update_details(zbx_uint64_t hostid, const char *name, zbx_uint64_t groupid,
		zbx_uint64_t templateid)
{
	char	audit_key_operator[AUDIT_DETAILS_KEY_LEN];

	if (0 != strlen(name))
	{
		zbx_snprintf(audit_key_operator, AUDIT_DETAILS_KEY_LEN, "hostprototype.groupPrototypes[%s]", name);
		zbx_audit_update_json_uint64(hostid, audit_key_operator, templateid);
	}
	else if (0 != groupid)
	{
		zbx_snprintf(audit_key_operator, AUDIT_DETAILS_KEY_LEN, "hostprototype.groupLinks[%lu]", groupid);
		zbx_audit_update_json_uint64(hostid, audit_key_operator, templateid);
	}
}

void	zbx_audit_graphs_create_entry(const int audit_action, zbx_uint64_t hst_graphid, const char *name, int width,
		int height, double yaxismin, double yaxismax, zbx_uint64_t graphid, unsigned char show_work_period,
		unsigned char show_triggers, unsigned char graphtype, unsigned char show_legend, unsigned char show_3d,
		double percent_left, double percent_right, unsigned char ymin_type, unsigned char ymax_type,
		zbx_uint64_t ymin_itemid, zbx_uint64_t ymax_itemid, unsigned char flags, unsigned char discover)
{
	char	audit_key_width[AUDIT_DETAILS_KEY_LEN], audit_key_height[AUDIT_DETAILS_KEY_LEN],
		audit_key_yaxismin[AUDIT_DETAILS_KEY_LEN], audit_key_yaxismax[AUDIT_DETAILS_KEY_LEN],
		audit_key_show_work_period[AUDIT_DETAILS_KEY_LEN], audit_key_show_triggers[AUDIT_DETAILS_KEY_LEN],
		audit_key_templateid[AUDIT_DETAILS_KEY_LEN], audit_key_graphtype[AUDIT_DETAILS_KEY_LEN],
		audit_key_show_legend[AUDIT_DETAILS_KEY_LEN], audit_key_show_3d[AUDIT_DETAILS_KEY_LEN],
		audit_key_percent_left[AUDIT_DETAILS_KEY_LEN], audit_key_percent_right[AUDIT_DETAILS_KEY_LEN],
		audit_key_ymin_type[AUDIT_DETAILS_KEY_LEN], audit_key_ymax_type[AUDIT_DETAILS_KEY_LEN],
		audit_key_ymin_itemid[AUDIT_DETAILS_KEY_LEN], audit_key_ymax_itemid[AUDIT_DETAILS_KEY_LEN],
		audit_key_flags[AUDIT_DETAILS_KEY_LEN], audit_key_discover[AUDIT_DETAILS_KEY_LEN];

	zbx_audit_entry_t	*local_audit_graph_entry;

	local_audit_graph_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_graph_entry->id = hst_graphid;
	local_audit_graph_entry->name = zbx_strdup(NULL, name);
	local_audit_graph_entry->audit_action = audit_action;

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
	{
		local_audit_graph_entry->resource_type = AUDIT_RESOURCE_GRAPH;

		zbx_snprintf(audit_key_width,            AUDIT_DETAILS_KEY_LEN, "graph.width");
		zbx_snprintf(audit_key_height,           AUDIT_DETAILS_KEY_LEN, "graph.height");
		zbx_snprintf(audit_key_yaxismin,         AUDIT_DETAILS_KEY_LEN, "graph.yaxismin");
		zbx_snprintf(audit_key_yaxismax,         AUDIT_DETAILS_KEY_LEN, "graph.yaxismax");
		zbx_snprintf(audit_key_show_work_period, AUDIT_DETAILS_KEY_LEN, "graph.show_work_period");
		zbx_snprintf(audit_key_show_triggers,    AUDIT_DETAILS_KEY_LEN, "graph.show_triggers");
		zbx_snprintf(audit_key_templateid,       AUDIT_DETAILS_KEY_LEN, "graph.templateid");
		zbx_snprintf(audit_key_graphtype,        AUDIT_DETAILS_KEY_LEN, "graph.graphtype");
		zbx_snprintf(audit_key_show_legend,      AUDIT_DETAILS_KEY_LEN, "graph.show_legend");
		zbx_snprintf(audit_key_show_3d,          AUDIT_DETAILS_KEY_LEN, "graph.show_3d");
		zbx_snprintf(audit_key_percent_left,     AUDIT_DETAILS_KEY_LEN, "graph.percent_left");
		zbx_snprintf(audit_key_percent_right,    AUDIT_DETAILS_KEY_LEN, "graph.percent_right");
		zbx_snprintf(audit_key_ymin_type,        AUDIT_DETAILS_KEY_LEN, "graph.ymin_type");
		zbx_snprintf(audit_key_ymax_type,        AUDIT_DETAILS_KEY_LEN, "graph.ymax_type");
		zbx_snprintf(audit_key_ymin_itemid,      AUDIT_DETAILS_KEY_LEN, "graph.ymin_itemid");
		zbx_snprintf(audit_key_ymax_itemid,      AUDIT_DETAILS_KEY_LEN, "graph.ymax_itemid");
		zbx_snprintf(audit_key_flags,            AUDIT_DETAILS_KEY_LEN, "graph.flags");
		zbx_snprintf(audit_key_discover,         AUDIT_DETAILS_KEY_LEN, "graph.discover");
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		local_audit_graph_entry->resource_type = AUDIT_RESOURCE_GRAPH_PROTOTYPE;

		zbx_snprintf(audit_key_width,            AUDIT_DETAILS_KEY_LEN, "graphprototype.width");
		zbx_snprintf(audit_key_height,           AUDIT_DETAILS_KEY_LEN, "graphprototype.height");
		zbx_snprintf(audit_key_yaxismin,         AUDIT_DETAILS_KEY_LEN, "graphprototype.yaxismin");
		zbx_snprintf(audit_key_yaxismax,         AUDIT_DETAILS_KEY_LEN, "graphprototype.yaxismax");
		zbx_snprintf(audit_key_show_work_period, AUDIT_DETAILS_KEY_LEN, "graphprototype.show_work_period");
		zbx_snprintf(audit_key_show_triggers,    AUDIT_DETAILS_KEY_LEN, "graphprototype.show_triggers");
		zbx_snprintf(audit_key_templateid,       AUDIT_DETAILS_KEY_LEN, "graphprototype.templateid");
		zbx_snprintf(audit_key_graphtype,        AUDIT_DETAILS_KEY_LEN, "graphprototype.graphtype");
		zbx_snprintf(audit_key_show_legend,      AUDIT_DETAILS_KEY_LEN, "graphprototype.show_legend");
		zbx_snprintf(audit_key_show_3d,          AUDIT_DETAILS_KEY_LEN, "graphprototype.show_3d");
		zbx_snprintf(audit_key_percent_left,     AUDIT_DETAILS_KEY_LEN, "graphprototype.percent_left");
		zbx_snprintf(audit_key_percent_right,    AUDIT_DETAILS_KEY_LEN, "graphprototype.percent_right");
		zbx_snprintf(audit_key_ymin_type,        AUDIT_DETAILS_KEY_LEN, "graphprototype.ymin_type");
		zbx_snprintf(audit_key_ymax_type,        AUDIT_DETAILS_KEY_LEN, "graphprototype.ymax_type");
		zbx_snprintf(audit_key_ymin_itemid,      AUDIT_DETAILS_KEY_LEN, "graphprototype.ymin_itemid");
		zbx_snprintf(audit_key_ymax_itemid,      AUDIT_DETAILS_KEY_LEN, "graphprototype.ymax_itemid");
		zbx_snprintf(audit_key_flags,            AUDIT_DETAILS_KEY_LEN, "graphprototype.flags");
		zbx_snprintf(audit_key_discover,         AUDIT_DETAILS_KEY_LEN, "graphprototype.discover");
	}

	zbx_json_init(&(local_audit_graph_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_width, (zbx_uint64_t)width);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_height, (zbx_uint64_t)height);
	zbx_json_addfloat(&local_audit_graph_entry->details_json, audit_key_yaxismin, yaxismin);
	zbx_json_addfloat(&local_audit_graph_entry->details_json, audit_key_yaxismax, yaxismax);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_show_work_period, show_work_period);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_show_triggers, show_triggers);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_templateid, graphid);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_graphtype, graphtype);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_show_legend, show_legend);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_show_3d, show_3d);
	zbx_json_addfloat(&local_audit_graph_entry->details_json, audit_key_percent_left, percent_left);
	zbx_json_addfloat(&local_audit_graph_entry->details_json, audit_key_percent_right, percent_right);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_ymin_type, ymin_type);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_ymax_type, ymax_type);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_ymin_itemid, ymin_itemid);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_ymax_itemid, ymax_itemid);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_flags, flags);
	zbx_json_adduint64(&local_audit_graph_entry->details_json, audit_key_discover, discover);

	zbx_hashset_insert(&zbx_audit, &local_audit_graph_entry, sizeof(local_audit_graph_entry));
}

void	zbx_audit_graphs_update_gitems(zbx_uint64_t hst_graphid, int flags, zbx_uint64_t gitemid, int drawtype,
		int sortorder, const char *color, int yaxisside, int calc_fnc, int type)
{
	char	audit_key_drawtype[AUDIT_DETAILS_KEY_LEN], audit_key_sortorder[AUDIT_DETAILS_KEY_LEN],
		audit_key_color[AUDIT_DETAILS_KEY_LEN], audit_key_yaxisside[AUDIT_DETAILS_KEY_LEN],
		audit_key_calc_fnc[AUDIT_DETAILS_KEY_LEN], audit_key_type[AUDIT_DETAILS_KEY_LEN];

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
	{
		zbx_snprintf(audit_key_drawtype, AUDIT_DETAILS_KEY_LEN, "graph.gitems[%lu].drawtype", gitemid);
		zbx_snprintf(audit_key_sortorder, AUDIT_DETAILS_KEY_LEN, "graph.gitems[%lu].sortorder", gitemid);
		zbx_snprintf(audit_key_color, AUDIT_DETAILS_KEY_LEN, "graph.gitems[%lu].color", gitemid);
		zbx_snprintf(audit_key_yaxisside, AUDIT_DETAILS_KEY_LEN, "graph.gitems[%lu].yaxisside", gitemid);
		zbx_snprintf(audit_key_calc_fnc, AUDIT_DETAILS_KEY_LEN, "graph.gitems[%lu].calc_fnc", gitemid);
		zbx_snprintf(audit_key_type, AUDIT_DETAILS_KEY_LEN, "graph.gitems[%lu].type", gitemid);
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		zbx_snprintf(audit_key_drawtype, AUDIT_DETAILS_KEY_LEN, "graphprototype.gitems[%lu].drawtype", gitemid);
		zbx_snprintf(audit_key_sortorder, AUDIT_DETAILS_KEY_LEN, "graphprototype.gitems[%lu].sortorder",
				gitemid);
		zbx_snprintf(audit_key_color, AUDIT_DETAILS_KEY_LEN, "graphprototype.gitems[%lu].color", gitemid);
		zbx_snprintf(audit_key_yaxisside, AUDIT_DETAILS_KEY_LEN, "graphprototype.gitems[%lu].yaxisside",
				gitemid);
		zbx_snprintf(audit_key_calc_fnc, AUDIT_DETAILS_KEY_LEN, "graphprototype.gitems[%lu].calc_fnc", gitemid);
		zbx_snprintf(audit_key_type, AUDIT_DETAILS_KEY_LEN, "graphprototype.gitems[%lu].type", gitemid);
	}

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags || ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		zbx_audit_update_json_uint64(hst_graphid, audit_key_drawtype, (zbx_uint64_t)drawtype);
		zbx_audit_update_json_uint64(hst_graphid, audit_key_sortorder, (zbx_uint64_t)sortorder);
		zbx_audit_update_json_string(hst_graphid, audit_key_color, color);
		zbx_audit_update_json_uint64(hst_graphid, audit_key_yaxisside, (zbx_uint64_t)yaxisside);
		zbx_audit_update_json_uint64(hst_graphid, audit_key_calc_fnc, (zbx_uint64_t)calc_fnc);
		zbx_audit_update_json_uint64(hst_graphid, audit_key_type, (zbx_uint64_t)type);
	}
}

void	zbx_audit_triggers_create_entry(const int audit_action, zbx_uint64_t new_triggerid, const char *description,
		zbx_uint64_t templateid, unsigned char recovery_mode, unsigned char status, unsigned char type,
		zbx_uint64_t value, zbx_uint64_t state, unsigned char priority, const char *comments, const char *url,
		unsigned char flags, unsigned char correlation_mode, const char *correlation_tag,
		unsigned char manual_close, const char *opdata, unsigned char discover, const char *event_name)
{
	char	audit_key_event_name[AUDIT_DETAILS_KEY_LEN], audit_key_opdata[AUDIT_DETAILS_KEY_LEN],
		audit_key_comments[AUDIT_DETAILS_KEY_LEN], audit_key_flags[AUDIT_DETAILS_KEY_LEN],
		audit_key_priority[AUDIT_DETAILS_KEY_LEN], audit_key_state[AUDIT_DETAILS_KEY_LEN],
		audit_key_status[AUDIT_DETAILS_KEY_LEN], audit_key_templateid[AUDIT_DETAILS_KEY_LEN],
		audit_key_type[AUDIT_DETAILS_KEY_LEN], audit_key_url[AUDIT_DETAILS_KEY_LEN],
		audit_key_value[AUDIT_DETAILS_KEY_LEN], audit_key_recovery_mode[AUDIT_DETAILS_KEY_LEN],
		audit_key_correlation_mode[AUDIT_DETAILS_KEY_LEN], audit_key_correlation_tag[AUDIT_DETAILS_KEY_LEN],
		audit_key_manual_close[AUDIT_DETAILS_KEY_LEN], audit_key_discover[AUDIT_DETAILS_KEY_LEN];

	zbx_audit_entry_t	*local_audit_trigger_entry;

	local_audit_trigger_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_trigger_entry->id = new_triggerid;
	local_audit_trigger_entry->name = zbx_strdup(NULL, description);
	local_audit_trigger_entry->audit_action = audit_action;

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
	{
		local_audit_trigger_entry->resource_type = AUDIT_RESOURCE_TRIGGER;

		zbx_snprintf(audit_key_event_name,       AUDIT_DETAILS_KEY_LEN, "trigger.event_name");
		zbx_snprintf(audit_key_opdata,           AUDIT_DETAILS_KEY_LEN, "trigger.opdata");
		zbx_snprintf(audit_key_comments,         AUDIT_DETAILS_KEY_LEN, "trigger.comments");
		zbx_snprintf(audit_key_flags,            AUDIT_DETAILS_KEY_LEN, "trigger.flags");
		zbx_snprintf(audit_key_priority,         AUDIT_DETAILS_KEY_LEN, "trigger.priority");
		zbx_snprintf(audit_key_state,            AUDIT_DETAILS_KEY_LEN, "trigger.state");
		zbx_snprintf(audit_key_status,           AUDIT_DETAILS_KEY_LEN, "trigger.status");
		zbx_snprintf(audit_key_templateid,       AUDIT_DETAILS_KEY_LEN, "trigger.tempateid");
		zbx_snprintf(audit_key_type,             AUDIT_DETAILS_KEY_LEN, "trigger.type");
		zbx_snprintf(audit_key_url,              AUDIT_DETAILS_KEY_LEN, "trigger.url");
		zbx_snprintf(audit_key_value,            AUDIT_DETAILS_KEY_LEN, "trigger.value");
		zbx_snprintf(audit_key_recovery_mode,    AUDIT_DETAILS_KEY_LEN, "trigger.recovery_mode");
		zbx_snprintf(audit_key_correlation_mode, AUDIT_DETAILS_KEY_LEN, "trigger.correlation_mode");
		zbx_snprintf(audit_key_correlation_tag,  AUDIT_DETAILS_KEY_LEN, "trigger.correlation_tag");
		zbx_snprintf(audit_key_manual_close,     AUDIT_DETAILS_KEY_LEN, "trigger.manual_close");
		zbx_snprintf(audit_key_discover,         AUDIT_DETAILS_KEY_LEN, "trigger.discover");
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		local_audit_trigger_entry->resource_type = AUDIT_RESOURCE_TRIGGER_PROTOTYPE;

		zbx_snprintf(audit_key_event_name,       AUDIT_DETAILS_KEY_LEN, "triggerprototype.event_name");
		zbx_snprintf(audit_key_opdata,           AUDIT_DETAILS_KEY_LEN, "triggerprototype.opdata");
		zbx_snprintf(audit_key_comments,         AUDIT_DETAILS_KEY_LEN, "triggerprototype.comments");
		zbx_snprintf(audit_key_flags,            AUDIT_DETAILS_KEY_LEN, "triggerprototype.flags");
		zbx_snprintf(audit_key_priority,         AUDIT_DETAILS_KEY_LEN, "triggerprototype.priority");
		zbx_snprintf(audit_key_state,            AUDIT_DETAILS_KEY_LEN, "triggerprototype.state");
		zbx_snprintf(audit_key_status,           AUDIT_DETAILS_KEY_LEN, "triggerprototype.status");
		zbx_snprintf(audit_key_templateid,       AUDIT_DETAILS_KEY_LEN, "triggerprototype.tempateid");
		zbx_snprintf(audit_key_type,             AUDIT_DETAILS_KEY_LEN, "triggerprototype.type");
		zbx_snprintf(audit_key_url,              AUDIT_DETAILS_KEY_LEN, "triggerprototype.url");
		zbx_snprintf(audit_key_value,            AUDIT_DETAILS_KEY_LEN, "triggerprototype.value");
		zbx_snprintf(audit_key_recovery_mode,    AUDIT_DETAILS_KEY_LEN, "triggerprototype.recovery_mode");
		zbx_snprintf(audit_key_correlation_mode, AUDIT_DETAILS_KEY_LEN, "triggerprototype.correlation_mode");
		zbx_snprintf(audit_key_correlation_tag,  AUDIT_DETAILS_KEY_LEN, "triggerprototype.correlation_tag");
		zbx_snprintf(audit_key_manual_close,     AUDIT_DETAILS_KEY_LEN, "triggerprototype.manual_close");
		zbx_snprintf(audit_key_discover,         AUDIT_DETAILS_KEY_LEN, "triggerprototype.discover");
	}

	zbx_json_init(&(local_audit_trigger_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	zbx_json_addstring(&local_audit_trigger_entry->details_json, audit_key_event_name, event_name,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_audit_trigger_entry->details_json, audit_key_opdata, opdata, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_audit_trigger_entry->details_json, audit_key_comments, comments,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_flags, flags);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_priority, priority);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_state, state);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_status, status);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_templateid, templateid);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_type, type);
	zbx_json_addstring(&local_audit_trigger_entry->details_json, audit_key_url, url, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_value, value);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_recovery_mode, recovery_mode);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_correlation_mode, correlation_mode);
	zbx_json_addstring(&local_audit_trigger_entry->details_json, audit_key_correlation_tag, correlation_tag,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_manual_close, manual_close);
	zbx_json_adduint64(&local_audit_trigger_entry->details_json, audit_key_discover, discover);

	zbx_hashset_insert(&zbx_audit, &local_audit_trigger_entry, sizeof(local_audit_trigger_entry));
}

void	zbx_audit_triggers_update_expression_and_recovery_expression(zbx_uint64_t new_triggerid, int flags,
		const char *new_expression, const char *new_recovery_expression)
{
	char	audit_key_expression[AUDIT_DETAILS_KEY_LEN], audit_key_recovery_expression[AUDIT_DETAILS_KEY_LEN];

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
	{
		zbx_snprintf(audit_key_expression, AUDIT_DETAILS_KEY_LEN, "trigger.expression");
		zbx_snprintf(audit_key_recovery_expression, AUDIT_DETAILS_KEY_LEN, "trigger.recovery_expression");
		zbx_audit_update_json_string(new_triggerid, audit_key_expression,
				new_expression);
		zbx_audit_update_json_string(new_triggerid, audit_key_recovery_expression,
				new_recovery_expression);
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		zbx_snprintf(audit_key_expression, AUDIT_DETAILS_KEY_LEN, "triggerprototype.expression");
		zbx_snprintf(audit_key_recovery_expression, AUDIT_DETAILS_KEY_LEN,
				"triggerprototype.recovery_expression");
		zbx_audit_update_json_string(new_triggerid, audit_key_expression,
				new_expression);
		zbx_audit_update_json_string(new_triggerid, audit_key_recovery_expression,
				new_recovery_expression);
	}
}

void	zbx_audit_triggers_update_dependencies(const char *triggerid_up_str, const char *triggerid_str,
		const char *flags_str, const char *triggerdepid_str)
{
	int		flags;
	zbx_uint64_t	triggerid;
	char		audit_key_dependencies[AUDIT_DETAILS_KEY_LEN];

	ZBX_STR2UINT64(flags, flags_str);
	ZBX_STR2UINT64(triggerid, triggerid_str);

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
	{
		zbx_snprintf(audit_key_dependencies, AUDIT_DETAILS_KEY_LEN, "trigger.dependencies[%s]",
				triggerdepid_str);
		zbx_audit_update_json_string(triggerid, audit_key_dependencies,
				triggerid_up_str);
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		zbx_snprintf(audit_key_dependencies, AUDIT_DETAILS_KEY_LEN, "triggerprototype.dependencies[%s]",
				triggerdepid_str);
		zbx_audit_update_json_string(triggerid, audit_key_dependencies,
				triggerid_up_str);
	}
}

void	zbx_audit_triggers_update_tags_and_values(zbx_uint64_t triggerid, const char *tag, const char *value,
		const char *flags_str, const char *tagid_str)
{
	int	flags;
	char	audit_key_tags_tag[AUDIT_DETAILS_KEY_LEN], audit_key_tags_value[AUDIT_DETAILS_KEY_LEN];

	ZBX_STR2UINT64(flags, flags_str);

	if (ZBX_FLAG_DISCOVERY_NORMAL == flags)
	{
		zbx_snprintf(audit_key_tags_tag, AUDIT_DETAILS_KEY_LEN, "trigger.tags[%s].tag", tagid_str);
		zbx_snprintf(audit_key_tags_value, AUDIT_DETAILS_KEY_LEN, "trigger.tags[%s].value", tagid_str);
		zbx_audit_update_json_string(triggerid, audit_key_tags_tag, tag);
		zbx_audit_update_json_string(triggerid, audit_key_tags_value, value);
	}
	else if (ZBX_FLAG_DISCOVERY_PROTOTYPE == flags)
	{
		zbx_snprintf(audit_key_tags_tag, AUDIT_DETAILS_KEY_LEN, "triggerprototype.tags[%s].tag", tagid_str);
		zbx_snprintf(audit_key_tags_value, AUDIT_DETAILS_KEY_LEN, "triggerprototype.tags[%s].value", tagid_str);
		zbx_audit_update_json_string(triggerid, audit_key_tags_tag, tag);
		zbx_audit_update_json_string(triggerid, audit_key_tags_value, value);
	}
}

void	zbx_audit_httptests_create_entry_add(zbx_uint64_t httptestid, char *name, char *delay,
		unsigned char status, char *agent, unsigned char authentication, char *http_user, char *http_password,
		char *http_proxy, int retries, uint64_t hostid, zbx_uint64_t templateid)
{
	zbx_audit_entry_t	*local_audit_http_test_entry;

	local_audit_http_test_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_http_test_entry->id = httptestid;
	local_audit_http_test_entry->name = zbx_strdup(NULL, name);
	local_audit_http_test_entry->audit_action = AUDIT_ACTION_ADD;
	local_audit_http_test_entry->resource_type = AUDIT_RESOURCE_SCENARIO;

	zbx_json_init(&(local_audit_http_test_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	zbx_json_addstring(&local_audit_http_test_entry->details_json, "httptest.delay", delay, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_audit_http_test_entry->details_json, "httptest.status", status);
	zbx_json_addstring(&local_audit_http_test_entry->details_json, "httptest.agent", agent, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_audit_http_test_entry->details_json, "httptest.authentication", authentication);
	zbx_json_addstring(&local_audit_http_test_entry->details_json, "httptest.http_user", http_user,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_audit_http_test_entry->details_json, "httptest.http_password", http_password,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_audit_http_test_entry->details_json, "httptest.http_proxy", http_proxy,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_audit_http_test_entry->details_json, "httptest.retries", (zbx_uint64_t)retries);
	zbx_json_adduint64(&local_audit_http_test_entry->details_json, "httptest.hostid", hostid);
	zbx_json_adduint64(&local_audit_http_test_entry->details_json, "httptest.templateid", templateid);

	zbx_hashset_insert(&zbx_audit, &local_audit_http_test_entry, sizeof(local_audit_http_test_entry));
}

void	zbx_audit_httptests_update_headers_and_variables(int type, zbx_uint64_t httpstepid, zbx_uint64_t httptestid,
		const char *name, const char *value)
{
	char	audit_key_name[AUDIT_DETAILS_KEY_LEN], audit_key_value[AUDIT_DETAILS_KEY_LEN];

	if (ZBX_HTTPFIELD_HEADER == type)
	{
		zbx_snprintf(audit_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.headers[%lu].name", httpstepid);
		zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "httptest.headers[%lu].value", httpstepid);
	}
	else if (ZBX_HTTPFIELD_VARIABLE == type)
	{
		zbx_snprintf(audit_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.variables[%lu].name", httpstepid);
		zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "httptest.variables[%lu].value", httpstepid);
	}

	zbx_audit_update_json_string(httptestid, audit_key_name, name);
	zbx_audit_update_json_string(httptestid, audit_key_value, value);
}

void	zbx_audit_httptests_steps_update(zbx_uint64_t httpstepid, zbx_uint64_t httptestid, int no,
		const char *name, const char *url, const char *timeout, const char *posts, const char *required,
		const char *status_codes, zbx_uint64_t follow_redirects, zbx_uint64_t retrieve_mode)
{
	char	audit_key_name[AUDIT_DETAILS_KEY_LEN], audit_key_url[AUDIT_DETAILS_KEY_LEN],
		audit_key_timeout[AUDIT_DETAILS_KEY_LEN], audit_key_posts[AUDIT_DETAILS_KEY_LEN],
		audit_key_required[AUDIT_DETAILS_KEY_LEN], audit_key_status_codes[AUDIT_DETAILS_KEY_LEN],
		audit_key_follow_redirects[AUDIT_DETAILS_KEY_LEN], audit_key_retrieve_mode[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].name", httpstepid, no);
	zbx_snprintf(audit_key_url, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].url", httpstepid, no);
	zbx_snprintf(audit_key_timeout, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].timeout", httpstepid, no);
	zbx_snprintf(audit_key_posts, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].posts", httpstepid, no);
	zbx_snprintf(audit_key_required, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].required", httpstepid, no);
	zbx_snprintf(audit_key_status_codes, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].status_codes",
			httpstepid, no);
	zbx_snprintf(audit_key_follow_redirects, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].follow_redirects",
			httpstepid, no);
	zbx_snprintf(audit_key_retrieve_mode, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].no[%d].retrieve_mode",
			httpstepid, no);

	zbx_audit_update_json_string(httptestid, audit_key_name, name);
	zbx_audit_update_json_string(httptestid, audit_key_url, url);
	zbx_audit_update_json_string(httptestid, audit_key_timeout, timeout);
	zbx_audit_update_json_string(httptestid, audit_key_posts, posts);
	zbx_audit_update_json_string(httptestid, audit_key_required, required);
	zbx_audit_update_json_string(httptestid, audit_key_status_codes, status_codes);
	zbx_audit_update_json_uint64(httptestid, audit_key_follow_redirects, follow_redirects);
	zbx_audit_update_json_uint64(httptestid, audit_key_retrieve_mode, retrieve_mode);
}

void	zbx_audit_httptests_steps_update_extra(int type, zbx_uint64_t httpstepid, int field_no, zbx_uint64_t httptestid,
		const char *name, const char *value)
{
	char	audit_step_key_name[AUDIT_DETAILS_KEY_LEN], audit_step_key_value[AUDIT_DETAILS_KEY_LEN];

	if (ZBX_HTTPFIELD_HEADER == type)
	{
		zbx_snprintf(audit_step_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].headers[%d].name",
				httpstepid, field_no);
		zbx_snprintf(audit_step_key_value, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].headers[%d].value",
				httpstepid, field_no);
	}
	else if (ZBX_HTTPFIELD_VARIABLE == type)
	{
		zbx_snprintf(audit_step_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].variables[%d].name",
				httpstepid, field_no);
		zbx_snprintf(audit_step_key_value, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].variables[%d].value",
				httpstepid, field_no);
	}
	else if (ZBX_HTTPFIELD_POST_FIELD == type)
	{
		zbx_snprintf(audit_step_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].posts[%d].name",
				httpstepid, field_no);
		zbx_snprintf(audit_step_key_value, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].posts[%d].value",
				httpstepid, field_no);
	}
	else if (ZBX_HTTPFIELD_QUERY_FIELD == type)
	{
		zbx_snprintf(audit_step_key_name, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].query_fields[%d].name",
				httpstepid, field_no);
		zbx_snprintf(audit_step_key_value, AUDIT_DETAILS_KEY_LEN, "httptest.steps[%lu].query_fields[%d].value",
				httpstepid, field_no);
	}
	else
		THIS_SHOULD_NEVER_HAPPEN;

	zbx_audit_update_json_string(httptestid, audit_step_key_name, name);
	zbx_audit_update_json_string(httptestid, audit_step_key_value, value);
}

void	zbx_audit_httptests_create_entry_update(zbx_uint64_t httptestid, char *name,
		zbx_uint64_t templateid)
{
	zbx_audit_entry_t	*local_audit_http_test_entry;

	local_audit_http_test_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_http_test_entry->id = httptestid;
	local_audit_http_test_entry->name = zbx_strdup(NULL, name);
	local_audit_http_test_entry->audit_action = AUDIT_ACTION_UPDATE;
	local_audit_http_test_entry->resource_type = AUDIT_RESOURCE_SCENARIO;

	zbx_json_init(&(local_audit_http_test_entry->details_json), ZBX_JSON_STAT_BUF_LEN);
	zbx_json_adduint64(&local_audit_http_test_entry->details_json, "httptest.templateid", templateid);
	zbx_hashset_insert(&zbx_audit, &local_audit_http_test_entry, sizeof(local_audit_http_test_entry));
}

void	zbx_audit_discovery_rule_overrides_update(int item_no, int rule_condition_no, zbx_uint64_t itemid,
		zbx_uint64_t op, const char *macro, const char *value)
{
	char	audit_key_operator[AUDIT_DETAILS_KEY_LEN], audit_key_macro[AUDIT_DETAILS_KEY_LEN],
		audit_key_value[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_operator, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].filter.conditions[%d].operator", item_no, rule_condition_no);
	zbx_snprintf(audit_key_macro, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].filter.conditions[%d].macro", item_no, rule_condition_no);
	zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].filter.conditions[%d].value", item_no, rule_condition_no);

	zbx_audit_update_json_uint64(itemid, audit_key_operator, op);
	zbx_audit_update_json_string(itemid, audit_key_macro, macro);
	zbx_audit_update_json_string(itemid, audit_key_value, value);
}

void	zbx_audit_discovery_rule_override_conditions_update(int audit_index, zbx_uint64_t itemid, zbx_uint64_t op,
		const char *macro, const char *value)
{
	char	audit_key_operator[AUDIT_DETAILS_KEY_LEN], audit_key_macro[AUDIT_DETAILS_KEY_LEN],
		audit_key_value[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_operator, AUDIT_DETAILS_KEY_LEN, "discoveryrule.filter.conditions[%d].operator",
			audit_index);
	zbx_snprintf(audit_key_macro, AUDIT_DETAILS_KEY_LEN, "discoveryrule.filter.conditions[%d].macro", audit_index);
	zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "discoveryrule.filter.conditions[%d].value", audit_index);

	zbx_audit_update_json_uint64(itemid, audit_key_operator, op);
	zbx_audit_update_json_string(itemid, audit_key_macro, macro);
	zbx_audit_update_json_string(itemid, audit_key_value, value);
}

void	zbx_audit_preprocessing_update(zbx_uint64_t itemid, unsigned char flags, const char *step,
		const char *type, const char *params, const char *error_handler, const char *error_handler_params)
{
	char	audit_key_type[AUDIT_DETAILS_KEY_LEN], audit_key_params[AUDIT_DETAILS_KEY_LEN],
		audit_key_error_handler[AUDIT_DETAILS_KEY_LEN], audit_key_error_handler_params[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_type, AUDIT_DETAILS_KEY_LEN, "%s.preprocessing[%s].type",
			zbx_audit_items_get_type_json_identifier(flags), step);
	zbx_snprintf(audit_key_params, AUDIT_DETAILS_KEY_LEN, "%s.preprocessing[%s].params",
			zbx_audit_items_get_type_json_identifier(flags), step);
	zbx_snprintf(audit_key_error_handler, AUDIT_DETAILS_KEY_LEN, "%s.preprocessing[%s].error_handler",
			zbx_audit_items_get_type_json_identifier(flags), step);
	zbx_snprintf(audit_key_error_handler_params, AUDIT_DETAILS_KEY_LEN, "%s.preprocessing[%s].error_handler_params",
			zbx_audit_items_get_type_json_identifier(flags), step);

	zbx_audit_update_json_string(itemid, audit_key_type, type);
	zbx_audit_update_json_string(itemid, audit_key_params, params);
	zbx_audit_update_json_string(itemid, audit_key_error_handler, error_handler);
	zbx_audit_update_json_string(itemid, audit_key_error_handler_params, error_handler_params);
}

void	zbx_audit_item_parameters_update(int audit_index, zbx_uint64_t itemid, const char *name, const char *value,
		const char *flags_str)
{
	int	resource_type, flags;
	char	audit_key_name[AUDIT_DETAILS_KEY_LEN], audit_key_value[AUDIT_DETAILS_KEY_LEN];

	ZBX_STR2UINT64(flags, flags_str);
	resource_type = item_flag_to_resource_type(flags);

	if (AUDIT_RESOURCE_ITEM == resource_type)
	{
		zbx_snprintf(audit_key_name, AUDIT_DETAILS_KEY_LEN, "item.parameters[%d].name", audit_index);
		zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "item.parameters[%d].value", audit_index);
	}
	else if (AUDIT_RESOURCE_ITEM_PROTOTYPE == resource_type)
	{
		zbx_snprintf(audit_key_name, AUDIT_DETAILS_KEY_LEN, "itemprototype.parameters[%d].name", audit_index);
		zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "itemprototype.parameters[%d].value", audit_index);

	}
	else if (AUDIT_RESOURCE_DISCOVERY_RULE == resource_type)
	{
		zbx_snprintf(audit_key_name, AUDIT_DETAILS_KEY_LEN, "discoveryrule.parameters[%d].name", audit_index);
		zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "discoveryrule.parameters[%d].value", audit_index);
	}
	else
		THIS_SHOULD_NEVER_HAPPEN;

	zbx_audit_update_json_string(itemid, audit_key_name, name);
	zbx_audit_update_json_string(itemid, audit_key_value, value);
}

void	zbx_audit_discovery_rule_lld_macro_paths_update(zbx_uint64_t no, zbx_uint64_t itemid, const char *lld_macro,
		const char *path)
{
	char	audit_key_lld_macro[AUDIT_DETAILS_KEY_LEN], audit_key_lld_path[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_lld_macro, AUDIT_DETAILS_KEY_LEN, "discoveryrule.lld_macro_paths[%lu].lld_macro", no);
	zbx_snprintf(audit_key_lld_path, AUDIT_DETAILS_KEY_LEN, "discoveryrule.lld_macro_paths[%lu].path", no);

	zbx_audit_update_json_string(itemid, audit_key_lld_macro, lld_macro);
	zbx_audit_update_json_string(itemid, audit_key_lld_path, path);
}

void	zbx_audit_discovery_rule_overrides_operations_update(int override_no, int operation_no, zbx_uint64_t itemid,
		zbx_uint64_t operation_type, zbx_uint64_t operator, const char *value)
{
	char	audit_key_operationobject[AUDIT_DETAILS_KEY_LEN], audit_key_operator[AUDIT_DETAILS_KEY_LEN],
		audit_key_value[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_operationobject, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].operations[%d].operationobject", override_no, operation_no);
	zbx_snprintf(audit_key_operator, AUDIT_DETAILS_KEY_LEN, "discoveryrule.overrides[%d].operations[%d].operator",
			override_no, operation_no);
	zbx_snprintf(audit_key_value, AUDIT_DETAILS_KEY_LEN, "discoveryrule.overrides[%d].operations[%d].value",
			override_no, operation_no);
	zbx_audit_update_json_uint64(itemid, audit_key_operationobject, operation_type);
	zbx_audit_update_json_uint64(itemid, audit_key_operator, operator);
	zbx_audit_update_json_string(itemid, audit_key_value, value);
}

void	zbx_audit_discovery_rule_overrides_operations_update_extra(int override_no, int operation_no,
		zbx_lld_override_operation_t *override_operation, zbx_uint64_t itemid)
{
	if (ZBX_PROTOTYPE_STATUS_COUNT != override_operation->status)
	{
		char	audit_key_opstatus[AUDIT_DETAILS_KEY_LEN];

		zbx_snprintf(audit_key_opstatus, AUDIT_DETAILS_KEY_LEN,
				"discoveryrule.overrides[%d].operations[%d].opstatus", override_no, operation_no);
		zbx_audit_update_json_uint64(itemid, audit_key_opstatus, override_operation->status);
	}

	if (ZBX_PROTOTYPE_DISCOVER_COUNT != override_operation->discover)
	{
		char audit_key_opdiscover[AUDIT_DETAILS_KEY_LEN];

		zbx_snprintf(audit_key_opdiscover, AUDIT_DETAILS_KEY_LEN,
				"discoveryrule.overrides[%d].operations[%d].opdiscover", override_no, operation_no);
		zbx_audit_update_json_uint64(itemid, audit_key_opdiscover, override_operation->discover);
	}

	if (NULL != override_operation->delay)
	{
		char	audit_key_opperiod[AUDIT_DETAILS_KEY_LEN];

		zbx_snprintf(audit_key_opperiod, AUDIT_DETAILS_KEY_LEN,
				"discoveryrule.overrides[%d].operations[%d].opperiod", override_no, operation_no);
		zbx_audit_update_json_string(itemid, audit_key_opperiod,
				override_operation->delay);
	}

	if (NULL != override_operation->history)
	{
		char	audit_key_ophistory[AUDIT_DETAILS_KEY_LEN];

		zbx_snprintf(audit_key_ophistory, AUDIT_DETAILS_KEY_LEN,
				"discoveryrule.overrides[%d].operations[%d].ophistory", override_no, operation_no);
		zbx_audit_update_json_string(itemid, audit_key_ophistory,
				override_operation->history);
	}

	if (NULL != override_operation->trends)
	{
		char	audit_key_optrends[AUDIT_DETAILS_KEY_LEN];

		zbx_snprintf(audit_key_optrends, AUDIT_DETAILS_KEY_LEN,
				"discoveryrule.overrides[%d].operations[%d].optrends", override_no, operation_no);
		zbx_audit_update_json_string(itemid, audit_key_optrends,
				override_operation->trends);
	}

	if (TRIGGER_SEVERITY_COUNT != override_operation->severity)
	{
		char	audit_key_severity[AUDIT_DETAILS_KEY_LEN];

		zbx_snprintf(audit_key_severity, AUDIT_DETAILS_KEY_LEN,
				"discoveryrule.overrides[%d].operations[%d].opseverity", override_no, operation_no);
		zbx_audit_update_json_uint64(itemid, audit_key_severity,
				override_operation->severity);
	}
}

void	zbx_audit_discovery_rule_overrides_operations_optag_update(int override_no, int override_operation_no,
		int override_operation_tag_no, zbx_uint64_t itemid, const char *tag, const char *value)
{
	char	audit_key_optag_tag[AUDIT_DETAILS_KEY_LEN], audit_key_optag_value[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_optag_tag, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].operations[%d].optag[%d].tag", override_no, override_operation_no,
			override_operation_tag_no);
	zbx_snprintf(audit_key_optag_value, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].operations[%d].optag[%d].value", override_no,
			override_operation_no, override_operation_tag_no);
	zbx_audit_update_json_string(itemid, audit_key_optag_tag, tag);
	zbx_audit_update_json_string(itemid, audit_key_optag_value, value);
}

void	zbx_audit_discovery_rule_overrides_operations_optemplate_update(int override_no, int override_operation_no,
		int override_operation_tag_no, zbx_uint64_t itemid, zbx_uint64_t templateid)
{
	char	audit_key_optemplate[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_optemplate, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].operations[%d].optemplate[%d].templateid", override_no,
			override_operation_no, override_operation_tag_no);
	zbx_audit_update_json_uint64(itemid, audit_key_optemplate, templateid);
}

void	zbx_audit_discovery_rule_overrides_operations_opinventory_update(int override_no, int override_operation_no,
		zbx_uint64_t itemid, zbx_uint64_t inventory_mode)
{
	char	audit_key_opinventory[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_opinventory, AUDIT_DETAILS_KEY_LEN,
			"discoveryrule.overrides[%d].operations[%d].opinventory.inventory_mode",
			override_no, override_operation_no);
	zbx_audit_update_json_uint64(itemid, audit_key_opinventory, inventory_mode);
}

void	zbx_audit_host_update_interfaces(zbx_uint64_t hostid, zbx_uint64_t interfaceid, zbx_uint64_t main_,
		zbx_uint64_t type, zbx_uint64_t useip, const char *ip, const char *dns, zbx_uint64_t port)
{
	char	audit_key_main[AUDIT_DETAILS_KEY_LEN], audit_key_type[AUDIT_DETAILS_KEY_LEN],
		audit_key_useip[AUDIT_DETAILS_KEY_LEN], audit_key_ip[AUDIT_DETAILS_KEY_LEN],
		audit_key_dns[AUDIT_DETAILS_KEY_LEN], audit_key_port[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_main,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].main", interfaceid);
	zbx_snprintf(audit_key_type,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].type", interfaceid);
	zbx_snprintf(audit_key_useip, AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].useip", interfaceid);
	zbx_snprintf(audit_key_ip,    AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].ip", interfaceid);
	zbx_snprintf(audit_key_dns,   AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].dns", interfaceid);
	zbx_snprintf(audit_key_port,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].port", interfaceid);

	zbx_audit_update_json_uint64(hostid, audit_key_main, main_);
	zbx_audit_update_json_uint64(hostid, audit_key_type, type);
	zbx_audit_update_json_uint64(hostid, audit_key_useip, useip);
	zbx_audit_update_json_string(hostid, audit_key_ip, ip);
	zbx_audit_update_json_string(hostid, audit_key_dns, dns);
	zbx_audit_update_json_uint64(hostid, audit_key_port, port);
}

void	zbx_audit_host_update_snmp_interfaces(zbx_uint64_t hostid, zbx_uint64_t version, zbx_uint64_t bulk,
		const char *community, const char *securityname, zbx_uint64_t securitylevel, const char *authpassphrase,
		const char *privpassphrase, zbx_uint64_t authprotocol, zbx_uint64_t privprotocol,
		const char *contextname, zbx_uint64_t interfaceid)
{
	char	audit_key_version[AUDIT_DETAILS_KEY_LEN], audit_key_bulk[AUDIT_DETAILS_KEY_LEN],
		audit_key_community[AUDIT_DETAILS_KEY_LEN], audit_key_securityname[AUDIT_DETAILS_KEY_LEN],
		audit_key_securitylevel[AUDIT_DETAILS_KEY_LEN], audit_key_authpassphrase[AUDIT_DETAILS_KEY_LEN],
		audit_key_privpassphrase[AUDIT_DETAILS_KEY_LEN], audit_key_authprotocol[AUDIT_DETAILS_KEY_LEN],
		audit_key_privprotocol[AUDIT_DETAILS_KEY_LEN], audit_key_contextname[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_version,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.version", interfaceid);
	zbx_snprintf(audit_key_bulk,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.bulk", interfaceid);
	zbx_snprintf(audit_key_community, AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.community", interfaceid);
	zbx_snprintf(audit_key_securityname,    AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.securityname",
			interfaceid);
	zbx_snprintf(audit_key_securitylevel,   AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.securitylevel",
			interfaceid);
	zbx_snprintf(audit_key_authpassphrase,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.authpassphrase",
			interfaceid);
	zbx_snprintf(audit_key_privpassphrase,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.privpassphrase",
			interfaceid);
	zbx_snprintf(audit_key_authprotocol,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.authprotocol",
			interfaceid);
	zbx_snprintf(audit_key_privprotocol,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.privprotocol",
			interfaceid);
	zbx_snprintf(audit_key_contextname,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.contextname",
			interfaceid);

	zbx_audit_update_json_uint64(hostid, audit_key_version, version);
	zbx_audit_update_json_uint64(hostid, audit_key_bulk, bulk);
	zbx_audit_update_json_string(hostid, audit_key_community, community);
	zbx_audit_update_json_string(hostid, audit_key_securityname, securityname);
	zbx_audit_update_json_uint64(hostid, audit_key_securitylevel, securitylevel);
	zbx_audit_update_json_string(hostid, audit_key_authpassphrase, authpassphrase);
	zbx_audit_update_json_string(hostid, audit_key_privpassphrase, privpassphrase);
	zbx_audit_update_json_uint64(hostid, audit_key_authprotocol, authprotocol);
	zbx_audit_update_json_uint64(hostid, audit_key_privprotocol, privprotocol);
	zbx_audit_update_json_string(hostid, audit_key_contextname, contextname);
}

void	zbx_audit_update_json_string(const zbx_uint64_t id, const char *key, const char *value)
{
	zbx_audit_entry_t	local_audit_entry, **found_audit_entry;
	zbx_audit_entry_t	*local_audit_entry_x = &local_audit_entry;

	local_audit_entry.id = id;

	found_audit_entry = (zbx_audit_entry_t**)zbx_hashset_search(&zbx_audit,
			&(local_audit_entry_x));

	if (NULL == found_audit_entry)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	zbx_json_addstring(&((*found_audit_entry)->details_json), key, value, ZBX_JSON_TYPE_STRING);
}

void	zbx_audit_update_json_uint64(const zbx_uint64_t id, const char *key, const uint64_t value)
{
	zbx_audit_entry_t	local_audit_entry, **found_audit_entry;
	zbx_audit_entry_t	*local_audit_entry_x = &local_audit_entry;

	local_audit_entry.id = id;

	found_audit_entry = (zbx_audit_entry_t**)zbx_hashset_search(&zbx_audit,
			&(local_audit_entry_x));
	if (NULL == found_audit_entry)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	zbx_json_adduint64(&((*found_audit_entry)->details_json), key, value);
}

int	zbx_audit_create_entry(const int action, const zbx_uint64_t resourceid, const char* resourcename,
		const int resourcetype, const char *recsetid, const char *details)
{
	int	res = SUCCEED;
	char	auditid_cuid[CUID_LEN];

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_new_cuid(auditid_cuid);
	DBexecute("insert into auditlog2 (auditid,userid,clock,action,ip,resourceid,resourcename,resourcetype,"
			"recsetid,details) values ('%s',%d,%d,'%d','%s'," ZBX_FS_UI64 ",'%s',%d,'%s','%s' )",
			auditid_cuid, USER_TYPE_SUPER_ADMIN, (int)time(NULL), action, "", resourceid,
			resourcename, resourcetype, recsetid, details);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(res));

	return res;
}

void	zbx_audit_host_groups_delete_create_entry(zbx_uint64_t hostid, char *hostname, zbx_vector_uint64_t *groupids)
{
	int			i;
	zbx_audit_entry_t	*local_audit_host_groups_entry;

	local_audit_host_groups_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_host_groups_entry->id = hostid;
	local_audit_host_groups_entry->name = zbx_strdup(NULL, hostname);
	local_audit_host_groups_entry->audit_action = AUDIT_ACTION_UPDATE;
	local_audit_host_groups_entry->resource_type = AUDIT_RESOURCE_HOST_GROUP;

	zbx_json_init(&(local_audit_host_groups_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	zbx_hashset_insert(&zbx_audit, &local_audit_host_groups_entry, sizeof(local_audit_host_groups_entry));

	for (i = 0; i < groupids->values_num; i++)
		zbx_audit_host_update_groups(AUDIT_DETAILS_ACTION_DELETE, hostid, groupids->values[i]);
}

void	zbx_audit_host_update_tls_and_psk(zbx_uint64_t hostid, int tls_connect, int tls_accept,
		const char *psk_identity, const char *psk)
{
	zbx_audit_update_json_uint64(hostid, "host.tls_connect", (zbx_uint64_t)tls_connect);
	zbx_audit_update_json_uint64(hostid, "host.tls_accept", (zbx_uint64_t)tls_accept);
	zbx_audit_update_json_string(hostid, "host.psk_identity", psk_identity);
	zbx_audit_update_json_string(hostid, "host.psk", psk);
}

void	zbx_audit_host_create_entry(int audit_action, zbx_uint64_t hostid, const char *name)
{
	zbx_audit_entry_t	*local_audit_host_entry;

	local_audit_host_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_host_entry->id = hostid;
	local_audit_host_entry->name = zbx_strdup(NULL, name);
	local_audit_host_entry->audit_action = audit_action;
	local_audit_host_entry->resource_type = AUDIT_RESOURCE_HOST;
	zbx_json_init(&(local_audit_host_entry->details_json), ZBX_JSON_STAT_BUF_LEN);
	zbx_hashset_insert(&zbx_audit, &local_audit_host_entry, sizeof(local_audit_host_entry));
}

void	zbx_audit_host_update_groups(const char *audit_details_action, zbx_uint64_t hostid, zbx_uint64_t groupid)
{
	char	audit_key_groupid[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_groupid, AUDIT_DETAILS_KEY_LEN, "host.groups[%lu]", groupid);
	zbx_audit_update_json_string(hostid, audit_key_groupid, audit_details_action);
}

void	zbx_audit_host_del(zbx_uint64_t hostid, const char *hostname)
{
	char		recsetid_cuid[CUID_LEN];

	zbx_new_cuid(recsetid_cuid);
	zbx_audit_create_entry(AUDIT_ACTION_DELETE, hostid, hostname, AUDIT_RESOURCE_HOST, recsetid_cuid, "");
}
