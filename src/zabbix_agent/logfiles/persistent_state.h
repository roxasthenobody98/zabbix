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

char	*make_persistent_server_directory_name(const char *base_path, const char *server, unsigned short port);
char	*create_persistent_server_directory(const char *base_path, const char *host, unsigned short port, char **error);
char	*make_persistent_file_name(const char *persistent_server_dir, const char *item_key);
int	write_persistent_file(const char *filename, const char *data, char **error);
int	remove_persistent_file(const char *pathname, char **error);
