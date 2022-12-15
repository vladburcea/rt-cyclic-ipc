#ifndef _UTIL_CONFIG_H_
#define _UTIL_CONFIG_H_
#include "utils.h"

#define PRINT_INSTR printf("List of available commands:\n" \
			" # start master -> starts a master process \n" \
			" # stop master -> stops the current master process \n" \
			" # list all slaves -> lists all available slaves\n" \
			" # list connected slaves -> lists only slaves connected to a master\n" \
			" # param SLAVE_NAME -> lists all parameters the are requested from a slave at that time\n" \
			" # value param SLAVE_NAME -> lists the value received from the slave for the asked parameters\n" \
			" # connect PID_OF_SLAVE -> connects to a certain slave\n" \
			" # disconnect SLAVE_NAME -> disconnects to a certain slave\n" \
			" # increase cycle time SLAVE_NAME VALUE -> increase cycle time of slave with name SLAVE_NAME by VALUE\n" \
			" # decrease cycle time SLAVE_NAME VALUE -> decrease cycle time of slave with name SLAVE_NAME by VALUE\n" \
			" # start param SLAVE_NAME PARAM_NAME -> ask slave with SLAVE_NAME to start giving parameter with PARAM_NAME\n" \
			" # stop param SLAVE_NAME PARAM_NAME -> ask slave with SLAVE_NAME to stop giving parameter with PARAM_NAME\n" \
			" # logs -> lists all logs by severity\n" \
			" # exit/end -> stops the configurator gracefully\n");

int n_slaves;
pid_t *slaves;

pid_t master;
int m_pipe[2], c_pipe[2];

struct pollfd master_pfds[2];

#endif /* UTIL_CONFIG_H */