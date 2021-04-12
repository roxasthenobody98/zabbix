#include "audit.h"
#include "zbxjson.h"
#include "db.h"
#include "log.h"

zbx_hashset_t items_audit;

typedef struct zbx_item_audit_entry
{
	zbx_uint64_t	itemid;
	char		*name;
	struct zbx_json	details_json;
	int		audit_action;
} zbx_item_audit_entry_t;

static unsigned	zbx_items_audit_hash_func(const void *data)
{
	const zbx_item_audit_entry_t	**item_audit_entry = (const zbx_item_audit_entry_t **)data;

	return ZBX_DEFAULT_UINT64_HASH_ALGO(&((*item_audit_entry)->itemid), sizeof(zbx_uint64_t),
			ZBX_DEFAULT_HASH_SEED);
}

static int	zbx_items_audit_compare_func(const void *d1, const void *d2)
{
	const zbx_item_audit_entry_t	**item_audit_entry_1 = (const zbx_item_audit_entry_t **)d1;
	const zbx_item_audit_entry_t	**item_audit_entry_2 = (const zbx_item_audit_entry_t **)d2;

	return (*item_audit_entry_1)->itemid > (*item_audit_entry_2)->itemid;
}

static void	clean_items_audit(void)
{
	zbx_hashset_iter_t	iter;
	zbx_item_audit_entry_t	**item_audit_entry;

	zbx_hashset_iter_reset(&items_audit, &iter);

	while (NULL != (item_audit_entry = (zbx_item_audit_entry_t **)zbx_hashset_iter_next(&iter)))
	{
		zbx_free((*item_audit_entry)->name);
		zbx_free(*item_audit_entry);
	}

	zbx_hashset_destroy(&items_audit);
}

void	zbx_items_audit_init(void)
{
	zbx_hashset_create(&items_audit, 10, zbx_items_audit_hash_func, zbx_items_audit_compare_func);
}


