#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mach/mig.h>
#include "SimulationMain.h"
#include "distributions.h"

FILE *sim_outfile, *warmup_outfile;

event event_list[EVENT_ENTRIES];
patient wait_list[WAIT_ENTRIES];

int pri_type = FIFO_PRIORITY;
int num_event_list = 0;
int warmup;
int total_patient;


/* Main function. */
int main()
{
	int etype, urg_type;

	total_sim_time = TOTAL_SIM_TIME;
    total_patient = START_SIZE;
    warmup = 1;

    /* Single period for now */
    patient_seed = INITIAL_SEED + 1000 ;
    surgery_seed = INITIAL_SEED + 1000 ;
    patient_type_seed = INITIAL_SEED + 1000;

	/* output files. */
	warmup_outfile = fopen("/Users/ArcticPirates/Desktop/SimulationModel/warmup_output.txt", "w");
	sim_outfile = fopen("/Users/ArcticPirates/Desktop/SimulationModel/simulation_output.txt", "w");

	/* write report heading and input parameters. */
	print_headings();

	/* Initialize variables. */
	initialize_vars();

	/* Initialize event list */
	generate_init_event();


	/* Simulation */
	while (tnow < total_sim_time){

		/* Determine the next event. */
		get_next_event(&etype, &urg_type);

		/* Update time-average statistical accumulators. */
		update_stats();

//		if(num_waitlist[0] != (num_waitlist[1]+num_waitlist[2])){
//			fprintf(sim_outfile, "\n total: %d typeI: %d typeII: %d\n", num_waitlist[0], num_waitlist[1], num_waitlist[2]);
//		}

		/* Invoke the appropriate event function. */
		if(etype == PAT_ARRIVAL){patient_arrival(urg_type);}
		else if(etype == SUR_COMPLETION){surgery_completion(urg_type);}


		/* Warmup */
		if(warmup){

            /* if total number of patient in the system > ...,  the warmup is done */
			if(num_waitlist[0] > total_patient)
			{
				print_warmup_results();
				warmup = 0;
				initialize_postwarmup_vars();
			}

		}

	} //end while loop

    /* Print the results and end the simulation. */
    print_results();
    close_results_files();

	return 0;
}



/* Initialization function */
void initialize_vars(){

    /* Initialize the simulation clock */
	tnow = 0.0;

    /* Initialize the state variables */
	num_surgeon_busy = 0;
	time_last_event = 0.0;

	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		num_waitlist[i] = 0;

	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		total_patient_entered[i] = 0;

	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		total_patient_served[i] = 0;

	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		total_of_delays[i] = 0.0;

	area_num_waitlist = 0.0;
	area_server_status = 0.0;
}


/* Initialization function after warm up*/
void initialize_postwarmup_vars(){

	/* Initialize the time */
	tnow = 0.0;
	time_last_event = 0.0;

	/* Initialize the statistical counters */
	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		total_patient_entered[i] = 0;

	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		total_patient_served[i] = 0;

	for(int i = 0; i < (TYPE_OF_PAT + 1); i++)
		total_of_delays[i] = 0.0;

	area_num_waitlist = 0.0;
	area_server_status = 0.0;
}


/* Generate the very first arrival event */
void generate_init_event(){
	generate_event(PAT_ARRIVAL, TYPE1);
}


/* Generate either a new patient arrival or a surgery completion (departure) */
/* urg_type only meaningful for SUR_COMPLETION event */
void generate_event(int etype, int urg_type){

	double time, mean, hours;

	if(etype == PAT_ARRIVAL){

		urg_type = get_urg_type();

		if(urg_type == TYPE1){
			mean = 1.0/pat_arrival_rate_type1;
		} else if (urg_type == TYPE2){
			mean = 1.0/pat_arrival_rate_type2;
		}

		hours = exponential(mean, &patient_seed)/24;
		time = tnow + hours;
	}
	else if(etype == SUR_COMPLETION){
        /* if completion, does not need to overwrite urgency type (determined by arrival) */
		mean = 1.0/sur_completion_rate;
		hours = exponential(mean, &surgery_seed)/24;
		time = tnow + hours;
	}
	else{
		fprintf(sim_outfile, "Invalid Event Type Generated");
		exit(2);
	}

	/* insert event into event list */
	insert_event(time, etype, urg_type);
}



