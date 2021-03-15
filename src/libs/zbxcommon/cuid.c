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

static int32_t	next()
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

static void	fromDeci(char res[], int base, unsigned long inputNum)
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
	size_t	i;
	size_t offset;
	size_t input_len;

	input_len = strlen(input);

	offset = pad_size > input_len ? pad_size - input_len : 0;

	printf("NNN PAD SIZE: %d and strlen: %d, offset: %d", pad_size, strlen(input), offset);

	memset(output, '0', pad_size);

	for (i = 0; i < strlen(input);i++)
	{
		printf("output at: %d, equals %c\n", i + offset,  input[i]);
		output[i + offset] = input[i];
	}


	output[pad_size] = '\0';
	printf("FINAL OUTPUT: ->%s<- \n", output);

}

int	new_cuid(char cuid[CUID_LEN])
{
	int	pid;
	char	rand_block_1[CUID_BLOCK_SIZE + 1], rand_block_2[CUID_BLOCK_SIZE + 1], fingerprint[CUID_BLOCK_SIZE + 1],
		timestamp[CUID_TIMESTAMP_SIZE + 1], counter_tmp[CUID_BLOCK_SIZE + 1],
		counter[CUID_BLOCK_SIZE+1], rand_block_1_tmp[CUID_BLOCK_SIZE + 1],
		rand_block_2_tmp[CUID_BLOCK_SIZE + 1];

	char hostname_block_tmp[3];
	char pid_block_tmp[PID_TMP_36_BASE_BUF_LEN];
	char host_block_tmp[HOST_TMP_36_BASE_BUF_LEN];
	char	pid_block[3], host_block[3];

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
		int	x, y, i;

		y = strlen(hostname);
		x = y + CUID_BASE_36;

		for (i = 0; i < y; i++)
			x = x + hostname[i];

		printf("X: ->%d<- \n",x);
		printf("X PID: ->%d<- \n",pid);

		fromDeci(host_block_tmp, 10, x );
		fromDeci(pid_block_tmp, CUID_BASE_36, pid );

		printf("PID_BLOCK TMP : ->%s<- \n", pid_block_tmp);
		pad(pid_block_tmp, CUID_PID_BLOCK_SIZE, pid_block);
		printf("PID_BLOCK: ->%s<- \n", pid_block);

		printf("HOST_BLOCK TMP : ->%s<- \n", host_block_tmp);
		pad(host_block_tmp, CUID_HOSTNAME_BLOCK_SIZE, host_block);
		printf("HOST_BLOCK ->%s<- \n", host_block);
		zbx_snprintf(fingerprint, sizeof(fingerprint), "%s%s", host_block, pid_block);
	}

	seconds = time(NULL);
	fromDeci(timestamp, CUID_BASE_36, seconds * 1000);
	fromDeci(counter_tmp, CUID_BASE_36, next());
	pad(counter_tmp, CUID_BLOCK_SIZE, counter);

	zbx_snprintf(rand_block_1_tmp, sizeof(rand_block_1_tmp), "%x", rand() & 0xffff);

	pad(rand_block_1_tmp, CUID_BLOCK_SIZE, rand_block_1);

	// printf("rand_block_1 0000: ->%s<-\n", rand_block_1);
	// printf("counter1X: ->%s<-\n", counter);

	zbx_snprintf(rand_block_2_tmp, sizeof(rand_block_2_tmp), "%x", rand() & 0xffff);
	pad(rand_block_2_tmp, CUID_BLOCK_SIZE, rand_block_2);

	// printf("rand_block_1: ->%s<-\n", rand_block_1);

	printf("FINGERPRINT: %s\n",fingerprint);

	printf("timestamp: ->%s<-\n", timestamp);
	printf("counter: ->%s<-\n", counter);
	printf("rand_block_1: ->%s<-\n", rand_block_1);
	printf("rand_block_12 ->%s<-\n", rand_block_2);

	// zbx_snprintf(cuid, 300, "c - timestamp: %s - counter %s - fingerprint %s - rand_block_1 %s - rand_block_2 %s", timestamp, counter, fingerprint, rand_block_1 ,rand_block_2);

	zbx_snprintf(cuid, CUID_LEN, "c%s%s%s%s%s", timestamp, counter, fingerprint, rand_block_1 ,rand_block_2);

	zbx_free(hostname);

	return SUCCEED;
}
