#include "utils_configurator.h"

int send_instr(instruction_t instr, char *param)
{
	int err;
	char buff[M_INSTR_LEN];

	memset(buff, 0, M_INSTR_LEN);
	if (param == NULL)
		sprintf(buff, "%d\n", instr);
	else
		sprintf(buff, "%d %s\n", instr, param);

	// Blocking write
	err = send_to_fd(buff, master_pfds[WRITE_END]);

	if (!err)
		return err;

	// Blocking read
	memset(buff, 0, M_INSTR_LEN);
	err = recv_from_fd(buff, master_pfds[READ_END]);

	if (!err)
		return err;

	printf("%s\n", buff);

	return 1;
}

void create_slaves(int n)
{
	int i;

	// Allocate memory for vector of pids for slaves
	slaves = (pid_t *) malloc(sizeof(*slaves) * n);

	if (slaves == NULL)
	{
		perror("Malloc error with slaves.\n");
		exit(-1);
	}

	for (i = 0; i < n; i++)
	{
		sleep(.05);
		slaves[i] = fork();
		
		if (slaves[i] < 0)
		{
			perror("Can't create new slave.\n");
			exit(-1);
		} 
		else if(slaves[i] == 0)
		{
			char *argv_vect[] = {NULL};

			if (execvp("./slave", argv_vect) == -1)
			{
				perror("C: Failed running ./slave\n");
				exit(-1);
			}
			// For peace of mind
			exit(-1);
		}
	}
}

void create_master(void)
{
	if (master != -1)
	{
		printf("There's allready a master running. "
			"Only one master cand run at a time.\n Master PID: %d\n", master);
	}
	else
	{
		// Create the pipes used to talk to this master
		if (pipe(m_pipe) < 0)
			exit(1);

		if (pipe(c_pipe) < 0)
			exit(1);

		master = fork();

		if (master < 0)
		{
			perror("Can't create new master.\n");
			exit(-1);
		} 
		else if(master == 0)
		{
			// Redirect stderr to stdout for debugging.
			close(STDERR_FILENO);
			dup2(STDOUT_FILENO, STDERR_FILENO);

			// Redirect stdin and stdout to communicate with the configurator.
			close(STDIN_FILENO);
			dup2(m_pipe[READ_END],STDIN_FILENO);
			close(m_pipe[READ_END]);
			close(m_pipe[WRITE_END]);

			close(STDOUT_FILENO);
			dup2(c_pipe[WRITE_END], STDOUT_FILENO);
			close(c_pipe[READ_END]);
			close(c_pipe[WRITE_END]);

			char *argv_vect[] = {NULL};
			if (execvp("./master", argv_vect) == -1)
			{
				perror("C: Failed running ./master\n");
				exit(-1);
			}
			// For peace of mind
			exit(-2);
		}
		else
		{
			// Close the ends of the pipe that don't iterest me
			close(c_pipe[WRITE_END]);
			close(m_pipe[READ_END]);

			// Initialize the struct pollfd variable
			master_pfds[READ_END].fd = c_pipe[READ_END];
			master_pfds[READ_END].events = POLLIN;
			
			master_pfds[WRITE_END].fd = m_pipe[WRITE_END];
			master_pfds[WRITE_END].events = POLLOUT;
		}
	}
}

int end_master(void)
{
	int err;
	char buff[M_INSTR_LEN];

	if (master > 0)
	{
		printf("Stopping master with pid %d.\n", master);

		err = send_instr(END_MASTER, NULL);

		if (!err)
			return err;

		// Close remaining pipe ends
		close(c_pipe[READ_END]);
		close(m_pipe[WRITE_END]);

		// kill(master, SIGKILL);
		master = -1;
	}
	else
	{
		printf ("There's no master to stop! "
			"First start a master process with 'start master'.\n");
	}

	return 1;
}

void initialize(int argc, char *argv[])
{
	// If no number is given to the configurator run with only one slave
	if (argc != 1)
		n_slaves = atoi(argv[1]);
	else 
		n_slaves = 1;

	// Create n slaves
	create_slaves(n_slaves);
	
	master = -1;

	(n_slaves != 1) ? printf("Created %d slaves succesfully! Waiting for commands from user:\n", n_slaves) :
						printf("Created one slave succesfully! Waiting for commands from user:\n");
	PRINT_INSTR
}

void finalize(void)
{
	int i, status;

	if (master != -1)
		end_master();

	for (i = 0; i < n_slaves; i++)
	{
		printf("Stopping slave with PID: %d.\n", slaves[i]);
		kill(slaves[i], SIGUSR2);
	}

	while(wait(&status) != -1);

	free(slaves);
}

