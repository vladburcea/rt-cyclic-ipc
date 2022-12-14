#ifndef _UTILS_MASTER_H_
#define _UTILS_MASTER_H_

#include "utils.h"
#include <pthread.h>

typedef struct utils_slave
{
	pid_t pid;
	int flag_offset;
	void *shm_addr;
	char name[M_NAME_LEN];
	param_set_t p_set;
} slave_t;

int curr_slaves;
slave_t slaves[M_SLAVES];
int connected, recv_data;

int shmid_flags;
char *flags_addr;
int flags[3 * M_SLAVES];
struct sigaction new_action;

sigset_t connect_set, command_set;
struct timespec timeout_connection, timeout_command;

#endif /* _UTILS_MASTER_H_ */