/* Get the urgency type of next patient randomly */
int get_urg_type(){

	int type;
	double type_seed;
	type_seed = uniform(&patient_type_seed);

	if(type_seed > 0.2){
		type = TYPE1;
	} else{
		type = TYPE2;
	}

	return type;
}



/* Insert event into event list (index is only meaningful for when the event is arrival) */
void insert_event(double time, int etype, int urg_type){

	event_list[num_event_list].time = time;
	event_list[num_event_list].event_type = etype;
	event_list[num_event_list].urg_type = urg_type;
	num_event_list++;

	/* exceed the maximum entries allowed, need to expand the list size */
	if (num_event_list >= EVENT_ENTRIES)
	{
		fprintf(sim_outfile, "need to expand event list size!\n");
		exit(0);
	}

}



/* Determine the next event. */
void get_next_event(int *etype, int *urg_type){

    /* get the index of the next event */
    int next_event_index = select_event();

	/* updates event type and clock */
	*etype = event_list[next_event_index].event_type;
	*urg_type = event_list[next_event_index].urg_type;
	tnow = event_list[next_event_index].time;

	/* if not last spot, reassign this spot to the last one and free up the last one */
	if (next_event_index < num_event_list-1)
	{
		event_list[next_event_index].event_type = event_list[num_event_list-1].event_type;
		event_list[next_event_index].urg_type = event_list[num_event_list-1].urg_type;
		event_list[next_event_index].time = event_list[num_event_list-1].time;
	}

	/* decrement the list by one */
	num_event_list--;
}



/* Select next event from event list */
int select_event(){

	int min_index = 0; //min index
	int min_type = event_list[0].event_type; //min type
	double min = event_list[0].time; //min time

	/* Determine the event type of the next event to occur (time of occurrence is the smallest) */
	for(int i = 0; i < num_event_list; i++)
	{
		if(event_list[i].time < min)
		{
			min = event_list[i].time;
			min_index = i;
		} else if(event_list[i].time == min){ // When there is a tie, choose the surgery completion event first
			if(event_list[i].event_type > min_type)  //this happens only when (surgery completion)1 > 0(arrival)
			{
				min = event_list[i].time;
				min_index = i;
			}
		}
	}

	return min_index;
}



/* Arrival event function. */
void patient_arrival(int urg_type){

    int curr_type = urg_type;

    /* Increment the # of patients that entered the system */
	total_patient_entered[0]++;
	total_patient_entered[curr_type]++;

    /* Schedule next arrival.
    (urg_type here is meaningless since generate_event will generate a new type randomly) */
    generate_event(PAT_ARRIVAL, urg_type);

	/* Check to see whether server is busy. */
	if(num_surgeon_busy >= NUM_SURGEON){

        /* Server is busy, increment # of patients in wait-list */
        ++num_waitlist[curr_type];
		++num_waitlist[0];

        /* Check to see whether an overflow condition exists. */
		if(num_waitlist[0] > WAIT_ENTRIES){
			fprintf(sim_outfile, "\nOverflow of the array time_arrival at ");
			fprintf(sim_outfile, "time %f \n\n", tnow);
			exit(2);
		}

		/* There is still room in the queue, store the arrival of the arriving patient at the (new) end of arrival */
		/* NOTE: wait list is updated (when event being executed)
		 * later than event list (when event being generated)*/
		wait_list[num_waitlist[0]].arrival_time = tnow;
		wait_list[num_waitlist[0]].urg_type = curr_type;

	}
	else {
		num_surgeon_busy++;
        /* Schedule a surgery completion event for the current patient.*/
        generate_event(SUR_COMPLETION, curr_type);
	}

}