void	zbx_items_persist(const char *recsetid_cuid)
{
	zbx_hashset_iter_t	iter;
	zbx_item_audit_entry_t	**item_audit_entry;
	char			item_audit_cuid[CUID_LEN];
	zbx_db_insert_t		db_insert_items_audit;

	zbx_hashset_iter_reset(&items_audit, &iter);

	zbx_db_insert_prepare(&db_insert_items_audit, "auditlog2", "auditid", "userid", "clock", "action", "ip",
			"resourceid","resourcename","resourcetype","recsetid","details", NULL);

	while (NULL != (item_audit_entry = (zbx_item_audit_entry_t **)zbx_hashset_iter_next(&iter)))
	{
		zbx_new_cuid(item_audit_cuid);

		zbx_db_insert_add_values(&db_insert_items_audit, item_audit_cuid, USER_TYPE_SUPER_ADMIN,
				(int)time(NULL), (*item_audit_entry)->audit_action, "",  (*item_audit_entry)->itemid,
				(*item_audit_entry)->name, AUDIT_RESOURCE_ITEM, recsetid_cuid,
				(*item_audit_entry)->details_json.buffer);

	}

	zbx_db_insert_execute(&db_insert_items_audit);
	zbx_db_insert_clean(&db_insert_items_audit);

	clean_items_audit();
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

void	zbx_items_audit_bulk_delete(zbx_vector_uint64_t *itemids, zbx_vector_str_t *items_names, char *recsetid_cuid)
{
	size_t			i;
	char			item_audit_cuid[CUID_LEN];
	zbx_db_insert_t		db_insert_items_audit;

	zbx_db_insert_prepare(&db_insert_items_audit, "auditlog2", "auditid", "userid", "clock", "action", "ip",
			"resourceid","resourcename","resourcetype","recsetid","details", NULL);

	for (i = 0; i < itemids->values_num; i++)
	{
		zbx_new_cuid(item_audit_cuid);
		zbx_db_insert_add_values(&db_insert_items_audit, item_audit_cuid, USER_TYPE_SUPER_ADMIN,
				(int)time(NULL), AUDIT_ACTION_DELETE, "",  itemids->values[i],
				items_names->values[i], AUDIT_RESOURCE_ITEM, recsetid_cuid,
				"");
	}

	zbx_db_insert_execute(&db_insert_items_audit);
	zbx_db_insert_clean(&db_insert_items_audit);
}

void	zbx_items_audit_create_entry(const zbx_template_item_t *item, const zbx_uint64_t hostid, const int audit_action)
{
	zbx_item_audit_entry_t	*local_item_audit_entry = (zbx_item_audit_entry_t*)zbx_malloc(NULL,
			sizeof(zbx_item_audit_entry_t));
	local_item_audit_entry->itemid = item->itemid;
	local_item_audit_entry->name = zbx_strdup(NULL, item->name);
	local_item_audit_entry->audit_action = audit_action;

	zbx_json_init(&(local_item_audit_entry->details_json), ZBX_JSON_STAT_BUF_LEN);

	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.itemid", item->itemid);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.name", item->name, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.key", item->key, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.hostid", hostid);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.type", item->type);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.value_type", item->value_type);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.delay", item->delay, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.history", item->history, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.trends", item->trends, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.status", item->status);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.trapper_hosts", item->trapper_hosts,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.units", item->units, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.formula", item->formula, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.logtimefmt", item->logtimefmt,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.valuemapid", item->valuemapid);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.params", item->params, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.ipmi_sensor", item->ipmi_sensor,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.snmp_oid",
			item->snmp_oid, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.authtype", item->authtype);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.username", item->username,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.password", item->password,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.publickey", item->publickey,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.privatekey", item->privatekey,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.templateid", item->templateid);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.flags", item->flags);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.description", item->description,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.inventory_link", item->inventory_link);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.interfaceid", item->interfaceid);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.lifetime", item->lifetime,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.evaltype", item->evaltype);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.jmx_endpoint", item->jmx_endpoint,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.master_itemid", item->master_itemid);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.timeout", item->timeout, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.url", item->url, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.query_fields", item->query_fields,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.posts", item->posts, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.status_codes", item->status_codes,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.follow_redirects", item->follow_redirects);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.post_type", item->post_type);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.http_proxy", item->http_proxy,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.headers", item->headers, ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.retrieve_mode", item->retrieve_mode);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.request_method", item->request_method);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.output_format", item->output_format);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.ssl_cert_file", item->ssl_cert_file,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.ssl_key_file", item->ssl_key_file,
			ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&local_item_audit_entry->details_json, "item.ssl_key_password", item->ssl_key_password,
			ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.verify_peer", item->verify_peer);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.verify_host", item->verify_host);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.allowed_traps", item->allow_traps);
	zbx_json_adduint64(&local_item_audit_entry->details_json, "item.discover", item->discover);

	zbx_hashset_insert(&items_audit, &local_item_audit_entry, sizeof(local_item_audit_entry));
}

void	zbx_items_audit_update_json_string(const zbx_uint64_t itemid, const char *key, const char *value)
{
	zbx_item_audit_entry_t local_item_audit_entry, **found_item_audit_entry;
	zbx_item_audit_entry_t *local_item_audit_entry_x = &local_item_audit_entry;
	local_item_audit_entry.itemid = itemid;

	found_item_audit_entry = (zbx_item_audit_entry_t**)zbx_hashset_search(&items_audit,
			&(local_item_audit_entry_x));

	if (NULL == found_item_audit_entry)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	zbx_json_addstring(&((*found_item_audit_entry)->details_json), key, value, ZBX_JSON_TYPE_STRING);
}

void	zbx_items_audit_update_json_uint64(const zbx_uint64_t itemid, const char *key, const uint64_t value)
{
	zbx_item_audit_entry_t local_item_audit_entry, **found_item_audit_entry;
	zbx_item_audit_entry_t *local_item_audit_entry_x = &local_item_audit_entry;
	local_item_audit_entry.itemid = itemid;

	found_item_audit_entry = (zbx_item_audit_entry_t**)zbx_hashset_search(&items_audit,
			&(local_item_audit_entry_x));
	if (NULL == found_item_audit_entry)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	zbx_json_adduint64(&((*found_item_audit_entry)->details_json), key, value);
}

