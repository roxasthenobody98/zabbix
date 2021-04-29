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
#include "../zbxdbhigh/template.h"

void	zbx_audit_init(void);
void	zbx_audit_flush(const char *recsetid_cuid);
void	DBselect_delete_for_item(const char *sql, zbx_vector_uint64_t *ids, int resource_type_type);
void	DBselect_delete_for_graph(const char *sql, zbx_vector_uint64_t *ids);
char	*zbx_audit_items_get_type_json_identifier(int flag);
void	zbx_audit_items_create_entry(const zbx_template_item_t *item, const zbx_uint64_t hostid,
		const int action);
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
void	zbx_audit_triggers_update_dependencies(const char *triggerid_up_str, const char *triggerid_str,
		const char *flags_str, const char *triggerdepid_str);
void	zbx_audit_triggers_update_tags_and_values(zbx_uint64_t triggerid, const char *tag, const char *value,
		const char *flags_str, const char *tagid_str);
void	zbx_audit_httptests_create_entry_add(zbx_uint64_t httptestid, char *name, char *delay,
		unsigned char status, char *agent, unsigned char authentication, char *http_user, char *http_password,
		char *http_proxy, int retries, uint64_t hostid, zbx_uint64_t templateid);
void	zbx_audit_httptests_update_headers_and_variables(int type, zbx_uint64_t httpstepid, zbx_uint64_t httptestid,
		const char *name, const char *value);
void	zbx_audit_httptests_steps_update(zbx_uint64_t httpstepid, zbx_uint64_t httptestid, int no,
		const char *name, const char *url, const char *timeout, const char *posts, const char *required,
		const char *status_codes, zbx_uint64_t follow_redirects, zbx_uint64_t retrieve_mode);
void	zbx_audit_httptests_steps_update_extra(int type, zbx_uint64_t httpstepid, zbx_uint64_t httptestid,
		const char *name, const char *value);
void	zbx_audit_httptests_create_entry_update(zbx_uint64_t httptestid, char *name, zbx_uint64_t templateid);
void	zbx_audit_create_entry_for_delete(zbx_uint64_t id, char *name, int resource_type);
void	zbx_audit_update_json_string(const zbx_uint64_t itemid, const char *key, const char *value);
void	zbx_audit_update_json_uint64(const zbx_uint64_t itemid, const char *key, const uint64_t value);
int	zbx_audit_create_entry(const int action, const zbx_uint64_t resourceid, const char* resourcename,
		const int resourcetype, const char *recsetid, const char *details);
void	zbx_audit_groups_add(zbx_uint64_t hostid, zbx_uint64_t hostgroupid, zbx_uint64_t groupid);
void	zbx_audit_groups_delete(zbx_uint64_t hostid);
void	zbx_audit_host_add(zbx_uint64_t hostid, char *recsetid_cuid);
void	zbx_audit_host_del(zbx_uint64_t hostid);
void	zbx_audit_host_status(zbx_uint64_t hostid, int status);
void	zbx_audit_host_inventory(zbx_uint64_t hostid, int inventory_mode);

#endif	/* ZABBIX_AUDIT_H */
