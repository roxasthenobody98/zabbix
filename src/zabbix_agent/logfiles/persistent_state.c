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
#include "md5.h"
#include "log.h"
#include "persistent_state.h"

#if !defined(_WINDOWS)
/******************************************************************************
 *                                                                            *
 * Function: str2file_name_part                                               *
 *                                                                            *
 * Purpose: for the specified string get the part of persistent storage path  *
 *                                                                            *
 * Parameters:                                                                *
 *          str - [IN] string (e.g. host, item key)                           *
 *                                                                            *
 * Return value: dynamically allocated string with the part of path           *
 *               corresponding to the input string                            *
 *                                                                            *
 * Comments: the part of persistent storage path is built of md5 sum of the   *
 *           input string with the length of input string appended            *
 *                                                                            *
 * Example: for input string                                                  *
 *           "logrt[/home/zabbix/test.log,,,,,,,,/home/zabbix/agent_private]" *
 *          the function returns                                              *
 *           "147bd30c08995022fbe78de3c26c082962"                             *
 *                                            \/ - length of input string     *
 *            \------------------------------/   - md5 sum of input string    *
 *                                                                            *
 ******************************************************************************/
static char	*str2file_name_part(const char *str)
{
	md5_state_t	state;
	md5_byte_t	md5[MD5_DIGEST_SIZE];
	char		size_buf[21];		/* 20 - max size of printed 'size_t' value, 1 - '\0' */
	char		*md5_text = NULL;
	size_t		str_sz, md5_text_sz, size_buf_len;

	str_sz = strlen(str);

	zbx_md5_init(&state);
	zbx_md5_append(&state, (const md5_byte_t *)str, (int)str_sz);
	zbx_md5_finish(&state, md5);

	size_buf_len = zbx_snprintf(size_buf, sizeof(size_buf), ZBX_FS_SIZE_T, (zbx_fs_size_t)str_sz);

	md5_text_sz = MD5_DIGEST_SIZE * 2 + size_buf_len + 1;
	md5_text = (char *)zbx_malloc(NULL, md5_text_sz);

	md5buf2str(md5, md5_text);
	memcpy(md5_text + MD5_DIGEST_SIZE * 2, size_buf, size_buf_len + 1);

	return md5_text;
}

/******************************************************************************
 *                                                                            *
 * Function: active_server2dir_name_part                                      *
 *                                                                            *
 * Purpose: calculate the part of persistent storage path for the specified   *
 *          server/port pair where the agent is sending active check data     *
 *                                                                            *
 * Parameters:                                                                *
 *          server - [IN] server string                                       *
 *          port   - [IN] port number                                         *
 *                                                                            *
 * Return value: dynamically allocated string with the part of path           *
 *                                                                            *
 * Comments: the part of persistent storage path is built of md5 sum of the   *
 *           input string with the length of input string appended            *
 *                                                                            *
 * Example: for server "127.0.0.1" and port 10091 the function returns        *
 *         "8392b6ea687188e70feac24517d2f9d715"                               *
 *                                          \/ - length  of "127.0.0.1:10091" *
 *          \------------------------------/   - md5 sum of "127.0.0.1:10091" *
 *                                                                            *
 ******************************************************************************/
static char	*active_server2dir_name_part(const char *server, unsigned short port)
{
	char	*endpoint, *dir_name;

	endpoint = zbx_dsprintf(NULL, "%s:%hu", server, port);
	dir_name = str2file_name_part(endpoint);
	zbx_free(endpoint);

	return dir_name;
}

/******************************************************************************
 *                                                                            *
 * Function: make_persistent_server_directory_name                            *
 *                                                                            *
 * Purpose: make the name of persistent storage directory for the specified   *
 *          server/proxy and port                                             *
 *                                                                            *
 * Parameters:                                                                *
 *          base_path - [IN] initial part of pathname                         *
 *          server    - [IN] server string                                    *
 *          port      - [IN] port number                                      *
 *                                                                            *
 * Return value: dynamically allocated string with the name of directory      *
 *                                                                            *
 * Example: for base_path "/var/tmp/", server "127.0.0.1", port 10091 the     *
 *          function returns "/var/tmp/8392b6ea687188e70feac24517d2f9d715"    *
 *                                                                            *
 ******************************************************************************/
