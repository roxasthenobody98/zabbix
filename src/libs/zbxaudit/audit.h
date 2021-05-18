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

#ifndef ZABBIX_AUDIT_H
#define ZABBIX_AUDIT_H

#include "common.h"
#include "zbxalgo.h"
#include "db.h"
#include "../zbxdbhigh/template.h"

typedef struct
{
	zbx_uint64_t		itemid;
	char			*delay;
	zbx_uint64_t		hostid;
	zbx_uint64_t		interfaceid;
	char			*key;
	char			*name;
	zbx_uint64_t		type;
	char			*url;
	zbx_uint64_t		value_type;
	zbx_uint64_t		allow_traps;
	zbx_uint64_t		authtype;
	char			*description;
	zbx_uint64_t		flags;
	zbx_uint64_t		follow_redirects;
	char			*headers;
	char			*history;
	char			*http_proxy;
	char			*ipmi_sensor;
	char			*jmx_endpoint;
	char			*logtimefmt;
	zbx_uint64_t		master_itemid;
	zbx_uint64_t		output_format;
	char			*params;
	char			*password;
	zbx_uint64_t		post_type;
	char			*posts;
	char			*privatekey;
	char			*publickey;
	char			*query_fields;
	zbx_uint64_t		request_method;
	zbx_uint64_t		retrieve_mode;
	char			*snmp_oid;
	char			*ssl_cert_file;
	char			*ssl_key_file;
	char			*ssl_key_password;
	zbx_uint64_t		status;
	char			*status_codes;
	char			*timeout;
	char			*trapper_hosts;
	char			*trends;
	char			*units;
	char			*username;
	zbx_uint64_t		valuemapid;
	zbx_uint64_t		verify_host;
	zbx_uint64_t		verify_peer;
	char			*formula;
}
zbx_audit_item_t;

void	zbx_audit_init(void);
void	zbx_audit_flush(void);
void	DBselect_delete_for_item(const char *sql, zbx_vector_uint64_t *ids);
void	DBselect_delete_for_http_test(const char *sql, zbx_vector_uint64_t *ids);
void	DBselect_delete_for_trigger(const char *sql, zbx_vector_uint64_t *ids);
void	DBselect_delete_for_graph(const char *sql, zbx_vector_uint64_t *ids);
const char	*zbx_audit_items_get_type_json_identifier(int flag);
void	zbx_audit_items_create_entry(const zbx_template_item_t *item, const zbx_uint64_t hostid,
		const int action);
void	zbx_audit_host_update_parent_template(const char *audit_details_action, zbx_uint64_t hostid,
		zbx_uint64_t templateid);
void	zbx_audit_host_delete_parent_templates(zbx_uint64_t hostid, const char *hostname,
		zbx_vector_uint64_t *del_templateids);
void	zbx_audit_host_prototypes_create_entry(const int audit_action, zbx_uint64_t hostid, char *name,
		unsigned char status, zbx_uint64_t templateid, unsigned char discover, unsigned char custom_interfaces);
void	zbx_audit_host_prototypes_update_details(zbx_uint64_t hostid, const char *name, zbx_uint64_t groupid,
		zbx_uint64_t templateid);
void	zbx_audit_graphs_create_entry(const int audit_action, zbx_uint64_t hst_graphid, const char *name, int width,
		int height, double yaxismin, double yaxismax, zbx_uint64_t graphid, unsigned char show_work_period,
		unsigned char show_triggers, unsigned char graphtype, unsigned char show_legend, unsigned char show_3d,
		double percent_left, double percent_right, unsigned char ymin_type, unsigned char ymax_type,
		zbx_uint64_t ymin_itemid, zbx_uint64_t ymax_itemid, unsigned char flags, unsigned char discover);
void	zbx_audit_graphs_update_gitems(zbx_uint64_t hst_graphid, int flags, zbx_uint64_t gitemid, int drawtype,
		int sortorder, const char *color, int yaxisside, int calc_fnc, int type);
void	zbx_audit_triggers_create_entry(const int audit_action, zbx_uint64_t new_triggerid, const char *description,
		zbx_uint64_t templateid, unsigned char recovery_mode, unsigned char status, unsigned char type,
		zbx_uint64_t value, zbx_uint64_t state, unsigned char priority, const char *comments, const char *url,
		unsigned char flags, unsigned char correlation_mode, const char *correlation_tag,
		unsigned char manual_close, const char *opdata, unsigned char discover, const char *event_name);
void	zbx_audit_triggers_update_expression_and_recovery_expression(zbx_uint64_t triggerid, int flags,
		const char *new_expression, const char *new_recovery_expression);
void	zbx_audit_triggers_update_dependencies(zbx_uint64_t triggerid_up, zbx_uint64_t triggerid,
		int flags, zbx_uint64_t triggerdepid);
void	zbx_audit_triggers_update_tags_and_values(zbx_uint64_t triggerid, const char *tag, const char *value,
		int flags, const char *tagid_str);
void	zbx_audit_httptests_create_entry_add(zbx_uint64_t httptestid, char *name, char *delay,
		unsigned char status, char *agent, unsigned char authentication, char *http_user, char *http_password,
		char *http_proxy, int retries, uint64_t hostid, zbx_uint64_t templateid);
