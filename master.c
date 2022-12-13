#include "utils_master.h"

/// @brief search_name() function establishes if the string given as argument is the name of one of the slaves or not
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

void connect_to_slave(pid_t pid)
{
	int shmid, i;
	key_t shm_key;
	command_t c;

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
	while(!connected);	// TODO: Should add a timeout limit -> sigtimedwait() ?

	// Reset connexion state
	connected = 0;

	// Load name into structure	
	memcpy(slaves[curr_slaves].name, 
			slaves[curr_slaves].shm_addr,
			M_NAME_LEN);

	// Load all the parameters the slave has into structure
	memcpy(&(slaves[curr_slaves].p_set),
			slaves[curr_slaves].shm_addr + M_NAME_LEN,
			sizeof(param_set_t));

	curr_slaves++;
}

void disconnect_slave(int idx)
{
	command_t cmd;
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
	curr_slaves--;
}

void change_cycle_time(char *param, command_id_t c)
{
	int found;
	command_t cmd;
	char *name, *arg, buff[M_INSTR_LEN];

	// Separate name from arg
	name = strtok(param, " ");

	// Find the index of the slave in question
	found = search_name(name);

	if (found != -1)
	{
		// get the argument of the command
		arg = strtok(NULL, " \n");

		// Set the type of the command (increase or decrease) and copy it into shm
		cmd.id = c;
		strcpy(cmd.args[0], arg);
		memcpy(slaves[found].shm_addr, &cmd, sizeof(cmd));

		// Send signal to slave
		kill(slaves[found].pid, SIGUSR1);

		// Prepare response for configurator
		memset(buff, 0, M_INSTR_LEN);
		if (cmd.id == INC_TIME)
			sprintf(buff, "M: Asked %s to increase cycle time by %d.\n",
						slaves[found].name, atoi(cmd.args[0]));
		else
			sprintf(buff, "M: Asked %s to decrease cycle time by %d.\n",
						slaves[found].name, atoi(cmd.args[0]));

		while (recv_data != 1);
		recv_data = 0;
		// TODO: check received message from slave. if ack or not

		if (cmd.id == DEC_TIME)
			strcat(buff, "M: Increased cycle time successfully.\n");
		else
			strcat(buff, "M: Decreased cycle time successfully.\n");
		write(STDOUT_FILENO, buff, M_INSTR_LEN);
	}
	else
	{
		memset(buff, 0, M_INSTR_LEN);
		sprintf(buff, "M: No slave with name %s is connected.", name);
		write(STDOUT_FILENO, buff, M_INSTR_LEN);
	}
}