instruction_t parse_input(char *instr, char *param)
{
	instruction_t c = UNDEFINED;
	char *aux;

	// Parse input from user
	if ((strcmp(instr, "end\n") == 0) || (strcmp(instr, "exit\n") == 0))
	{
		return END_CFGT;
	}
	else if (strcmp(instr, "start master\n") == 0)
	{
		return START_MASTER;
	}
	else if (strcmp(instr, "stop master\n") == 0)
	{
		return STOP_MASTER;
	}
	else if (strcmp(instr, "list all slaves\n") == 0)
	{
		return LIST_ALL_SLAVES;
	}
	else if (strcmp(instr, "list connected slaves\n") == 0)
	{
		return LIST_CONN_SLAVES;
	}
	else if (strncmp(instr, "param", 5) == 0)
	{
		aux = strtok(instr, " ");
		if (!aux)
			return UNDEFINED;
		aux = strtok(NULL, " ");
		if (!aux)
			return UNDEFINED;
		strcpy(param, aux);
		param[strlen(param) - 1] = '\0';

		return ALL_PARAM_SLAVE;
	}
	else if (strncmp(instr, "value param", 11) == 0)
	{
		aux = strtok(instr, " ");
		if (!aux)
			return UNDEFINED;
		aux = strtok(NULL, " ");
		if (!aux)
			return UNDEFINED;
		aux = strtok(NULL, " ");
		if (!aux)
			return UNDEFINED;
		strcpy(param, aux);
		param[strlen(param) - 1] = '\0';

		return VALUE_PARAM_SLAVE;
	}
	else if (strncmp(instr, "connect", 7) == 0)
	{
		aux = strtok(instr, " ");
		if (!aux)
			return UNDEFINED;
		aux = strtok(NULL, " ");
		if (!aux)
			return UNDEFINED;
		strcpy(param, aux);
		param[strlen(param) - 1] = '\0';

		return CONNECT_SLAVE;
	}
	else if (strncmp(instr, "disconnect", 10) == 0)
	{
		aux = strtok(instr, " ");
		if (!aux)
			return UNDEFINED;
		aux = strtok(NULL, " ");
		if (!aux)
			return UNDEFINED;
		strcpy(param, aux);
		param[strlen(param) - 1] = '\0';

		return DISCONNECT_SLAVE;
	}
	else if (strcmp(instr, "help\n") == 0)
	{
		return HELP;
	}
	else if (strcmp(instr, "logs\n") == 0)
	{
		return LOGS;
	}
	else if (strncmp(instr,"start param", 11) == 0)
	{
		strcpy(param, instr + 12);
		param[strlen(param) - 1] = '\0';
		return START_PARAM_SLAVE;
	}
	else if (strncmp(instr,"stop param", 10) == 0)
	{
		strcpy(param, instr + 11);
		param[strlen(param) - 1] = '\0';
		return STOP_PARAM_SLAVE;
	}
	else if (strncmp(instr,"increase cycle time", 19) == 0)
	{
		strcpy(param, instr + 20);
		param[strlen(param) - 1] = '\0';
		return INC_CYC_TIME;
	}
	else if (strncmp(instr,"decrease cycle time", 19) == 0)
	{
		strcpy(param, instr + 20);
		param[strlen(param) - 1] = '\0';
		return DEC_CYC_TIME;
	}
	return c;
}

int exec_instr(instruction_t instr, char *param)
{
	int i, err;
	char buff[M_INSTR_LEN];
	pid_t p;

	// Check if there's a master to send the comand to 
	if (instr > START_MASTER && master == -1)
	{
		printf ("There's no master to send this command to! "
			"First start a master process with 'start master'.\n");
		return 1;
	}

	switch (instr)
	{
	case END_CFGT:
		return 0;
		break;

	case LIST_ALL_SLAVES:
		(n_slaves > 1) ? printf("There are %d available slaves:\n", n_slaves) : 
							printf("There is only one slave:\n");

		for (i = 0; i < n_slaves; i++)
			printf("%d. PID: %d.\n", i + 1, slaves[i]);
		break;

	case START_MASTER:
		create_master();
		err = recv_from_fd(buff, master_pfds[READ_END]);

		if (!err)
			return err;

		printf("%s\n", buff);

		break;

	case HELP:
		PRINT_INSTR
		break;

	case STOP_MASTER:
		end_master();
		break;

	case LIST_CONN_SLAVES:
		return send_instr(LIST_CONN_SLAVES, NULL);
		break;

	case CONNECT_SLAVE:
		p = atoi(param);
		for(i = 0; i < n_slaves; i++)
			if (slaves[i] == p)
				break;

		if (i == n_slaves)
			printf("There's no slave with pid: %d. "
				"You can see all available slaves using 'list all slaves'.\n", p);
		else
			return send_instr(CONNECT_SLAVE, param);
		break;

	case DISCONNECT_SLAVE:
		return send_instr(DISCONNECT_SLAVE, param);
		break;

	case ALL_PARAM_SLAVE:
		return send_instr(ALL_PARAM_SLAVE, param);
		break;

	case VALUE_PARAM_SLAVE:
		return send_instr(VALUE_PARAM_SLAVE, param);
		break;
	
	case START_PARAM_SLAVE:
		return send_instr(START_PARAM_SLAVE, param);
		break;

	case STOP_PARAM_SLAVE:
		return send_instr(STOP_PARAM_SLAVE, param);
		break;

	case INC_CYC_TIME:
		return send_instr(INC_CYC_TIME, param);
		break;

	case DEC_CYC_TIME:
		return send_instr(DEC_CYC_TIME, param);
		break;

	case LOGS:
		printf("Logs not yet implemented... :(\n");
		break;

	default: // Wrong command
		printf("Invalid command. To review command list again try 'help'\n");
		break;
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int alive;
	char instr[M_INSTR_LEN], param[M_INSTR_LEN];
	instruction_t instr_id;

	printf("Started configurator!\n");

	initialize(argc, argv);

	alive = 1;
	instr_id = UNDEFINED;
	while (alive)
	{
		printf("> ");
		memset(instr, 0, M_INSTR_LEN);

		if (!fgets(instr, M_INSTR_LEN, stdin)) {
			perror("Error reading command from stdin!");
			exit(1);
		}

		instr_id = parse_input(instr, param);

		alive = exec_instr(instr_id, param);
	}

	finalize();

	printf("Done Configurator\n");

	return 0;
}