void	zbx_audit_groups_add(zbx_uint64_t hostid, zbx_uint64_t hostgroupid, zbx_uint64_t groupid)
{
		char	recsetid_cuid[CUID_LEN];
		struct zbx_json	details_json;
		DB_RESULT	result;
		DB_ROW			row;

		result = DBselect("select name from hstgrp where groupid=" ZBX_FS_UI64, groupid);

		if (NULL == result)
		{
			return;
		}

		while (NULL != (row = DBfetch(result)))
		{
			zbx_new_cuid(recsetid_cuid);

			zbx_json_init(&details_json, ZBX_JSON_STAT_BUF_LEN);
			zbx_json_addobject(&details_json, NULL);
			zbx_json_adduint64(&details_json, "hostgroupid", hostgroupid);
			zbx_json_adduint64(&details_json, "hostid", hostid);
			zbx_json_adduint64(&details_json, "groupid", groupid);

			zbx_json_close(&details_json);

			zbx_audit_create_entry(AUDIT_ACTION_ADD, hostid, row[0],
					AUDIT_RESOURCE_HOST_GROUP, recsetid_cuid, details_json.buffer);

			zbx_json_free(&details_json);
		}
		DBfree_result(result);
}

void	zbx_audit_groups_delete(zbx_uint64_t hostid)
{
		char	recsetid_cuid[CUID_LEN];
		struct zbx_json	details_json;
		DB_RESULT	result;
		DB_ROW			row;

		result = DBselect("select name from hstgrp where hostid=" ZBX_FS_UI64, hostid);

		if (NULL == result)
		{
			return;
		}

		while (NULL != (row = DBfetch(result)))
		{
			zbx_new_cuid(recsetid_cuid);
			zabbix_log(LOG_LEVEL_INFORMATION, "OP_AUDIT_GROUPS_DELETE RECSETID: ->%s<-\n",recsetid_cuid);
			zabbix_log(LOG_LEVEL_INFORMATION, "DEL GROUPID NAME: ->%s<-\n", row[0]);

			zbx_audit_create_entry(AUDIT_ACTION_DELETE, hostid, row[0],
					AUDIT_RESOURCE_HOST_GROUP, recsetid_cuid, "");

			zbx_json_free(&details_json);
		}
		DBfree_result(result);
}

void	zbx_audit_host_add(zbx_uint64_t hostid, char *recsetid_cuid)
{
	struct zbx_json	details_json;
	DB_RESULT	result;
	DB_ROW			row;

	result = DBselect("select hostid,proxy_hostid,host,status,lastaccess, ipmi_authtype,ipmi_privilege,ipmi_username,"
			"ipmi_password,maintenanceid,maintenance_status,maintenance_type,maintenance_from,"
			"name,flags,templateid,description,tls_connect,tls_accept,tls_issuer,tls_subject,"
			"tls_psk_identity,tls_psk,proxy_address,auto_compress,discover,custom_interfaces"
			" from hosts where hostid=" ZBX_FS_UI64, hostid);

	if (NULL == result)
	{
		return;
	}

	while (NULL != (row = DBfetch(result)))
	{
		zabbix_log(LOG_LEVEL_INFORMATION, "OP_TEMPLATE_ADD RECSETID: ->%s<-\n",recsetid_cuid);
		zabbix_log(LOG_LEVEL_INFORMATION, "NEW HOSTNAME: ->%s<-\n", row[13]);

		zbx_json_init(&details_json, ZBX_JSON_STAT_BUF_LEN);
		zbx_json_addobject(&details_json, NULL);
		zbx_json_addstring(&details_json, "hostid", row[0], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "proxy_hostid", row[1], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "host", row[2], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "status", row[3], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "lastaccess", row[4], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "ipmi_authtype", row[5], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "ipmi_privilege", row[6], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "ipmi_username", row[7], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "ipmi_password", row[8], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "maintenanceid", row[9], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "maintenance_status", row[10], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "maintenance_type", row[11], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "maintenance_from", row[12], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "name", row[13], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "flags", row[14], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "templateid", row[15], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "description", row[16], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "tls_connect", row[17], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "tls_accept", row[18], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "tls_issuer", row[19], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "tls_subject", row[20], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "tls_psk_identity", row[21], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "tls_psk", row[22], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "proxy_address", row[23], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "auto_compress", row[24], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "discover", row[25], ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&details_json, "custom_interfaces", row[26], ZBX_JSON_TYPE_STRING);

		zbx_json_close(&details_json);

		zbx_audit_create_entry(AUDIT_ACTION_ADD, hostid, row[13],
				AUDIT_RESOURCE_HOST, recsetid_cuid, details_json.buffer);

		zbx_json_free(&details_json);
	}
	DBfree_result(result);
}

