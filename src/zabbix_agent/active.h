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

#ifndef ZABBIX_ACTIVE_H
#define ZABBIX_ACTIVE_H

#include "threads.h"
#include "zbxalgo.h"

extern char	*CONFIG_SOURCE_IP;
extern char	*CONFIG_HOSTNAME;
extern char	*CONFIG_HOST_METADATA;
extern char	*CONFIG_HOST_METADATA_ITEM;
extern char	*CONFIG_HOST_INTERFACE;
extern char	*CONFIG_HOST_INTERFACE_ITEM;
extern int	CONFIG_REFRESH_ACTIVE_CHECKS;
extern int	CONFIG_BUFFER_SEND;
extern int	CONFIG_BUFFER_SIZE;
extern int	CONFIG_MAX_LINES_PER_SECOND;
extern char	*CONFIG_LISTEN_IP;
extern int	CONFIG_LISTEN_PORT;

#define HOST_METADATA_LEN	255	/* UTF-8 characters, not bytes */
#define HOST_INTERFACE_LEN	255	/* UTF-8 characters, not bytes */

typedef struct
{
	char		*host;
	unsigned short	port;
}
ZBX_THREAD_ACTIVECHK_ARGS;

typedef struct
{
	char		*host;
	char		*key;
	char		*value;
	unsigned char	state;
	zbx_uint64_t	lastlogsize;
	int		timestamp;
	char		*source;
	int		severity;
	zbx_timespec_t	ts;
	int		logeventid;
	int		mtime;
	unsigned char	flags;
	zbx_uint64_t	id;
}
ZBX_ACTIVE_BUFFER_ELEMENT;

typedef struct
{
	ZBX_ACTIVE_BUFFER_ELEMENT	*data;
	int				count;
	int				pcount;
	int				lastsent;
	int				first_error;
}
ZBX_ACTIVE_BUFFER;

ZBX_THREAD_ENTRY(active_checks_thread, args);

#define MAX_PART_FOR_MD5	512	/* maximum size of the part of the file to calculate MD5 sum for */

typedef struct
{
	char		*key_orig;
	char		*persistent_file_name;
	/* data for writing into persistent file */
	char		*filename;
	int		mtime;
	int		md5size;
	int		last_rec_size;
	int		seq;
	int		incomplete;
	int		copy_of;
	zbx_uint64_t	dev;
	zbx_uint64_t	ino_lo;
	zbx_uint64_t	ino_hi;
	zbx_uint64_t	size;
	zbx_uint64_t	processed_size;
	md5_byte_t	md5buf[MD5_DIGEST_SIZE];
	char		last_rec_part[MAX_PART_FOR_MD5];	/* Keep a copy of up to the first 512 bytes of */
								/* the last record to calculate md5 sum. It could */
								/* be a not null-terminated string! */
}
zbx_pre_persistent_t;

int	zbx_pre_persistent_compare_func(const void *d1, const void *d2);

ZBX_VECTOR_DECL(pre_persistent, zbx_pre_persistent_t)

int	zbx_find_or_create_prep_vec_element(zbx_vector_pre_persistent_t *prep_vec, const char *key,
		const char *persistent_file_name);
#endif	/* ZABBIX_ACTIVE_H */
