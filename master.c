#include "utils_master.h"

void sig_handler(int signum)
{
	switch (signum)
	{
	case SIGUSR1:
		recv_data = 1;
		break;

	case SIGUSR2:
		connected = 1;
		break;

	default:
		break;
	}
}

/// @brief search_name() establishes if argument is the name of one of the slaves or not
/// @param name a value of type (char *) to be compared to each slave's name
/// @return return -1 if the name isn't found or the index of the slave with said name
int search_name(char *name)
{
	int found, i;

	found = -1;
	for (i = 0 ; i < curr_slaves; i++)
		if (strcmp(name, slaves[i].name) == 0)
		{
			found = i;
			break;
		}
	return found;
}

/// @brief print_parameters() appends the list of all parameters of a given slave
/// to the first argument of the function
/// @param buff (Output) pointer to a string where information regarding the status of the command will be outputed
/// @param index (Input) index of the slave
void print_parameters(char* buff, int index)
{
	int i;
	char spare_buff[M_INSTR_LEN];

	if (slaves[index].p_set.nr == 0)
	{
		strcat(buff, "M: There are no parameters available for this slave.");
	}
	else
	{
		memset(spare_buff, 0, M_INSTR_LEN);
		sprintf(spare_buff, "M: Available parameters for %s are: ", slaves[index].name);
		strcat(buff, spare_buff);
		for (i = 0; i < slaves[index].p_set.nr; i++)
		{
			memset(spare_buff, 0, M_INSTR_LEN);
			if (i == slaves[index].p_set.nr - 1)
				sprintf(spare_buff, "%s", slaves[index].p_set.params[i].name);
			else
				sprintf(spare_buff, "%s, ", slaves[index].p_set.params[i].name);
			strcat(buff, spare_buff);
		}
	}
}

/// @brief change_name() sends a command to change its name to a specified value
/// @param buff (Output) pointer to a string where information regarding the status of the command will be outputed
/// @param new_name (Input) a pointer to the new name of the slave. 
/// If new_name is NULL, then the name sent to the slave will be the female varinat of its original name
/// @param index (Input) index of the slave
void change_name(char *buff, char *new_name, int index)
{
	command_t new_cmd;
	siginfo_t info;

	// Tell the newly connected slave to change his name
	new_cmd.id = CHANGE_NAME;

	// Get the female variant of the name
	if (new_name == NULL)
	{
		if (strcmp(slaves[index].name, "Kyle") == 0)
		{
			memset(new_cmd.args[0], 0, M_ARG_CMD);
			strcpy(new_cmd.args[0], "Kyla");
		}
		else if (strcmp(slaves[index].name, "Dan") == 0)
		{
			memset(new_cmd.args[0], 0, M_ARG_CMD);
			strcpy(new_cmd.args[0], "Dana");
		}
		else if (strcmp(slaves[index].name, "Andrew") == 0)
		{
			memset(new_cmd.args[0], 0, M_ARG_CMD);
			strcpy(new_cmd.args[0], "Andrea");
		}
		else if (strcmp(slaves[index].name, "Brian") == 0)
		{
			memset(new_cmd.args[0], 0, M_ARG_CMD);
			strcpy(new_cmd.args[0], "Brianna");
		}
		else if (strcmp(slaves[index].name, "Felix") == 0)
		{
			memset(new_cmd.args[0], 0, M_ARG_CMD);
			strcpy(new_cmd.args[0], "Felicia");
		}
	}
	else
	{
		memset(new_cmd.args[0], 0, M_ARG_CMD);
		strcpy(new_cmd.args[0], new_name);
	}
	
	// Update name in personal storage
	memset(slaves[index].name, 0, M_ARG_CMD);
	strcpy(slaves[index].name, new_cmd.args[0]);

	// Send the cmd to the slave
	memcpy(slaves[index].shm_addr, &new_cmd, sizeof(new_cmd));
	kill(slaves[index].pid, SIGUSR1);
	
	sprintf(buff, "M: Asked newly connected slave to change name.\n");

	// Wait for response from slave.
	if(sigtimedwait(&command_set, &info, &timeout_connection) == -1)
	{
		// TODO: do action 
		strcat(buff, "M: Name change failed. Slave didn't responde in designated time frame.\n");
	}
	else
	{
		recv_data = 0;
		strcat(buff, "M: Name change sucessfull.\n");
		// TODO: check received message from slave. if ack or not
	}
}