void	zbx_audit_host_del(zbx_uint64_t hostid)
{
		char	recsetid_cuid[CUID_LEN];
		struct zbx_json	details_json;
		DB_RESULT	result;
		DB_ROW			row;

		result = DBselect("name from hosts where hostid=" ZBX_FS_UI64, hostid);

		if (NULL == result)
		{
			return;
		}

		while (NULL != (row = DBfetch(result)))
		{
			zbx_new_cuid(recsetid_cuid);
			zabbix_log(LOG_LEVEL_INFORMATION, "OP_TEMPLATE_DELETE RECSETID: ->%s<-\n",recsetid_cuid);
			zabbix_log(LOG_LEVEL_INFORMATION, "NEW HOSTNAME: ->%s<-\n", row[0]);


			zbx_audit_create_entry(AUDIT_ACTION_DELETE, hostid, row[0],
					AUDIT_RESOURCE_HOST, recsetid_cuid, "");

			zbx_json_free(&details_json);
		}
		DBfree_result(result);
}

void	zbx_audit_host_status(zbx_uint64_t hostid, int status)
{
	char	recsetid_cuid[CUID_LEN];
	struct zbx_json	details_json;
	DB_RESULT	result;
	DB_ROW			row;

	result = DBselect("name from hosts where hostid=" ZBX_FS_UI64, hostid);

	if (NULL == result)
	{
		return;
	}

	while (NULL != (row = DBfetch(result)))
	{
		zbx_new_cuid(recsetid_cuid);

		zbx_json_init(&details_json, ZBX_JSON_STAT_BUF_LEN);
		zbx_json_addobject(&details_json, NULL);
		zbx_json_adduint64(&details_json, "status", status);
		zbx_json_close(&details_json);

		zbx_audit_create_entry(AUDIT_ACTION_UPDATE, hostid, row[0],
				AUDIT_RESOURCE_HOST, recsetid_cuid, details_json.buffer);

		zbx_json_free(&details_json);
	}
	DBfree_result(result);
}

void	zbx_audit_host_inventory(zbx_uint64_t hostid, int inventory_mode)
{
	char	recsetid_cuid[CUID_LEN];
	struct zbx_json	details_json;
	DB_RESULT	result;
	DB_ROW			row;

	result = DBselect("name from hosts where hostid=" ZBX_FS_UI64, hostid);

	if (NULL == result)
	{
		return;
	}

	while (NULL != (row = DBfetch(result)))
	{
		zbx_new_cuid(recsetid_cuid);

		zbx_json_init(&details_json, ZBX_JSON_STAT_BUF_LEN);
		zbx_json_addobject(&details_json, NULL);
		zbx_json_adduint64(&details_json, "inventory_mode", inventory_mode);
		zbx_json_close(&details_json);

		zbx_audit_create_entry(AUDIT_ACTION_UPDATE, hostid, row[0],
				AUDIT_RESOURCE_HOST, recsetid_cuid, details_json.buffer);

		zbx_json_free(&details_json);
	}
	DBfree_result(result);
}