char	*make_persistent_server_directory_name(const char *base_path, const char *server, unsigned short port)
{
	char	*server_part, *file_name;

	server_part = active_server2dir_name_part(server, port);
	file_name = zbx_dsprintf(NULL, "%s/%s", base_path, server_part);
	zbx_free(server_part);

	return file_name;
}

/******************************************************************************
 *                                                                            *
 * Function: check_persistent_directory_exists                                *
 *                                                                            *
 * Purpose: check if the directory exists                                     *
 *                                                                            *
 * Parameters:                                                                *
 *          pathname  - [IN] full pathname of directory                       *
 *          error     - [OUT] error message                                   *
 *                                                                            *
 * Return value: SUCCEED or FAIL (with dynamically allocated 'error')         *
 *                                                                            *
 ******************************************************************************/
static int	check_persistent_directory_exists(const char *pathname, char **error)
{
	zbx_stat_t	status;

	if (0 != lstat(pathname, &status))
	{
		*error = zbx_dsprintf(*error, "cannot obtain directory information: %s", zbx_strerror(errno));
		return FAIL;
	}

	if (0 == S_ISDIR(status.st_mode))
	{
		*error = zbx_dsprintf(*error, "file exists but is not a directory");
		return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Function: create_persistent_directory                                      *
 *                                                                            *
 * Purpose: create directory if it does not exist or check access if it       *
 *          exists                                                            *
 *                                                                            *
 * Parameters:                                                                *
 *          pathname  - [IN] full pathname of directory                       *
 *          error     - [OUT] error message                                   *
 *                                                                            *
 * Return value: SUCCEED - directory created or already exists                *
 *               FAIL    - cannot create directory or it has insufficient     *
 *                         access rights (see dynamically allocated 'error')  *
 *                                                                            *
 ******************************************************************************/
static int	create_persistent_directory(const char *pathname, char **error)
{
	if (0 != mkdir(pathname, S_IRWXU))
	{
		if (EEXIST == errno)
			return check_persistent_directory_exists(pathname, error);

		*error = zbx_dsprintf(*error, "%s(): cannot create directory \"%s\": %s", __func__, pathname,
				zbx_strerror(errno));
		return FAIL;
	}

	zabbix_log(LOG_LEVEL_DEBUG, "%s(): created directory:[%s]", __func__, pathname);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Function: create_base_path_directories                                     *
 *                                                                            *
 * Purpose: create all subdirectories in the pathname if they do not exist    *
 *                                                                            *
 * Parameters:                                                                *
 *          pathname - [IN] full pathname of directory, must consist of only  *
 *                          ASCII characters                                  *
 *          error    - [OUT] error message                                    *
 *                                                                            *
 * Return value: SUCCEED - directories created or already exist               *
 *               FAIL    - cannot create directory or it has insufficient     *
 *                         access rights (see dynamically allocated 'error')  *
 *                                                                            *
 ******************************************************************************/
static int	create_base_path_directories(const char *pathname, char **error)
{
	char	*path, *p;

	zabbix_log(LOG_LEVEL_DEBUG, "%s(): pathname:[%s]", __func__, pathname);

	if ('/' != pathname[0])
	{
		*error = zbx_dsprintf(*error, "persistent directory name is not an absolute path,"
				" it does not start with '/'");
		return FAIL;
	}

	path = zbx_strdup(NULL, pathname);	/* mutable copy */

	for (p = path + 1; ; ++p)
	{
		if ('/' == *p)
		{
			*p = '\0';

			zabbix_log(LOG_LEVEL_DEBUG, "%s(): checking directory:[%s]", __func__, path);

			if (SUCCEED != create_persistent_directory(path, error))
			{
				zbx_free(path);
				return FAIL;
			}

			*p = '/';
		}
		else if ('\0' == *p)
		{
			zabbix_log(LOG_LEVEL_DEBUG, "%s(): checking directory:[%s]", __func__, path);

			if (SUCCEED != create_persistent_directory(path, error))
			{
				zbx_free(path);
				return FAIL;
			}

			break;
		}
	}

	zbx_free(path);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Function: create_persistent_server_directory                               *
 *                                                                            *
 * Purpose: create directory if it does not exist or check access if it       *
 *          exists. Directory name is derived from host and port.             *
 *                                                                            *
 * Parameters:                                                                *
 *          base_path - [IN] initial part of pathname, must consist of only   *
 *                           ASCII characters                                 *
 *          host      - [IN] host string                                      *
 *          port      - [IN] port number                                      *
 *          error     - [OUT] error message                                   *
 *                                                                            *
 * Return value: on success - dynamically allocated string with the name of   *
 *                            directory,                                      *
 *               on error   - NULL (with dynamically allocated 'error')       *
 *                                                                            *
 ******************************************************************************/
char	*create_persistent_server_directory(const char *base_path, const char *host, unsigned short port, char **error)
{
	char	*server_dir_name, *err_msg = NULL;

	/* persistent storage top-level directory */
	if (SUCCEED != create_base_path_directories(base_path, error))
		return NULL;

	/* directory for server/proxy where the current active check process will be sending data */
	server_dir_name = make_persistent_server_directory_name(base_path, host, port);

	if (SUCCEED != create_persistent_directory(server_dir_name, &err_msg))
	{
		*error = zbx_dsprintf(*error, "cannot create directory \"%s\": %s", server_dir_name, err_msg);
		zbx_free(err_msg);
		return NULL;
	}

	if (0 != access(server_dir_name, R_OK | W_OK | X_OK))
	{
		*error = zbx_dsprintf(*error, "insufficient access rights to directory \"%s\"", server_dir_name);
		return NULL;
	}

	return server_dir_name;
}

/******************************************************************************
 *                                                                            *
 * Function: make_persistent_file_name                                        *
 *                                                                            *
 * Purpose: make the name of persistent storage directory or file             *
 *                                                                            *
 * Parameters:                                                                *
 *          persistent_server_dir - [IN] initial part of pathname             *
 *          item_key              - [IN] item key (not NULL)                  *
 *                                                                            *
 * Return value: dynamically allocated string with the name of file           *
 *                                                                            *
 ******************************************************************************/
char	*make_persistent_file_name(const char *persistent_server_dir, const char *item_key)
{
	char	*file_name, *item_part;

	item_part = str2file_name_part(item_key);
	file_name = zbx_dsprintf(NULL, "%s/%s", persistent_server_dir, item_part);
	zbx_free(item_part);

	return file_name;
}

/******************************************************************************
 *                                                                            *
 * Function: write_persistent_file                                            *
 *                                                                            *
 * Purpose: write metric info into persistent file                            *
 *                                                                            *
 * Parameters:                                                                *
 *          filename  - [IN] file name                                        *
 *          data      - [IN] file content                                     *
 *          error     - [OUT] error message                                   *
 *                                                                            *
 * Return value: SUCCEED - file was successfully written                      *
 *               FAIL    - cannot write the file (see dynamically allocated   *
 *                         'error' message)                                   *
 *                                                                            *
 ******************************************************************************/
int	write_persistent_file(const char *filename, const char *data, char **error)
{
	FILE	*fp;
	size_t	sz;

	if (NULL == (fp = fopen(filename, "w")))
	{
		*error = zbx_dsprintf(*error, "cannot open file: %s", zbx_strerror(errno));
		return FAIL;
	}

	sz = strlen(data);

	if (sz != fwrite(data, 1, sz, fp))
	{
		*error = zbx_dsprintf(*error, "cannot write to file: %s", zbx_strerror(errno));
		fclose(fp);
		return FAIL;
	}

	fclose(fp);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Function: remove_persistent_file                                           *
 *                                                                            *
 * Purpose: remove the specified file                                         *
 *                                                                            *
 * Parameters:                                                                *
 *          pathname  - [IN] file name                                        *
 *          error     - [OUT] error message                                   *
 *                                                                            *
 * Return value: SUCCEED - file was removed or does not exist                 *
 *               FAIL    - cannot remove the specified file (see dynamically  *
 *                         allocated 'error' message)                         *
 *                                                                            *
 ******************************************************************************/
int remove_persistent_file(const char *pathname, char **error)
{
	if (0 == unlink(pathname) || ENOENT == errno)
		return SUCCEED;

	*error = zbx_strdup(*error, zbx_strerror(errno));

	return FAIL;
}
#endif	/* not WINDOWS */
