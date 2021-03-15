#include "string.h"
#include "stdio.h"
#include <time.h>
#include <math.h>
#include "stdint.h"
#include <stdlib.h>

#include "sysinfo.h"
#ifdef HAVE_SYS_UTSNAME_H
#	include <sys/utsname.h>
#endif

#define CUID_PID_BLOCK_SIZE	2
#define CUID_HOSTNAME_BLOCK_SIZE	2

#define CUID_BLOCK_SIZE	4
#define CUID_BASE_36	36
#define DISCRETE_VALUES	1679616

#define CUID_TIMESTAMP_SIZE	8
#define PID_TMP_36_BASE_BUF_LEN 10
#define HOST_TMP_36_BASE_BUF_LEN 10

static int32_t	counterValue;

void	cuid_init(void)
{
	counterValue = 0;
	srand(time(NULL));
}

static int32_t	next(void)
{
	int32_t	ret;

	ret = counterValue;
	counterValue++;

	if (counterValue > DISCRETE_VALUES)
		counterValue = 0;

	return ret;
}

static void	strev(char *str)
{
	int	len, i;
	char	temp;

	len = strlen(str);

	for (i = 0; i < len/2; i++)
	{
		temp = str[i];
		str[i] = str[len - i - 1];
		str[len - i - 1] = temp;
	}
}

static char	reVal(int num)
{
	if (num >= 0 && num <= 9)
		return (char)(num + '0');
	else
		return (char)(num - 10 + 'a');
}

static void	fromDeci(char res[], int base, size_t inputNum)
{
	int	index = 0;

	if (inputNum == 0)
	{
		res[0] = '0';
		res[1] = '\0';
	}

	while (inputNum > 0)
	{
		res[index++] = reVal(inputNum % base);
		inputNum /= base;
	}
	res[index] = '\0';

	strev(res);
}

static void	pad(char *input, size_t pad_size, char *output)
{
	size_t	i, offset, input_len;

	input_len = strlen(input);
	offset = pad_size > input_len ? pad_size - input_len : 0;
	memset(output, '0', pad_size);

	for (i = 0; i < strlen(input);i++)
		output[i + offset] = input[i];

	output[pad_size] = '\0';
}

int	new_cuid(char cuid[CUID_LEN])
{
	int	pid;
	char	rand_block_1[CUID_BLOCK_SIZE + 1], rand_block_2[CUID_BLOCK_SIZE + 1], fingerprint[CUID_BLOCK_SIZE + 1],
		timestamp[CUID_TIMESTAMP_SIZE + 1], counter_tmp[CUID_BLOCK_SIZE + 1], counter[CUID_BLOCK_SIZE+1],
		rand_block_1_tmp[CUID_BLOCK_SIZE + 1], rand_block_2_tmp[CUID_BLOCK_SIZE + 1],
		pid_block_tmp[PID_TMP_36_BASE_BUF_LEN], host_block_tmp[HOST_TMP_36_BASE_BUF_LEN],
		pid_block[CUID_PID_BLOCK_SIZE + 1], host_block[CUID_HOSTNAME_BLOCK_SIZE + 1];
	char	*hostname = NULL;
	time_t	seconds;
	struct utsname	name;

	pid = (int)getpid();

	if (-1 == uname(&name))
	{
		zbx_error("failed to call uname");
		return FAIL;
	}

	hostname = zbx_strdup(NULL, name.nodename);

	{
		size_t	hostname_num, hostname_len, i;

		hostname_len = strlen(hostname);
		hostname_num = hostname_len + CUID_BASE_36;

		for (i = 0; i < hostname_len; i++)
			hostname_num = hostname_num + hostname[i];

		fromDeci(host_block_tmp, 10, hostname_num);
		fromDeci(pid_block_tmp, CUID_BASE_36, pid );
		pad(pid_block_tmp, CUID_PID_BLOCK_SIZE, pid_block);
		pad(host_block_tmp, CUID_HOSTNAME_BLOCK_SIZE, host_block);
		zbx_snprintf(fingerprint, sizeof(fingerprint), "%s%s", host_block, pid_block);
	}

	seconds = time(NULL);
	fromDeci(timestamp, CUID_BASE_36, seconds * 1000);
	fromDeci(counter_tmp, CUID_BASE_36, next());
	pad(counter_tmp, CUID_BLOCK_SIZE, counter);

	zbx_snprintf(rand_block_1_tmp, sizeof(rand_block_1_tmp), "%x", rand() & 0xffff);

	pad(rand_block_1_tmp, CUID_BLOCK_SIZE, rand_block_1);

	zbx_snprintf(rand_block_2_tmp, sizeof(rand_block_2_tmp), "%x", rand() & 0xffff);
	pad(rand_block_2_tmp, CUID_BLOCK_SIZE, rand_block_2);

	zbx_snprintf(cuid, CUID_LEN, "c%s%s%s%s%s", timestamp, counter, fingerprint, rand_block_1, rand_block_2);

	zbx_free(hostname);

	return SUCCEED;
}
