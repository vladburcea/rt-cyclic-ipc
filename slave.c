#include "utils_slave.h"

char name[M_NAME_LEN];

void *flag_addr;
int flag_offset;

int shmid;
void *shm_addr;

pid_t pid_master = -1;
struct sigaction new_action;
int cycle_time;

param_set_t p_set; 

int lifetime, new_cmd;
connection_state_t cstate;

void sig_handler(int signum, siginfo_t *info, void *context __attribute__((unused)))
{
	switch (signum)
	{
	case SIGUSR1:
		new_cmd = 1;
		break;

	case SIGUSR2:
		if (info->si_pid == getppid())
		{
			lifetime = 1;
		}
		else if (pid_master == -1)
		{
			cstate = HANDSHAKE;
			pid_master = info->si_pid;
		}
		break;
	
	default:
		break;
	}
}

void initialize(void)
{
	int i;
	key_t shm_key;

	cstate = MASTERLESS;

	// Set the seed for rand
	struct timeval t1;
	gettimeofday(&t1, NULL);
	srand(t1.tv_usec * t1.tv_sec);

	// Pick a name from the list
	strcpy(name, names[rand() % n_names]);

	// Pick a parameter from the list
	p_set.nr = rand() % M_PARAM_SLAVE;

	// Choose name and type for each parameter
	for (i = 0; i < p_set.nr; i++)
	{
		p_set.params[i].type = rand() % 3;
		strcpy(p_set.params[i].name, param_list[rand() % n_param]);
	}

	// Create key used for shared memory
	shm_key = ftok("key_shared_memory", getpid());

	// Create shared memory space for communication with master
	shmid = shmget(shm_key, SHARED_MEM_SIZE, IPC_CREAT | 0666);
	if (shmid < 0)
	{
		perror("Failed allocating shared memory in slave.");
		exit(1);
	}

	// Attach to the segment
	shm_addr = shmat(shmid, NULL, 0);
	if (shm_addr == (void *) -1)
	{
		perror("Failed attaching to shared memory segment in slave.\n");
		exit(1);
	}

	// Overwrite signal handler for SIGUSR1 & SIGUSR2 
	// Set the handler in the new_action struct
	new_action.sa_sigaction = sig_handler;
	
	// Empty the sa_mask -> no signal is blocked
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
	
	// Overwrite signal management for following signals: 
	sigaction(SIGUSR1, &new_action, NULL);
	sigaction(SIGUSR2, &new_action, NULL);
}

void finalize(void)
{
	if (shmctl(shmid, IPC_RMID, 0) == -1)
	{
		perror("shmctl shared memory slave");
		exit(1);
	}
}

void establish_conn(char args[][M_CMD_LEN])
{
	int shmid_flags;
	key_t shm_key;

	// Create key used for flags shm
	shm_key = ftok("key_shared_memory", pid_master);

	// Init shared memory space for flags
	shmid_flags = shmget(shm_key, M_SLAVES, 0666);
	if (shmid_flags < 0)
	{
		perror("Failed allocating shared memory in slave.\n");
		exit(1);
	}

	// Attach to the segment
	flag_addr = shmat(shmid_flags, NULL, 0);
	if (flag_addr == (void *) -1)
	{
		perror("Failed attaching to shared memory segment in slave.\n");
		exit(1);
	}
	// Set cycle time
	cycle_time = atoi(args[0]);

	// save the offset
	flag_offset = atoi(args[1]);

	// Load param into shared memory space
	// Load all the parameters the slave has into structure
	memset(shm_addr, 0, SHARED_MEM_SIZE);
	memcpy(shm_addr, name, M_NAME_LEN);
	memcpy(shm_addr + M_NAME_LEN, &p_set, sizeof(param_set_t));

	kill(pid_master, SIGUSR2);
}

void send_data()
{

}

void send_ack(void)
{
	command_t c;
	c.id = ACK;

	memcpy(shm_addr, &c, sizeof(c));
	kill(pid_master, SIGUSR1);
}

int exec_cmd(command_t cmd)
{
	switch (cmd.id)
	{
	case ESTABLISH_CONN:
		establish_conn(cmd.args);
		break;

	case ACK:
		// TODO: what do when ack
		break;
	
	case SLEEP:
		sleep(atoi(cmd.args[1]));
		break;

	case STOP_CONN:
		// Detach from shm for flags
		if (flag_addr != NULL)
			if (shmdt(flag_addr) == -1)
			{
				perror("shmdt flags slave");
				exit(1);
			}
		
		// Reset master pid
		pid_master = -1;
		cstate = MASTERLESS;
		break;

	case INC_TIME:
		cycle_time += atoi(cmd.args[0]);
		printf("S: New cycle time is %d.\n", cycle_time);
		send_ack();
		break;

	case DEC_TIME:
		if (cycle_time - atoi(cmd.args[0]) > 0)
			cycle_time -= atoi(cmd.args[0]);
		printf("S: New cycle time is %d.\n", cycle_time);
		// TODO: ce fac in caz contrar -> NACK
		send_ack();
		break;

	case CHANGE_NAME:
		memset(name, 0, M_NAME_LEN);
		strcpy(name, cmd.args[0]);
		send_ack();
		break;

	default:
		break;
	}
	return 0;
}

int main(void) {
	command_t c;
	int i;

	initialize();
	printf("S: %s %d ", name, getpid());
	for (i = 0; i < p_set.nr; i++)
		printf(" %s", p_set.params[i].name);
	printf("\n");

	while(!lifetime)
	{
		if (cstate == HANDSHAKE)
		{
			printf("S: From %s %d, initiated handshake\n", name, getpid());
			cstate = MASTERFULL;
			new_cmd = 1;
		}
		if (cstate == MASTERFULL && new_cmd)
		{
			memcpy(&c, shm_addr, sizeof(c));
			printf("S: Received %d %s %s\n", c.id, c.args[0], c.args[1]);
			new_cmd = 0;
			exec_cmd(c);
		}
	}

	finalize();

	return 0;
}