/// @brief connect_to_slave() connects to the slave with the pid specified in the second argument and outputs the status of 
/// the connection into the first argument
/// @param buff (Output) pointer to a string where information regarding the status of the command will be outputed
/// @param pid (Input) procces id of the slave to which the connection is going to be made
void connect_to_slave(char *buff, pid_t pid)
{
	int shmid, i, found;
	char spare_buff[50];
	command_t c;
	key_t shm_key;
	siginfo_t info;

	// Create key used for shared memory
	shm_key = ftok("key_shared_memory", pid);

	// Init shared memory space for communication with slave
	shmid = shmget(shm_key, SHARED_MEM_SIZE, 0666);
	if (shmid < 0)
	{
		perror("Failed allocating shared memory in slave.\n");
		exit(1);
	}

	// Attach to the segment
	slaves[curr_slaves].shm_addr = shmat(shmid, NULL, 0);
	if (slaves[curr_slaves].shm_addr == (void *) -1)
	{
		perror("Failed attaching to shared memory segment in slave.\n");
		exit(1);
	}

	// Load the first command into the shared memory
	c.id = ESTABLISH_CONN;

	// Set default cycle time to 50ms
	memset(c.args[0], 0, M_INSTR_LEN);
	sprintf(c.args[0], "50");

	// Get first available slot for a flag
	i = 0;
	while(flags[i] == 1)
		i++;
	flags[i] = 1;
	
	// Set the flag offset for this slave
	slaves[curr_slaves].flag_offset = i;
	slaves[curr_slaves].pid = pid;

	// Set the flag offset for the slave
	memset(c.args[1], 0, M_INSTR_LEN);
	sprintf(c.args[1], "%d", i);
	memcpy(slaves[curr_slaves].shm_addr, &c, sizeof(c));

	// Send signal to slave to initialize connection
	kill(pid, SIGUSR2);

	// Wait for response from slave.
	if(sigtimedwait(&connect_set, &info, &timeout_connection) == -1)
	{
		sprintf(buff, "Connection to slave %d timed out. (Time-out: 5 seconds).", pid);
		// TODO: and erase slave from memory
	}
	else
	{
		// Reset connexion state
		connected = 0;

		// Load name into structure	
		memcpy(slaves[curr_slaves].name, slaves[curr_slaves].shm_addr, M_NAME_LEN);

		// Load all the parameters the slave has into structure
		memcpy(&(slaves[curr_slaves].p_set), slaves[curr_slaves].shm_addr + M_NAME_LEN, sizeof(param_set_t));

		curr_slaves++;

		// Check for duplicate name
		if (curr_slaves >= 2)
		{
			found = -1;
			for (i = 0 ; i < curr_slaves - 1; i++)
				if (strcmp(slaves[curr_slaves - 1].name, slaves[i].name) == 0)
				{
					found = i;
					break;
				}

			if (found != -1)
			{
				change_name(buff, NULL, curr_slaves - 1);
			}
		}

		memset(spare_buff, 0, 50);
		sprintf(spare_buff, "M: Connection to slave %s established.\n", slaves[curr_slaves - 1].name);

		// Present all parameters to the master
		strcat(buff, spare_buff);
		print_parameters(buff, curr_slaves - 1);
	}
}

/// @brief disconnect_slave() disconnects from the slave specified in the second argument and outputs the status of 
/// the command into the first argument
/// @param buff (Output) pointer to a string where information regarding the status of the command will be outputed
/// @param name (Input) name of a connected slave
void disconnect_slave(char *buff, char *name)
{
	command_t cmd;
	int idx;

	idx = search_name(name);

	if (idx != -1)
	{
		cmd.id = STOP_CONN;

		// Tell the slave to stop communications
		memcpy(slaves[idx].shm_addr, &cmd, sizeof(cmd));
		kill(slaves[idx].pid, SIGUSR1);

		// Detach from slave's shm space
		if (shmdt(slaves[idx].shm_addr) == -1)
		{
			perror("shmdt flags");
			exit(1);
		}

		// Erase the slave from memory
		if (idx == curr_slaves - 1)
		{
			flags[slaves[idx].flag_offset] = 0;
			memset(&(slaves[idx]), 0, sizeof(*slaves));
		}
		else
		{
			flags[slaves[idx].flag_offset] = 0;
			memcpy(&(slaves[idx]), &(slaves[curr_slaves - 1]), sizeof(*slaves));
			memset(&(slaves[curr_slaves - 1]), 0, sizeof(*slaves));
		}

		sprintf(buff, "M: Stopped communication with slave %s.", name);

		curr_slaves--;
	}
	else
	{
		sprintf(buff, "M: No slave with name %s is connected.", name);
	}
}