void	zbx_audit_httptests_update_headers_and_variables(int type, zbx_uint64_t httpstepid, zbx_uint64_t httptestid,
		const char *name, const char *value);
void	zbx_audit_httptests_steps_update(zbx_uint64_t httpstepid, zbx_uint64_t httptestid, int no,
		const char *name, const char *url, const char *timeout, const char *posts, const char *required,
		const char *status_codes, zbx_uint64_t follow_redirects, zbx_uint64_t retrieve_mode);
void	zbx_audit_httptests_steps_update_extra(int type, zbx_uint64_t httpstepid, int field_no, zbx_uint64_t httptestid,
		const char *name, const char *value);
void	zbx_audit_httptests_create_entry_update(zbx_uint64_t httptestid, char *name, zbx_uint64_t templateid);
void	zbx_audit_discovery_rule_overrides_update(int item_no, int rule_condition_no, zbx_uint64_t itemid,
		zbx_uint64_t op, const char *macro, const char *value);
void	zbx_audit_discovery_rule_override_conditions_update(int audit_index, zbx_uint64_t itemid, zbx_uint64_t op,
		const char *macro, const char *value);
void	zbx_audit_preprocessing_update(zbx_uint64_t itemid, unsigned char flags, int step,
		int type, const char *params, int error_handler, const char *error_handler_params);
void	zbx_audit_item_parameters_update(int audit_index, zbx_uint64_t itemid, const char *name,
		const char *value, int flags);
void	zbx_audit_discovery_rule_lld_macro_paths_update(zbx_uint64_t no, zbx_uint64_t itemid, const char *lld_macro,
		const char *path);
void	zbx_audit_discovery_rule_overrides_operations_update(int override_no, int operation_no, zbx_uint64_t itemid,
		zbx_uint64_t operation_type, zbx_uint64_t operator, const char *value);
void	zbx_audit_discovery_rule_overrides_operations_update_extra(int override_no, int operation_no,
		zbx_lld_override_operation_t *override_operation, zbx_uint64_t itemid);
void	zbx_audit_discovery_rule_overrides_operations_optag_update(int override_no, int override_operation_no,
		int override_operation_tag_no, zbx_uint64_t itemid, const char *tag, const char *value);
void	zbx_audit_discovery_rule_overrides_operations_optemplate_update(int override_no, int override_operation_no,
		int override_operation_tag_no, zbx_uint64_t itemid, zbx_uint64_t templateid);
void	zbx_audit_discovery_rule_overrides_operations_opinventory_update(int override_no, int override_operation_no,
		zbx_uint64_t itemid, zbx_uint64_t inventory_mode);
void	zbx_audit_host_update_interfaces(zbx_uint64_t hostid, zbx_uint64_t interfaceid, zbx_uint64_t main_,
		zbx_uint64_t type, zbx_uint64_t useip, const char *ip, const char *dns, zbx_uint64_t port);
void	zbx_audit_host_update_snmp_interfaces(zbx_uint64_t hostid, zbx_uint64_t version, zbx_uint64_t bulk,
		const char *community, const char *securityname, zbx_uint64_t securitylevel, const char *authpassphrase,
		const char *privpassphrase, zbx_uint64_t authprotocol, zbx_uint64_t privprotocol,
		const char *contextname, zbx_uint64_t interfaceid);
void	zbx_audit_update_json_string(const zbx_uint64_t itemid, const char *key, const char *value);
void	zbx_audit_update_json_uint64(const zbx_uint64_t itemid, const char *key, const uint64_t value);
int	zbx_audit_create_entry(const int action, const zbx_uint64_t resourceid, const char* resourcename,
		const int resourcetype, const char *recsetid, const char *details);
void	zbx_audit_host_update_groups(const char *audit_details_action, zbx_uint64_t hostid, zbx_uint64_t groupid);
void	zbx_audit_host_groups_delete_create_entry(zbx_uint64_t hostid, char *hostname, zbx_vector_uint64_t *groupids);
void	zbx_audit_host_update_tls_and_psk(zbx_uint64_t hostid, int tls_connect, int tls_accept, const char *psk_identity,
		const char *psk);
void	zbx_audit_host_create_entry(int audit_action, zbx_uint64_t hostid, const char *name);
void	zbx_audit_lld_host_create_entry(int audit_action, zbx_uint64_t hostid, char *host, char *name,
		zbx_uint64_t proxy_hostid, char ipmi_authtype, unsigned char ipmi_privilege, const char *ipmi_username,
		const char *ipmi_password, unsigned char status, zbx_uint64_t flags, unsigned char tls_connect,
		unsigned char tls_accept, const char *tls_issuer, const char *tls_subject,
		const char *tls_psk_identity, const char *tls_psk);
void	zbx_audit_host_del(zbx_uint64_t hostid, const char *hostname);
void	zbx_audit_lld_items_create_entry(const zbx_audit_item_t *item, const zbx_uint64_t hostid,
		const int action);
void	zbx_audit_item_tags_update(int audit_index, zbx_uint64_t itemid, const char *tag, const char *value);
#endif	/* ZABBIX_AUDIT_H */
