#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "SimulationMain.h"
#include "distributions.h"

FILE *sim_outfile, *warmup_outfile;

event event_list[EVENT_ENTRIES];
patient wait_list[WAIT_ENTRIES];

int pri_type = FIFO;
int num_event_list = 0;
int warmup;
int total_patient; // if num_wait > total_patient, the warmup is done


/* Main function. */
int main()
{
	int etype, urg_type;

	total_sim_time = TOTAL_SIM_TIME;
    total_patient = START_SIZE;
    warmup = 1; // need to warmup first

    patient_seed = INITIAL_SEED + 1000 ;
    surgery_seed = INITIAL_SEED + 1000 ;
    patient_type_seed = INITIAL_SEED + 1000;

	/* output files. */
	warmup_outfile = fopen("/Users/ArcticPirates/Desktop/Process Flexibility/Simulation Code/Textbook Example/warmup_output.txt", "w");
	sim_outfile = fopen("/Users/ArcticPirates/Desktop/Process Flexibility/Simulation Code/Textbook Example/simulation_output.txt", "w");

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

		/* Invoke the appropriate event function. */
		if(etype == PAT_ARRIVAL){patient_arrival(urg_type);}
		else if(etype == SUR_COMPLETION){surgery_completion(urg_type);}


		/* Warmup */
		if(warmup){

			if(num_waitlist > total_patient) // done with warmup
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
	num_waitlist = 0;
	num_waitlist_type1 = 0;
	num_waitlist_type2 = 0;
	time_last_event = 0.0;


    /* Initialize the statistical counters */
	total_num_patient_entered = 0;
	total_num_type1_pat_entered = 0;
	total_num_type2_pat_entered = 0;

	total_num_patient_served = 0;
	total_num_type1_pat_served = 0;
	total_num_type2_pat_served = 0;

	total_of_delays = 0.0;
	total_of_delays_type1 = 0.0;
	total_of_delays_type2 = 0.0;

	area_num_waitlist = 0.0;
	area_server_status = 0.0;
}


/* Initialization function after warm up*/
void initialize_postwarmup_vars(){

	/* Initialize the time */
	tnow = 0.0;
	time_last_event = 0.0;

	/* Initialize the statistical counters */
	total_num_patient_entered = 0;
	total_num_type1_pat_entered = 0;
	total_num_type2_pat_entered = 0;

	total_num_patient_served = 0;
	total_num_type1_pat_served = 0;
	total_num_type2_pat_served = 0;

	total_of_delays = 0.0;
	total_of_delays_type1 = 0.0;
	total_of_delays_type2 = 0.0;

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

		urg_type = get_urg_type(); // need to modify

		if(urg_type == TYPE1){
			mean = 1.0/pat_arrival_rate_type1;
		} else if (urg_type == TYPE2){
			mean = 1.0/pat_arrival_rate_type2;
		}
		hours = exponential(mean, &patient_seed)/24;
		time = tnow + hours;
	}
	else if(etype == SUR_COMPLETION){
		mean = 1.0/sur_completion_rate;
		hours = exponential(mean, &surgery_seed)/24;
		time = tnow + hours;
		// if completion, does not need to overwrite urgency type (determined by arrival)
	}
	else{
		fprintf(sim_outfile, "Invalid Event Type Generated");
		exit(2);
	}

	//insert event into event list
	insert_event(time, etype, urg_type);
}


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

	// exceed the maximum entries allowed, need to expand the list size
	if (num_event_list >= EVENT_ENTRIES)
	{
		fprintf(sim_outfile, "need to expand event list size!\n");
		exit(0);
	}

}



/* Determine the next event. */
void get_next_event(int *etype, int *urg_type){

    //get the index of the next event
    int next_event_index = select_event();

	//updates event type and clock
	*etype = event_list[next_event_index].event_type;
	*urg_type = event_list[next_event_index].urg_type;
	tnow = event_list[next_event_index].time;


	//if not last spot, reassign this spot to the last one and free up the last one
	if (next_event_index < num_event_list-1)
	{
		event_list[next_event_index].event_type = event_list[num_event_list-1].event_type;
		event_list[next_event_index].urg_type = event_list[num_event_list-1].urg_type;
		event_list[next_event_index].time = event_list[num_event_list-1].time;
	}

	//decrement the list by one
	num_event_list--;
}


/* Select event based on certain priority rule, return the corresponding index */
int select_event(){

	int min_index = 0;

	if(pri_type == FIFO){
		min_index = select_event_fifo();
	} else{ //for now there's only two cases (either FIFO or non-preemptive priority)
		min_index = select_event_nonpre_pri();
	}

    return min_index;
}