/// @brief change_cycle_time() increases or decreases the cycle time of a slave based on the second argument of the command
/// @param param a pointer to a string with the name and value of the cycle time
/// @param c the value can be either INC_TIME or DEC_TIME based on which functionality the function needs to implements
void change_cycle_time(char *param, command_id_t c)
{
	int found;
	command_t cmd;
	siginfo_t info;
	char name[M_NAME_LEN], *arg, *endptr, buff[M_INSTR_LEN];

	// Separate name from arg
	strcpy(name, param);

	// get the argument of the command
	arg = strtok(NULL, " \n");

	if (arg == NULL)
	{
		sprintf(buff, "M: No value inputed for cycle time!");
		send_to_fd(buff, conf_pfds[WRITE_END]);
		return;
	}

	// Find the index of the slave in question
	found = search_name(name);

	memset(buff, 0, M_INSTR_LEN);
	if (found != -1)
	{
		// Set the type of the command (increase or decrease) and copy it into shm
		cmd.id = c;
		strcpy(cmd.args[0], arg);

		strtol(arg, &endptr, 10);
		if (arg == endptr || *endptr != '\0')
		{
			sprintf(buff, "M: Invalid input value for cycle time!");
		}
		else
		{
			memcpy(slaves[found].shm_addr, &cmd, sizeof(cmd));

			// Send signal to slave
			kill(slaves[found].pid, SIGUSR1);

			// Prepare response for configurator
			if (cmd.id == INC_TIME)
				sprintf(buff, "M: Asked %s to increase cycle time by %d.\n",
							slaves[found].name, atoi(cmd.args[0]));
			else
				sprintf(buff, "M: Asked %s to decrease cycle time by %d.\n",
							slaves[found].name, atoi(cmd.args[0]));

			// Wait for response from slave.
			if(sigtimedwait(&command_set, &info, &timeout_command) == -1)
			{
				// TODO: do action 
				if (cmd.id == INC_TIME)
					strcat(buff, "M: Failed to increase cycle time."
							"Slave didn't responde in designated time frame.\n");
				else
					strcat(buff, "M: Failed to decrease cycle time."
							"Slave didn't responde in designated time frame.\n");
			}
			else
			{
				recv_data = 0;
				if (cmd.id == INC_TIME)
					strcat(buff, "M: Increased cycle time successfully.\n");
				else
					strcat(buff, "M: Decreased cycle time successfully.\n");
				// TODO: check received message from slave. if ack or not
			}
		}
	}
	else
		sprintf(buff, "M: No slave with name %s is connected.", name);

	send_to_fd(buff, conf_pfds[WRITE_END]);
}

int exec_instr(char *instr)
{
	int i, instr_id, found;
	char buff[M_INSTR_LEN];
	char *aux;
	pid_t p;

	// Obtain command id from message
	aux = strtok(instr, "  \n");
	instr_id = atoi(aux);

	switch (instr_id)
	{
	case END_MASTER:
		return 0;
		break;
	case LIST_CONN_SLAVES:
		if (curr_slaves == 0)
		{
			send_to_fd("M: No slaves connected.", conf_pfds[WRITE_END]);
		}
		else
		{
			memset(buff, 0, M_INSTR_LEN);
			sprintf(buff, "M: Connected slaves: ");
			for (i = 0; i < curr_slaves; i++)
			{
				if (i == curr_slaves - 1)
					sprintf(aux, "%s.", slaves[i].name);
				else
					sprintf(aux, "%s, ", slaves[i].name);
				strcat(buff, aux);
			}
			send_to_fd(buff, conf_pfds[WRITE_END]);
		}
		break;

	case CONNECT_SLAVE:
		aux = strtok(NULL, " ");
		p = atoi(aux);
	
		memset(buff, 0, M_INSTR_LEN);
		connect_to_slave(buff, p); 

		// Send feedback to configurator about connection status
		send_to_fd(buff, conf_pfds[WRITE_END]);
		break;

	case DISCONNECT_SLAVE:
		aux = strtok(NULL, " \n");

		memset(buff, 0, M_INSTR_LEN);
		disconnect_slave(buff, aux);

		// Send feedback to configurator about connection status
		send_to_fd(buff, conf_pfds[WRITE_END]);

		break;

	case ALL_PARAM_SLAVE:
		aux = strtok(NULL, " \n");

		// Find the index of the slave in question
		found = search_name(aux);

		memset(buff, 0, M_INSTR_LEN);
		if (found != -1)
			print_parameters(buff, found);
		else
			sprintf(buff, "M: No slave with name %s is connected.", aux);
		send_to_fd(buff, conf_pfds[WRITE_END]);
		break;

	case INC_CYC_TIME:
		// Separate arguments from instr_id
		aux = strtok(NULL, " \n");

		change_cycle_time(aux, INC_TIME);
		break;
	
	case DEC_CYC_TIME:
		// Separate arguments from instr_id
		aux = strtok(NULL, " \n");

		change_cycle_time(aux, DEC_TIME);
		break;

	case LOGS:
		printf("Logs not yet implemented yet... :(\n");
		break;

	default: // Wrong command
		printf("Invalid command.\n");
		break;
	}

	return 1;
}