int exec_instr(char *instr)
{
	int i, instr_id, found;
	char buff[M_INSTR_LEN];
	char *aux, spare[50];;
	command_t to_send;
	pid_t p;

	// Obtain command id from message
	aux = strtok(instr, "  \n");
	instr_id = atoi(aux);

	switch (instr_id)
	{
	case LIST_CONN_SLAVES:
		if (curr_slaves == 0)
		{
			write(STDOUT_FILENO, "M: No slaves connected.\n", M_INSTR_LEN);
		}
		else
		{
			memset(buff, 0, M_INSTR_LEN);
			sprintf(buff, "M: Connected slaves: ");
			for (i = 0; i < curr_slaves; i++)
			{
				if (i == curr_slaves - 1)
					sprintf(aux, "%s\n", slaves[i].name);
				else
					sprintf(aux, "%s, ", slaves[i].name);
				strcat(buff, aux);
			}
			write(STDOUT_FILENO, buff, M_INSTR_LEN);
		}
		break;

	case CONNECT_SLAVE:
		aux = strtok(NULL, " ");
		p = atoi(aux);
	
		connect_to_slave(p); // TODO: send feedback to configurator about connection

		memset(buff, 0, M_INSTR_LEN);
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
				// Tell the newly connected slave to change his name
				to_send.id = CHANGE_NAME;

				// Get the female variant of the name
				if (strcmp(slaves[curr_slaves - 1].name, "Kyle") == 0)
				{
					memset(to_send.args[0], 0, M_ARG_CMD);
					strcpy(to_send.args[0], "Kyla");
				}
				else if (strcmp(slaves[curr_slaves - 1].name, "Dan") == 0)
				{
					memset(to_send.args[0], 0, M_ARG_CMD);
					strcpy(to_send.args[0], "Dana");
				}
				else if (strcmp(slaves[curr_slaves - 1].name, "Andrew") == 0)
				{
					memset(to_send.args[0], 0, M_ARG_CMD);
					strcpy(to_send.args[0], "Andrea");
				}
				else if (strcmp(slaves[curr_slaves - 1].name, "Brian") == 0)
				{
					memset(to_send.args[0], 0, M_ARG_CMD);
					strcpy(to_send.args[0], "Brianna");
				}
				else if (strcmp(slaves[curr_slaves - 1].name, "Felix") == 0)
				{
					memset(to_send.args[0], 0, M_ARG_CMD);
					strcpy(to_send.args[0], "Felicia");
				}

				// Update name in personal storage
				memset(slaves[curr_slaves - 1].name, 0, M_ARG_CMD);
				strcpy(slaves[curr_slaves - 1].name, to_send.args[0]);

				// Send the cmd to the slave
				memcpy(slaves[curr_slaves - 1].shm_addr, &to_send, sizeof(to_send));
				kill(slaves[curr_slaves - 1].pid, SIGUSR1);
				
				sprintf(buff, "M: Asked newly connected slave to change name.\n");

				while (recv_data != 1);
				recv_data = 0;
				// TODO: check received message from slave. if ack or not

				strcat(buff, "M: Name change sucessfull.\n");
			}
		}

		memset(spare, 0, 50);
		sprintf(spare, "M: Connection to slave %s established.\n", slaves[curr_slaves - 1].name);

		// Present all parameters to the master
		strcat(buff, spare);
		if (slaves[curr_slaves - 1].p_set.nr == 0)
			{
				strcat(buff, "M: There are no parameters available for this slave.\n");
			}
			else
			{
				memset(spare, 0, 50);
				sprintf(spare, "M: Available parameters for %s are: ", slaves[curr_slaves - 1].name);
				strcat(buff, spare);
				for (i = 0; i < slaves[curr_slaves - 1].p_set.nr; i++)
				{
					memset(spare, 0, 50);
					if (i == slaves[curr_slaves - 1].p_set.nr - 1)
						sprintf(spare, "%s\n", slaves[curr_slaves - 1].p_set.params[i].name);
					else
						sprintf(spare, "%s, ", slaves[curr_slaves - 1].p_set.params[i].name);
					strcat(buff, spare);
				}
				write(STDOUT_FILENO, buff, M_INSTR_LEN);
			}

		write(STDOUT_FILENO, buff, M_INSTR_LEN);
		break;

	case DISCONNECT_SLAVE:
		aux = strtok(NULL, " \n");

		// Find the index of the slave in question
		found = search_name(aux);

		if (found != -1)
		{
			disconnect_slave(found);
			memset(buff, 0, M_INSTR_LEN);
			sprintf(buff, "M: Stopped communication with slave %s.\n", aux);
			write(STDOUT_FILENO, buff, M_INSTR_LEN);
		}
		else
		{
			memset(buff, 0, M_INSTR_LEN);
			sprintf(buff, "M: No slave with name %s is connected.", aux);
			write(STDOUT_FILENO, buff, M_INSTR_LEN);
		}
		break;

	case ALL_PARAM_SLAVE:
		aux = strtok(NULL, " \n");

		// Find the index of the slave in question
		found = search_name(aux);

		if (found != -1)
		{
			if (slaves[found].p_set.nr == 0)
			{
				memset(buff, 0, M_INSTR_LEN);
				sprintf(buff, "M: There are no parameters available for %s.", slaves[found].name);
				write(STDOUT_FILENO, buff, M_INSTR_LEN);
			}
			else
			{
				memset(buff, 0, M_INSTR_LEN);
				sprintf(buff, "M: Available parameters for %s are: ", slaves[found].name);
				for (i = 0; i < slaves[found].p_set.nr; i++)
				{
					if (i == slaves[found].p_set.nr - 1)
						sprintf(aux, "%s", slaves[found].p_set.params[i].name);
					else
						sprintf(aux, "%s, ", slaves[found].p_set.params[i].name);
					strcat(buff, aux);
				}
				write(STDOUT_FILENO, buff, M_INSTR_LEN);
			}
		}
		else
		{
			memset(buff, 0, M_INSTR_LEN);
			sprintf(buff, "M: No slave with name %s is connected.", aux);
			write(STDOUT_FILENO, buff, M_INSTR_LEN);
		}
		break;

	case INC_CYC_TIME:
		// Separate arguments from instr_id
		aux = strtok(NULL, " \n");

		change_cycle_time(aux, INC_CYC_TIME);
		break;
	
	case DEC_CYC_TIME:
		// Separate arguments from instr_id
		aux = strtok(NULL, " \n");

		change_cycle_time(aux, DEC_CYC_TIME);
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

	// memset(flags, 0, sizeof(*flags) * M_SLAVES);
	strcpy(flags_addr, "abcdefg\n");

	// Overwrite signal handler for SIGUSR1 & SIGUSR2 
	// Set the handler in the new_action struct
	new_action.sa_handler = sig_handler;
	
	// Empty the sa_mask -> no signal is blocked
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	
	// Overwrite signal management for following signals: 
	sigaction(SIGUSR1, &new_action, NULL);
	sigaction(SIGUSR2, &new_action, NULL);
}

void finalize(void)
{
	int i;
	for (i = 0; i < curr_slaves; i++)
		if (shmdt(slaves[i].shm_addr) == -1)
		{
			perror("shmdt shm slaves");
			exit(1);
		}

	if (shmctl(shmid_flags, IPC_RMID, 0) == -1)
	{
		perror("shmctl flags");
		exit(1);
	}
}

int main(void) {
	int alive;
	char buffer[M_INSTR_LEN];

	initialize();

	alive = 1;
	while (alive)
	{
		memset(buffer, 0, M_INSTR_LEN);
		if (read(STDIN_FILENO, buffer, M_INSTR_LEN) <= 0)
			break;
		alive = exec_instr(buffer);
	}
	
	finalize();

	return 0;
}


