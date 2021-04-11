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


void	zbx_items_audit_init(void);

void	zbx_items_persist(char *recsetid_cuid);

int	zbx_audit_create_entry(int action, zbx_uint64_t resourceid, char* resourcename, int resourcetype,
		char *recsetid, char *details);

int	zbx_items_audit_create_entry(zbx_template_item_t *item, zbx_uint64_t hostid, int action);
void	zbx_items_audit_update_json_string(zbx_uint64_t itemid, char *key, char *value);
void	zbx_items_audit_update_json_uint64(zbx_uint64_t itemid, char *key, uint64_t value);

#endif	/* ZABBIX_AUDIT_H */