/* Select event based on First-In-First-Out */
int select_event_fifo(){

	int min_index = 0; //min index
	int min_type = event_list[0].event_type; //min type
	double min = event_list[0].time; //min time

	/* Determine the event type of the next event to occur (time of occurrence is the smallest)
        *in case of ties, the lowest-numbered event type is chosen */
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

/* Select event based on non-preemptive priority rule, subject to changes (set of urgency types) */
int select_event_nonpre_pri(){

	int min_index = 0;
	int min_event_type = event_list[0].event_type;
	double min = event_list[0].time;

	// find global min
	for(int i = 0; i < num_event_list; i++)
	{
		if(event_list[i].time < min)
		{
			min = event_list[i].time;
			min_index = i;
		} else if(event_list[i].time == min){ // When there is a tie, choose the surgery completion event first
			if(event_list[i].event_type > min_event_type)  //this happens only when (surgery completion)1 > 0(arrival)
			{
				min = event_list[i].time;
				min_index = i;
			}
		}
	}

	// If the first event is priority patient arrival or surgery completion
	if(priority(event_list[min_index].urg_type) || event_list[min_index].event_type == 1){
		return min_index;
	}
	else{

		double min_pri = total_sim_time; //set enough large

		for(int i = 0; i < num_event_list; i++)
		{
			if(event_list[i].time < min_pri &&priority(event_list[i].urg_type)
			&& event_list[i].event_type == PAT_ARRIVAL)
			{
				min_pri = event_list[i].time;
				min_index = i;
			}
		}

		return min_index;
	}

}




/* Arrival event function. */
void patient_arrival(int urg_type){

    int curr_type = urg_type;

    /* Increment the total # of patients that entered the system */
    ++total_num_patient_entered;

    /* Schedule next arrival. */
    //urg_type here is meaningless since generate_event will generate a new type randomly
    generate_event(PAT_ARRIVAL, urg_type);


	if(curr_type == TYPE1){
	    ++total_num_type1_pat_entered;
	} else if (curr_type == TYPE2){
        ++total_num_type2_pat_entered;
	}

	/* Check to see whether server is busy. */
	if(num_surgeon_busy >= NUM_SURGEON){

        /* Server is busy, increment # of patients in wait-list */
		++num_waitlist;
		if(curr_type == TYPE1){
			++num_waitlist_type1;
		} else if (curr_type == TYPE2){
			++num_waitlist_type2;
		}

        /* Check to see whether an overflow condition exists. (for debug) */
		if(num_waitlist > WAIT_ENTRIES){
			fprintf(sim_outfile, "\nOverflow of the array time_arrival at ");
			fprintf(sim_outfile, "time %f \n\n", tnow);
			exit(2);
		}

		/* There is still room in the queue, store the arrival of the arriving patient at the (new) end of arrival */
		/* NOTE: wait list is updated (when event being executed)
		 * later than event list (when event being generated)*/
		wait_list[num_waitlist].arrival_time = tnow;
		wait_list[num_waitlist].urg_type = curr_type;

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
	total_num_patient_served++;

	if(curr_type == TYPE1){
		total_num_type1_pat_served++;
	} else if(curr_type == TYPE2){
		total_num_type2_pat_served++;
	}

    /* Check to see whether the queue is empty. */
	if(num_waitlist == 0)
	{
		num_surgeon_busy--;
	}
	else{

        /* Queue non-empty, decrement the # of patients in queue */
		--num_waitlist;
		if(curr_type == TYPE1){
			--num_waitlist_type1;
		} else if (curr_type == TYPE2){
			--num_waitlist_type2;
		}

		//the one that leaves must be the first patient that enters (later on might not be true)
        /* Compute the delay of the patient who is beginning surgery and update the total delay accumulator*/
		delay = tnow - wait_list[1].arrival_time;
		total_of_delays += delay;


		if(wait_list[1].urg_type == TYPE1){
			total_of_delays_type1 += delay;
		}
		else if(wait_list[1].urg_type == TYPE2){
			total_of_delays_type2 += delay;
		}


        /* Schedule next surgery completion. */
		generate_event(SUR_COMPLETION, wait_list[1].urg_type);

		/* Move each patient in queue (if any) up one place */
		for(int i = 1; i <= num_waitlist; ++i){
			wait_list[i].arrival_time = wait_list[i+1].arrival_time;
			wait_list[i].urg_type = wait_list[i+1].urg_type;
		}
	}
}


bool priority(int type){
	for(int i = 0; i < sizeof(priority_set)/sizeof(priority_set[0]); i++){
		if(priority_set[i] == type ){return 1;}
		return 0;
	}
}

/* write report heading and input parameters. */
void print_headings(){
	fprintf(sim_outfile, "M/M/1 Surgery Wait-list Simulation \n\n") ;
	fprintf(sim_outfile, "Input Parameters:\n") ;
	fprintf(sim_outfile, "Patient Arrival Rate %11.3f\n", pat_arrival_rate);
	fprintf(sim_outfile, "Surgery Completion Rate %11.3f\n", sur_completion_rate);
	fprintf(sim_outfile, "Total Simulation Time %12.f minutes\n", total_sim_time);
}


/* print_results generator function. */
void print_results(){
	fprintf(sim_outfile, "\nOutput Results:\n") ;
	fprintf(sim_outfile, "Total number of Patients Entered the System: %d\n", total_num_patient_entered);
	fprintf(sim_outfile, "Total number of Type I Patients Entered by the System: %d\n", total_num_type1_pat_entered);
	fprintf(sim_outfile, "Total number of Type II Patients Entered by the System: %d\n", total_num_type2_pat_entered);

	fprintf(sim_outfile, "\nTotal number of Patients Served by the System: %d\n", total_num_patient_served);
	fprintf(sim_outfile, "Total number of Type I Patients Served by the System: %d\n", total_num_type1_pat_served);
	fprintf(sim_outfile, "Total number of Type II Patients Served by the System: %d\n", total_num_type2_pat_served);

	fprintf(sim_outfile, "\nTime Statistics: \n");
	fprintf(sim_outfile, "Total and Average Wait time in Queue: Total:%11.3f, Avg:%11.3f Minutes\n", total_of_delays, total_of_delays / total_num_patient_served);
	fprintf(sim_outfile, "Total and Average Wait time in Queue (Type I): Total:%11.3f, Avg:%11.3f Minutes\n", total_of_delays_type1, total_of_delays_type1 / total_num_type1_pat_served);
	fprintf(sim_outfile, "Total and Average Wait time in Queue (Type II): Total:%11.3f, Avg:%11.3f Minutes\n", total_of_delays_type2, total_of_delays_type2 / total_num_type2_pat_served);

	fprintf(sim_outfile, "\nAverage Number in Queue: %10.3f\n", area_num_waitlist / tnow);
	fprintf(sim_outfile, "Server Utilization: %15.3f\n", area_server_status / tnow);

	write_event_list();
	write_wait_list();
}


void print_warmup_results(){

	fprintf(warmup_outfile, "Warm Up Period finished\n\n");
	fprintf(warmup_outfile, "Total number of Patients Entered the System: %d\n", total_num_patient_entered);
	fprintf(warmup_outfile, "Total number of Type I Patients Entered by the System: %d\n", total_num_type1_pat_entered);
	fprintf(warmup_outfile, "Total number of Type II Patients Entered by the System: %d\n", total_num_type2_pat_entered);

	fprintf(warmup_outfile, "\nTotal number of Patients Served by the System: %d\n", total_num_patient_served);
	fprintf(warmup_outfile, "Total number of Type I Patients Served by the System: %d\n", total_num_type1_pat_served);
	fprintf(warmup_outfile, "Total number of Type II Patients Served by the System: %d\n", total_num_type2_pat_served);

	fprintf(warmup_outfile, "\nTime Statistics:\n");
	fprintf(warmup_outfile, "Total and Average Wait time in Queue: Total:%11.3f, Avg:%11.3f Minutes\n", total_of_delays, total_of_delays / total_num_patient_served);
	fprintf(warmup_outfile, "Total and Average Wait time in Queue (Type I): Total:%11.3f, Avg:%11.3f Minutes\n", total_of_delays_type1, total_of_delays_type1 / total_num_type1_pat_served);
	fprintf(warmup_outfile, "Total and Average Wait time in Queue (Type II): Total:%11.3f, Avg:%11.3f Minutes\n", total_of_delays_type2, total_of_delays_type2 / total_num_type2_pat_served);

	fprintf(warmup_outfile, "\nCurrent Wait List Length: %d\n", num_waitlist);
	fprintf(warmup_outfile, "Current Event List Length: %d", num_event_list);
}


/* Update area accumulators for time-average statistics. */ 
void update_stats(){

	double time_since_last_event;

    /* Compute time since last event, and update last-event-time marker */
	time_since_last_event = tnow - time_last_event;
	time_last_event = tnow;

    /* Update area under number-in-queue function */
	area_num_waitlist += num_waitlist * time_since_last_event;

    /* Update area under server-busy indicator function */
	area_server_status += num_surgeon_busy * time_since_last_event;
}



/* Write the current event list and wait list (for debug) */
void write_event_list()
{
	fprintf(sim_outfile, "\nCurrent Event List\n");
	fprintf(sim_outfile, "Number of events to be proceed: %d\n", num_event_list);
	for (int i = 0; i < num_event_list; ++i)
		fprintf(sim_outfile, "Time:%.8f Event Type:%d\n", event_list[i].time, event_list[i].event_type);
}


void write_wait_list(){
	fprintf(sim_outfile, "\nCurrent Wait List\n");
	for(int i = 1; i <= num_waitlist; i++){
	    //note: wait_list[0] is never written or used
		fprintf(sim_outfile, "Patient %d, Arrival Time: %.8f, Urgency Type: %d \n", i, wait_list[i].arrival_time, wait_list[i].urg_type);
	}
}


void close_results_files(){
	fclose(warmup_outfile);
	fclose(sim_outfile);
}