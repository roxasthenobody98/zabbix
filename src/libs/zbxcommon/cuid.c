#include "string.h"
#include "sysinfo.h"
#include <time.h>
#include <stdlib.h>

#ifdef HAVE_SYS_UTSNAME_H
#	include <sys/utsname.h>
#endif

#include "common.h"

#define CUID_PID_BLOCK_SIZE		2
#define CUID_HOSTNAME_BLOCK_SIZE	2
#define CUID_BLOCK_SIZE			4
#define CUID_BASE_36			36
#define DISCRETE_VALUES			1679616
#define CUID_TIMESTAMP_SIZE		8
#define PID_TMP_36_BASE_BUF_LEN		10
#define HOST_TMP_36_BASE_BUF_LEN	10

static size_t	counterValue;

static size_t	next(void)
{
	size_t	ret;

	ret = counterValue;
	counterValue++;

	if (counterValue > DISCRETE_VALUES)
		counterValue = 0;

	return ret;
}

static void	strev(char *str)
{
	size_t	len, i;
	char	temp;

	len = strlen(str);

	for (i = 0; i < len/2; i++)
	{
		temp = str[i];
		str[i] = str[len - i - 1];
		str[len - i - 1] = temp;
	}
}

static char	re_val(size_t num)
{
	if (num <= 9)
		return (char)(num + '0');
	else
		return (char)(num - 10 + 'a');
}

static void	from_deci(char res[], size_t base, size_t inputNum)
{
	size_t	index = 0;

	if (0 == inputNum)
	{
		res[0] = '0';
		res[1] = '\0';
	}

	while (0 < inputNum)
	{
		res[index++] = re_val(inputNum % base);
		inputNum /= base;
	}
	res[index] = '\0';

	strev(res);
}

static void	pad(char *input, size_t pad_size, char *output)
{
	size_t	i, input_len;

	input_len = strlen(input);

	if (pad_size > input_len)
	{
		memset(output, '0', pad_size);

		for (i = 0; i < input_len; i++)
			output[i + pad_size - input_len] = input[i];
	}
	else
	{
		for (i = 0; i < pad_size; i++)
			output[i] = input[i + input_len - pad_size];
	}

	output[pad_size] = '\0';
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_cuid_init                                                    *
 *                                                                            *
 * Purpose: initializes context for the cuid generation                       *
 *                                                                            *
 ******************************************************************************/
void	zbx_cuid_init(void)
{
	counterValue = 0;
	srand((unsigned int)time(NULL) + getpid());
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_new_cuid                                                     *
 *                                                                            *
 * Purpose: generates cuid, is based on the go cuid implementation from       *
 *          https://github.com/lucsky/cuid/blob/master/cuid.go                *
 *          consider using mutexes around it if used inside threads           *
 *                                                                            *
 * Parameters: cuid      - [OUT] resulting cuid                               *
 *                                                                            *
 * Return value: SUCCEED - if cuid was generated successfully                 *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
int	zbx_new_cuid(char cuid[CUID_LEN])
{
	size_t	pid, seconds, hostname_num, hostname_len, i;
	char	rand_block_1[CUID_BLOCK_SIZE + 1], rand_block_2[CUID_BLOCK_SIZE + 1], fingerprint[CUID_BLOCK_SIZE + 1],
		timestamp[CUID_TIMESTAMP_SIZE + 1], counter_tmp[CUID_BLOCK_SIZE + 1], counter[CUID_BLOCK_SIZE+1],
		rand_block_1_tmp[CUID_BLOCK_SIZE + 1], rand_block_2_tmp[CUID_BLOCK_SIZE + 1],
		pid_block_tmp[PID_TMP_36_BASE_BUF_LEN], host_block_tmp[HOST_TMP_36_BASE_BUF_LEN],
		pid_block[CUID_PID_BLOCK_SIZE + 1], host_block[CUID_HOSTNAME_BLOCK_SIZE + 1];
	char	*hostname = NULL;
	struct utsname	name;

	pid = (size_t)getpid();

	if (-1 == uname(&name))
	{
		zbx_error("failed to call uname");
		return FAIL;
	}

	hostname = zbx_strdup(NULL, name.nodename);
	hostname_len = strlen(hostname);
	hostname_num = hostname_len + CUID_BASE_36;

	for (i = 0; i < hostname_len; i++)
		hostname_num = hostname_num + (size_t)hostname[i];

	from_deci(host_block_tmp, 10, hostname_num);
	from_deci(pid_block_tmp, CUID_BASE_36, pid);
	pad(pid_block_tmp, CUID_PID_BLOCK_SIZE, pid_block);
	pad(host_block_tmp, CUID_HOSTNAME_BLOCK_SIZE, host_block);
	zbx_snprintf(fingerprint, sizeof(fingerprint), "%s%s", host_block, pid_block);
	seconds = (size_t)time(NULL);
	from_deci(timestamp, CUID_BASE_36, seconds * 1000);
	from_deci(counter_tmp, CUID_BASE_36, next());
	pad(counter_tmp, CUID_BLOCK_SIZE, counter);
	zbx_snprintf(rand_block_1_tmp, sizeof(rand_block_1_tmp), "%lx", (size_t)rand() & 0xffff);
	pad(rand_block_1_tmp, CUID_BLOCK_SIZE, rand_block_1);
	zbx_snprintf(rand_block_2_tmp, sizeof(rand_block_2_tmp), "%lx", (size_t)rand() & 0xffff);
	pad(rand_block_2_tmp, CUID_BLOCK_SIZE, rand_block_2);
	zbx_snprintf(cuid, CUID_LEN, "c%s%s%s%s%s", timestamp, counter, fingerprint, rand_block_1, rand_block_2);
	zbx_free(hostname);

	return SUCCEED;
}
