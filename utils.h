#ifndef _UTILS_H_
#define _UTILS_H_

/**
 * For sigaction and siginfo_t _POSIX_C_SOURCE
 *  greater or equal to the value of 199309
 */
#define _POSIX_C_SOURCE 199309

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <poll.h>
#include <sys/time.h>

#define M_SLAVES 10
#define M_CMD_LEN 100
#define M_ARG_CMD 5 
#define M_PARAM_SLAVE 5
#define M_NAME_LEN 10
#define M_INSTR_LEN 256

#define READ_END 0
#define WRITE_END 1

#define SHARED_MEM_SIZE 1024


/// @brief enum used to pass instructions between a master and the configurator
typedef enum
{
	UNDEFINED               =  0,
	END_CFGT                =  1,
	LIST_ALL_SLAVES         =  2,
	START_MASTER            =  3,
	HELP                    =  4,
	STOP_MASTER             =  5,
	LIST_CONN_SLAVES        =  6,
	CONNECT_SLAVE           =  7,
	DISCONNECT_SLAVE        =  8,
	ALL_PARAM_SLAVE         =  9,
	VALUE_PARAM_SLAVE       = 10,
	LOGS                    = 11,
	INC_CYC_TIME            = 12,
	DEC_CYC_TIME            = 13,
	START_PARAM_SLAVE       = 14,
	STOP_PARAM_SLAVE        = 15,
} instruction_t;

typedef enum {
	INTEGER = 0,
	STRING = 1,
	BOOL = 2,
} param_type_t;

typedef enum
{
	ESTABLISH_CONN          = 20,
	ACK                     = 21,
	INC_TIME                = 22,
	DEC_TIME                = 23,
	CHANGE_NAME             = 24,
	START_PARAM             = 25,
	STOP_PARAM              = 26,
	SLEEP                   = 27,
	STOP_CONN               = 28,
} command_id_t;

typedef struct command
{
	command_id_t id;
	char args[5][M_CMD_LEN];
} command_t;

typedef union param_value
{
	bool boolean;
	char string[50];
	int integer;
} param_value_t;

typedef struct parameters
{
	char name[M_NAME_LEN];
	param_type_t type;
	param_value_t value;
} parameter_t;

/// @brief data type used by a master or slave to store information about the parameters of a slave
typedef struct param_set
{
    int nr;
    parameter_t params[M_PARAM_SLAVE];
} param_set_t;



#endif /* UTILS_H */