/* surgery completion event function. */
void surgery_completion(int urg_type){

	double delay;
    int curr_type = urg_type;

	/* Increment the total # of patients served by the system */
	++total_patient_served[0];
	++total_patient_served[curr_type];

    /* Check to see whether the queue is empty. */
	if(num_waitlist[0] == 0)
	{
		num_surgeon_busy--;
	}

	/* Queue non-empty, decrement the # of patients in queue */
	else{

		/* Schedule next surgery completion */
		int next_patient_index = find_next_patient();
		int next_urg_type = wait_list[next_patient_index].urg_type;


        /* Compute & update the delay, generate completion event */
		delay = tnow - wait_list[next_patient_index].arrival_time;
		total_of_delays[0] += delay;
		total_of_delays[next_urg_type] += delay;
		generate_event(SUR_COMPLETION, next_urg_type);


		/* If not last spot, reassign this spot to the last one and free up the last one */
		if(next_patient_index < num_waitlist[0] - 1){
			wait_list[next_patient_index].arrival_time = wait_list[num_waitlist[0] - 1].arrival_time;
			wait_list[next_patient_index].urg_type = wait_list[num_waitlist[0] - 1].urg_type;
		}


		/* Update wait list */
        --num_waitlist[next_urg_type];
		--num_waitlist[0];
	}

}



/* Find the next patient from the wait list based on priority rules */
int find_next_patient(){
	int min_index = 0;

	if(pri_type == FIFO){
		min_index = find_next_patient_fifo();
	} else if(pri_type == FIFO_PRIORITY){
		min_index = find_next_patient_fifo_pri();
	}

	return min_index;
}



/* Find the next patient based on first in first out */
int find_next_patient_fifo(){

	int min_index = 1;
	double min_time = wait_list[1].arrival_time;

	for(int i = 1; i < num_waitlist[0]; i++)
	{
		if(wait_list[i].arrival_time < min_time) {
			min_time = wait_list[i].arrival_time;
			min_index = i;
		}
	}

	return min_index;
}



/* Find the next patient based on first in first out */
int find_next_patient_fifo_pri(){

	int min_index = 1;

	for(int i = 1; i < num_waitlist[0]; i++){
		if(higher_priority(wait_list[i], wait_list[min_index])){
			min_index = i;
		}
	}

	return min_index;
}


/* Compare two patients are determine which one has a higher priority */
bool higher_priority(patient pat1, patient pat2){
	if(pat1.urg_type == pat2.urg_type){
		return (pat1.arrival_time < pat2.arrival_time);
	} else {
	    /* Assuming higher type has higher urgency */
		return (pat1.urg_type > pat2.urg_type);
	}
}



/* write report heading and input parameters. */
void print_headings(){
	fprintf(sim_outfile, "M/M/n Surgery Wait-list Simulation \n\n") ;
	fprintf(sim_outfile, "Input Parameters:\n") ;
	fprintf(sim_outfile, "Patient Arrival Rate %11.3f\n", pat_arrival_rate);
	fprintf(sim_outfile, "Surgery Completion Rate %11.3f\n", sur_completion_rate);
    fprintf(sim_outfile, "Priority Rule: %d\n", pri_type);
	fprintf(sim_outfile, "Number of Surgeons: %d\n", NUM_SURGEON);
	fprintf(sim_outfile, "Total Simulation Time %12.f hours\n", total_sim_time);
}



void print_results(){
	fprintf(sim_outfile, "\nOutput Results:\n") ;
	fprintf(sim_outfile, "Total number of Patients Entered the System: %d\n", total_patient_entered[0]);
	fprintf(sim_outfile, "Total number of Type I Patients Entered by the System: %d\n", total_patient_entered[TYPE1]);
	fprintf(sim_outfile, "Total number of Type II Patients Entered by the System: %d\n", total_patient_entered[TYPE2]);

	fprintf(sim_outfile, "\nTotal number of Patients Served by the System: %d\n", total_patient_served[0]);
	fprintf(sim_outfile, "Total number of Type I Patients Served by the System: %d\n", total_patient_served[TYPE1]);
	fprintf(sim_outfile, "Total number of Type II Patients Served by the System: %d\n", total_patient_served[TYPE2]);

	fprintf(sim_outfile, "\nTime Statistics: \n");
	fprintf(sim_outfile, "Total and Average Wait time in Queue: Total:%11.3f, Avg:%11.3f Hours\n", total_of_delays[0], total_of_delays[0] / total_patient_served[0]);
	fprintf(sim_outfile, "Total and Average Wait time in Queue (Type I): Total:%11.3f, Avg:%11.3f Hours\n", total_of_delays[TYPE1], total_of_delays[TYPE1] / total_patient_served[TYPE1]);
	fprintf(sim_outfile, "Total and Average Wait time in Queue (Type II): Total:%11.3f, Avg:%11.3f Hours\n", total_of_delays[TYPE2], total_of_delays[TYPE2] / total_patient_served[TYPE2]);

	fprintf(sim_outfile, "\nAverage Number in Queue: %10.3f\n", area_num_waitlist / tnow);
	fprintf(sim_outfile, "Server Utilization: %15.3f\n", area_server_status / (NUM_SURGEON * tnow));

	write_event_list();
	write_wait_list();
}