void initialize(void)
{
	key_t shm_key;
	char buff[M_INSTR_LEN];

	sigemptyset(&connect_set);
	sigaddset(&connect_set, SIGUSR2);

	sigprocmask(SIG_BLOCK, &connect_set, NULL);

	sigemptyset(&command_set);
	sigaddset(&command_set, SIGUSR1);

	sigprocmask(SIG_BLOCK, &command_set, NULL);

	timeout_connection.tv_sec = 5;     /* Number of seconds to wait */
	timeout_connection.tv_nsec = 0;  /* Number of nanoseconds to wait */

	timeout_command.tv_sec = 1;     /* Number of seconds to wait */
	timeout_command.tv_nsec = 0;  /* Number of nanoseconds to wait */

	// Create key used for shared memory for flags
	shm_key = ftok("key_shared_memory", getpid());

	// Create shared memory for flags
	shmid_flags = shmget(shm_key, sizeof(char) * M_SLAVES, IPC_CREAT | 0666);
	if(shmid_flags < 0)
	{
		perror("shmget flags");
		exit(-2);
	}

	// Attach to the segment
	flags_addr = shmat(shmid_flags, NULL, 0);
	if (flags_addr == (void *) -1)
	{
		perror("Failed attaching to shared memory segment in slave.\n");
		exit(1);
	}

	memset(flags, 0, sizeof(*flags) * M_SLAVES);

	// Overwrite signal handler for SIGUSR1 & SIGUSR2 
	// Set the handler in the new_action struct
	new_action.sa_handler = sig_handler;
	
	// Empty the sa_mask -> no signal is blocked
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	
	// Overwrite signal management for following signals: 
	sigaction(SIGUSR1, &new_action, NULL);
	sigaction(SIGUSR2, &new_action, NULL);
	
	// Initialize the struct pollfd variable
	conf_pfds[READ_END].fd = STDIN_FILENO;
	conf_pfds[READ_END].events = POLLIN | POLLHUP;
	
	conf_pfds[WRITE_END].fd = STDOUT_FILENO;
	conf_pfds[WRITE_END].events = POLLOUT;

	sprintf(buff, "Master with pid %d initialized.", getpid());
	send_to_fd(buff, conf_pfds[WRITE_END]);
}

void finalize(void)
{
	char buff[M_INSTR_LEN], buff2[M_INSTR_LEN];
	command_t cmd;
	int i;

	cmd.id = STOP_CONN;
	memset(buff, 0, M_INSTR_LEN);
	for (i = 0; i < curr_slaves; i++)
	{
		// Tell the slave to stop communications
		memcpy(slaves[i].shm_addr, &cmd, sizeof(cmd));
		kill(slaves[i].pid, SIGUSR1);

		// Detach from slave's shm space
		if (shmdt(slaves[i].shm_addr) == -1)
		{
			perror("shmdt flags");
			exit(1);
		}

		memset(buff2, 0, M_INSTR_LEN);
		if (i == curr_slaves - 1)
			sprintf(buff2,
				"M: Stopped communication with slave %s.",
				slaves[i].name);
		else
			sprintf(buff2,
					"M: Stopped communication with slave %s.\n",
					slaves[i].name);
		strcat(buff, buff2);
	}
	
	if (shmctl(shmid_flags, IPC_RMID, 0) == -1)
	{
		perror("shmctl flags");
		exit(1);
	}

	memset(buff2, 0, M_INSTR_LEN);
	if (curr_slaves == 0)
		sprintf(buff2,"M: Stopped master instance with pid %d gracefully.", getpid());
	else
		sprintf(buff2,"\nM: Stopped master instance with pid %d gracefully.", getpid());
	strcat(buff, buff2);
	send_to_fd(buff, conf_pfds[WRITE_END]);
}

int main(void) {
	int alive;
	char buffer[M_INSTR_LEN];

	initialize();

	alive = 1;
	while (alive)
	{
		memset(buffer, 0, M_INSTR_LEN);
		// read_from_fd(buffer, conf_pfds[READ_END]);
		if (recv_from_fd(buffer, conf_pfds[READ_END]) <= 0)
			break;
		alive = exec_instr(buffer);
	}
	
	finalize();

	return 0;
}


