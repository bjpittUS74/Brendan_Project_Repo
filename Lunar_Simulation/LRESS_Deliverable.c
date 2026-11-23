#include <stdio.h>
#include <stdlib.h>

#define MAX_CHANNELS 4
#define NUM_TYPES 6
#define MAX_JOBS     128
#define SIM_END_MS   4000
#define UDW_MAX      50 



/*Lunar Data Type Definition*/ 
typedef struct {
	const char *name;
	int gpcn;   	
	int exec_ms;
	int period_ms;
	int udw;
	} DataType; 

/*Job/Task Definition*/
typedef enum {JOB_FREE, JOB_PENDING, JOB_RUNNING} JobState;

typedef struct{
	int type_idx;
	int release_ms;
	int deadline_ms;
	int remaining_ms;
	float ldpn;
	JobState state;
} Job;


DataType types[NUM_TYPES]=
{
	{"Safety-Critical Data",      10,  20,  100, 1},
    {"Sensor Data",                8,  60,  200, 1},
    {"Coordinate Data",            7,  40,  400, 1},
    {"Network Status Data",        5,  25,  500, 1},
    {"General Imagery Data",       3, 800, 2000, 1},
    {"Lunar Rover Summary Data",   2, 400, 4000, 1},
};

Job jobs[MAX_JOBS]; /*Container for all Jobs*/
int channel_job[MAX_CHANNELS]; /*Container for all Channels*/
int num_channels; /*User input of Channel*/



/*Input Validation and Retrieval*/
int get_int_in_range(const char*prompt, int lo, int hi)
{
	int v;
	while(1){
			printf("%s (%d-%d): ", prompt, lo, hi);
				if (scanf("%d", &v) == 1 && v >= lo && v <= hi) return v;
				while (getchar() != '\n');   /* Flush bad input */
				printf("Invalid input.\n");
			}
}

void get_user_input(void)
{
	num_channels = get_int_in_range("Number of Channels",1,MAX_CHANNELS);
	for (int i=0; i < NUM_TYPES; i++)
	{
	char prompt[80];
	snprintf(prompt, sizeof prompt, "UDW for %s", types[i].name);
	types[i].udw = get_int_in_range(prompt, 1, UDW_MAX);	
	}
}

/*Computing the Deadline Distance and Lunar Data Priority Number*/

float compute_dd(const Job *j, int now)
{
	int period = types[j->type_idx].period_ms;
	float slack = (float)(j->deadline_ms- now - j->remaining_ms);
	float dd = slack/period *10.0f;
	if (dd < 0.0f) dd = 0.0f;
	if (dd > 10.0f) dd = 10.0f;
	return dd;
	
}
float compute_ldpn(const Job *j, int now)
{
	const DataType *t =&types[j->type_idx];
	float ldpn = 0.4f* t->gpcn + 0.4f*(10.0f - compute_dd(j,now)) + 0.2f * t->udw;
	return ldpn;
}

/*Release Jobs (JOB FREE -> JOB PENDING)*/
void release_jobs(int now)
{
 for(int i=0; i < NUM_TYPES; i++)
 {
	if(now % types[i].period_ms != 0) continue;
	
	int slot = -1;
	for(int k = 0; k < MAX_JOBS; k++)
	{
		if(jobs[k].state == JOB_FREE)
		{
			slot = k;
			break;
		}
	}
	if (slot<0)
	{
	//Error for if jobs[] array has reached capacity (128 Jobs)
	printf("[t=%d ms] ERROR: job pool exhausted\n",now);
	exit(1);
	}
	
	jobs[slot] = (Job){i,now,now +types[i].period_ms, types[i].exec_ms,0.0f,JOB_PENDING};
 }
	
}

/*Deadline Verification*/
void check_deadlines(int now)
{
	for (int k=0; k<MAX_JOBS; k++)
	{
			if(jobs[k].state == JOB_FREE) continue;
			if(now >= jobs[k].deadline_ms && jobs[k].remaining_ms > 0)
			{
				printf("[t=%d ms] ERROR: scheduling conflict - %s (released %d ms) " "missed its deadline and could not be sent.\n", now, types[jobs[k].type_idx].name, jobs[k].release_ms);
				exit(1);	
			}
		
	}
}

/*Dispatch Jobs Into Schedule*/
void dispatch(int now)
{
	for (int c = 0; c < num_channels; c++)
	{
		if(channel_job[c] != -1) continue;
		
		int best = -1;
		for (int k=0; k< MAX_JOBS; k++)
		{
				if(jobs[k].state != JOB_PENDING) continue;
				jobs[k].ldpn = compute_ldpn(&jobs[k],now);
				if (best < 0 || jobs[k].ldpn > jobs[best].ldpn) best = k;
		}
		if (best < 0) return;
		
		jobs[best].state = JOB_RUNNING;
		channel_job[c] = best;
		printf("[t=%d ms] Channel %d: transmitting %s (LDPN %.2f)\n",now, c + 1, types[jobs[best].type_idx].name, jobs[best].ldpn);
	}
}

/*Execute a Millisecond Tic*/
void execute_tick(int now)
{
		for(int c=0; c < num_channels; c++)
		{
			int k =channel_job[c];
			if (k == -1) continue;
			if(--jobs[k].remaining_ms == 0)
				{
					printf("[t=%d ms] Channel %d: finished %s\n",now + 1, c + 1, types[jobs[k].type_idx].name);
					jobs[k].state = JOB_FREE;
					channel_job[c] = -1;
				}
			
		}
}

/*Main*/
int main(void)
{
    get_user_input();
    for (int c = 0; c < MAX_CHANNELS; c++) channel_job[c] = -1;

    for (int now = 0; now < SIM_END_MS; now++) {
        release_jobs(now);
        check_deadlines(now);
        dispatch(now);
        execute_tick(now);
    }
    printf("Simulation completed with no scheduling conflicts.\n");
    return 0;
}