void print_warmup_results(){

	fprintf(warmup_outfile, "Warm Up Period finished\n\n");
	fprintf(warmup_outfile, "Total number of Patients Entered the System: %d\n", total_patient_entered[0]);
	fprintf(warmup_outfile, "Total number of Type I Patients Entered by the System: %d\n", total_patient_entered[TYPE1]);
	fprintf(warmup_outfile, "Total number of Type II Patients Entered by the System: %d\n", total_patient_entered[TYPE2]);

	fprintf(warmup_outfile, "\nTotal number of Patients Served by the System: %d\n", total_patient_served[0]);
	fprintf(warmup_outfile, "Total number of Type I Patients Served by the System: %d\n", total_patient_served[TYPE1]);
	fprintf(warmup_outfile, "Total number of Type II Patients Served by the System: %d\n", total_patient_served[TYPE2]);

	fprintf(warmup_outfile, "\nTime Statistics:\n");
	fprintf(warmup_outfile, "Total and Average Wait time in Queue: Total:%11.3f, Avg:%11.3f Hours\n", total_of_delays[0], total_of_delays[0] / total_patient_served[0]);
	fprintf(warmup_outfile, "Total and Average Wait time in Queue (Type I): Total:%11.3f, Avg:%11.3f Hours\n", total_of_delays[TYPE1], total_of_delays[TYPE1] / total_patient_served[TYPE1]);
	fprintf(warmup_outfile, "Total and Average Wait time in Queue (Type II): Total:%11.3f, Avg:%11.3f Hours\n", total_of_delays[TYPE2], total_of_delays[TYPE2] / total_patient_served[TYPE2]);

	fprintf(warmup_outfile, "\nCurrent Wait List Length: %d\n", num_waitlist[0]);
	fprintf(warmup_outfile, "Current Event List Length: %d", num_event_list);
}


/* Update area accumulators for time-average statistics. */ 
void update_stats(){

	double time_since_last_event;

    /* Compute time since last event, and update last-event-time marker */
	time_since_last_event = tnow - time_last_event;
	time_last_event = tnow;

    /* Update area under number-in-queue function */
	area_num_waitlist += num_waitlist[0] * time_since_last_event;

    /* Update area under server-busy indicator function */
	area_server_status += num_surgeon_busy * time_since_last_event;
}



/* Write the current event list and wait list (for debug) */
void write_event_list()
{
	fprintf(sim_outfile, "\nCurrent Event List\n");
	fprintf(sim_outfile, "Number of events to be proceed: %d\n", num_event_list);
	for (int i = 0; i < num_event_list; ++i)
		fprintf(sim_outfile, "Time:%.8f Event Type:%d Urgency Type: %d\n", event_list[i].time, event_list[i].event_type, event_list[i].urg_type);
}


void write_wait_list(){
	fprintf(sim_outfile, "\nCurrent Wait List\n");
	fprintf(sim_outfile, "Number of patients in the wait list: %d\n", num_waitlist[0]);
	for(int i = 1; i <= num_waitlist[0]; i++){
	    //note: wait_list[0] is never written or used
		fprintf(sim_outfile, "Patient %d, Arrival Time: %.8f, Urgency Type: %d \n", i, wait_list[i].arrival_time, wait_list[i].urg_type);
	}
}


void close_results_files(){
	fclose(warmup_outfile);
	fclose(sim_outfile);
}