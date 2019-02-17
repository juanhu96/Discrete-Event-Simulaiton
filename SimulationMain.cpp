#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mach/mig.h>
#include "SimulationMain.h"
#include "distributions.h"

/* Output Files */
FILE *sim_outfile, *warmup_outfile;

/* Lists */
event event_list[EVENT_ENTRIES];
patient wait_list[WAIT_ENTRIES];

/* Variables */
int pri_type = FIFO_PRIORITY; //Available Rules: FIFO, FIFO_FLEX, FIFO_PRIORITY, FIFO_PRIORITY_FLEX
int num_event_list = 0;
int warmup;
int total_patient;



/* Main function. */
int main()
{
	int etype, urg_type, surgeon_index;
	bool warmup_status;

	total_sim_time = TOTAL_SIM_TIME;
    total_patient = START_SIZE;
    warmup = 1;

    patient_seed = INITIAL_PAT_SEED;
    surgery_seed = INITIAL_SUR_SEED;
    patient_type_seed = INITIAL_PAT_SEED;
    surgeon_type_seed = INITIAL_SUR_SEED;


    /* Output File */
	warmup_outfile = fopen("/Users/ArcticPirates/Desktop/SimulationModel/warmup_output.txt", "w");
	sim_outfile = fopen("/Users/ArcticPirates/Desktop/SimulationModel/simulation_output.txt", "w");

    /* Initialization */
	print_headings();
	initialize_vars();
	generate_init_event();

	/* Simulation */
	while (tnow < total_sim_time){

		/* Determine the next event: event type, urgency type, surgeon index,
		 * and whether it is in warmup period or not */
		get_next_event(&etype, &urg_type, &surgeon_index, &warmup_status);

		/* Update time-average statistical accumulators. */
		update_stats();

		/* Invoke the appropriate event function. */
		if(etype == PAT_ARRIVAL){patient_arrival(urg_type, surgeon_index);}
		else if(etype == SUR_COMPLETION){surgery_completion(urg_type, surgeon_index, warmup_status);}


		/* Warmup */
		if(warmup){

            /* if total number of patient in the system > total_patient,  the warmup is done */
			if(wait_list_matrix[0][0] > total_patient)
			{
				print_warmup_results();
				warmup = 0;
				initialize_postwarmup_vars();
			}

		} //end if loop

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

	/* Initialize the statistical counters */
	for(int i=0; i < (NUM_SURGEON + 1); i++){
		for(int j=0; j < (TYPE_OF_PAT + 1); j++){
			available_matrix[i][j] = 0;
			num_pat_entered_matrix[i][j] = 0;
			num_pat_served_matrix[i][j] = 0;
			wait_list_matrix[i][j] = 0;
			delays_matrix[i][j] = 0.0;
			num_pat_left_matrix[i][j] = 0;
		}
		surgeon_status[i] = IDLE;
	}

	area_num_waitlist = 0.0;
	area_server_status = 0.0;
}



/* Initialization Function after Warm Up*/
void initialize_postwarmup_vars(){

	/* Initialize the time */
	tnow = 0.0;
	time_last_event = 0.0;

	/* Initialize the statistical counters */
	for(int i=0; i < (NUM_SURGEON + 1); i++){
		for(int j=0; j < (TYPE_OF_PAT + 1); j++){
			num_pat_entered_matrix[i][j] = 0;
			num_pat_served_matrix[i][j] = 0;
			wait_list_matrix[i][j] = 0;
			delays_matrix[i][j] = 0.0;
			num_pat_left_matrix[i][j] = 0;
		}
	}

    /* Label the patients that are in surgery by the time warm up period ends */
    /* Otherwise these patients will be counted for patients finished surgery,
     * but not for patients entered surgery */
	label_patient();

	area_num_waitlist = 0.0;
	area_server_status = 0.0;
}



/* Label the patients that are in surgery by the time warm up period ends, do not count these */
void label_patient(){
	for(int i = 0; i < num_event_list; i++){
		if(event_list[i].event_type == SUR_COMPLETION){
			event_list[i].warmup_status = TRUE;
		}
	}
}



/* Generate the very first arrival event randomly */
void generate_init_event(){
	generate_event(PAT_ARRIVAL, TYPE1, 0);
}


/* Generate either a new patient arrival or a surgery completion (departure) */
/* urg_type parameter is only meaningful for SUR_COMPLETION event */
void generate_event(int etype, int urg_type, int surgeon_index){

	double time, mean, hours;

	if(etype == PAT_ARRIVAL){

		/* generate urgency type and surgeon index randomly */
		urg_type = get_urg_type();
		surgeon_index = get_sur_type();

		if(urg_type == TYPE1){
			mean = 1.0/pat_arrival_rate_type1;
		} else if (urg_type == TYPE2){
			mean = 1.0/pat_arrival_rate_type2;
		} else if (urg_type == TYPE3){
			mean = 1.0/pat_arrival_rate_type3;
		}

		hours = exponential(mean, &patient_seed)/24;
		time = tnow + hours;

	}
	else if(etype == SUR_COMPLETION){

        /* if completion, does not need to overwrite urgency & surgeon type (determined by arrival) */
		mean = 1.0/sur_completion_rate;
		hours = exponential(mean, &surgery_seed)/24;
		time = tnow + hours;

	}
	else{
		fprintf(sim_outfile, "Invalid Event Type Generated");
		exit(2);
	}

	/* insert event into event list */
	insert_event(time, etype, urg_type, surgeon_index);

}



/* Get the urgency type of next patient randomly */
int get_urg_type(){

	int type;
	double type_seed;
	type_seed = uniform(&patient_type_seed);

	if(type_seed <= PROB_PAT_TYPE1){
		type = TYPE1;
	} else if (type_seed <= PROB_PAT_TYPE1+PROB_PAT_TYPE2){
		type = TYPE2;
	} else{
		type = TYPE3;
	}

	return type;
}



/* Get the Surgeon type of next patient randomly */
int get_sur_type(){

	double type_seed;
	type_seed = uniform(&surgeon_type_seed);

	if(type_seed < 0.25){
		return 1;
	} else if(type_seed < 0.5){
		return 2;
	} else if(type_seed < 0.75){
		return 3;
	} else{
		return 4;
	}

}



/* Insert event into event list */
void insert_event(double time, int etype, int urg_type, int surgeon_index){

	event_list[num_event_list].time = time;
	event_list[num_event_list].event_type = etype;
	event_list[num_event_list].urg_type = urg_type;
	event_list[num_event_list].surgeon_index = surgeon_index;
	event_list[num_event_list].warmup_status = FALSE;
	num_event_list++;

	/* exceed the maximum entries allowed, need to expand the list size */
	if (num_event_list >= EVENT_ENTRIES)
	{
		fprintf(sim_outfile, "need to expand event list size!\n");
		exit(0);
	}

}



/* Determine the next event. */
void get_next_event(int *etype, int *urg_type, int *surgeon_index, bool *warmup_status){

    /* get the index of the next event */
    int next_event_index = select_event();

	/* updates event type and clock */
	tnow = event_list[next_event_index].time;
	*etype = event_list[next_event_index].event_type;
	*urg_type = event_list[next_event_index].urg_type;
	*surgeon_index = event_list[next_event_index].surgeon_index;
	*warmup_status = event_list[next_event_index].warmup_status;


	/* if not last spot, reassign this spot to the last one and free up the last one */
	if (next_event_index < num_event_list-1)
	{
		event_list[next_event_index].time = event_list[num_event_list-1].time;
		event_list[next_event_index].event_type = event_list[num_event_list-1].event_type;
		event_list[next_event_index].urg_type = event_list[num_event_list-1].urg_type;
		event_list[next_event_index].surgeon_index = event_list[num_event_list-1].surgeon_index;
		event_list[next_event_index].warmup_status = event_list[num_event_list-1].warmup_status;
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
void patient_arrival(int urg_type, int surgeon_index){

    int curr_type = urg_type;
    int curr_sur = surgeon_index;

    /* Increment the # of patients that entered the system */
    ++num_pat_entered_matrix[0][0];
	++num_pat_entered_matrix[curr_sur][0];
	++num_pat_entered_matrix[0][curr_type];
	++num_pat_entered_matrix[curr_sur][curr_type];

    /* Schedule next arrival.
    (parameter here is meaningless since generate_event will generate a new type randomly) */
    generate_event(PAT_ARRIVAL, urg_type, curr_sur);

    // next_surgeon = NUM_SURGEON + 1 if all busy,
    // otherwise returns the responsible surgeon (not necessary the initial surgeon)
    int next_surgeon = surgery_busy(curr_type, surgeon_index);

	/* Check to see whether server is busy. */
	/* Either all server busy, or there are other patients in the waitlist waiting for the idle surgeon */
	/* (Otherwise if a patient enters the queue exactly when a surgeon is idle, he will jump into the surgery) */
	if(next_surgeon > NUM_SURGEON || wait_list_matrix[next_surgeon][0] > 0){

	    /* Enter wait list */

        /* Server is busy, increment # of patients in wait-list */
		++wait_list_matrix[0][0];
		++wait_list_matrix[curr_sur][0];
        ++wait_list_matrix[0][curr_type];
		++wait_list_matrix[curr_sur][curr_type];

        /* Check to see whether an overflow condition exists. */
		if(wait_list_matrix[0][0] > WAIT_ENTRIES){
			fprintf(sim_outfile, "\nOverflow of the array time_arrival at ");
			fprintf(sim_outfile, "time %f \n\n", tnow);
			exit(2);
		}

		/* There is still room in the queue, store the arrival of the arriving patient at the (new) end of arrival */
		/* NOTE: wait list is updated (when event being executed)
		 * later than event list (when event being generated)*/
		wait_list[wait_list_matrix[0][0]].arrival_time = tnow;
		wait_list[wait_list_matrix[0][0]].urg_type = curr_type;
		wait_list[wait_list_matrix[0][0]].surgeon_index = curr_sur;

	}
	else {

	    /* Served by the next_surgeon determined by surgery_busy() */

	    /* Update surgeon status */
		num_surgeon_busy++;
		surgeon_status[next_surgeon] = BUSY;

        /* Schedule a surgery completion event for the current patient. */
        /* NOTE: Patient finished waiting, and is under surgery now. */
        generate_event(SUR_COMPLETION, curr_type, next_surgeon);

        /* Update statistics */
		++num_pat_served_matrix[0][0];
		++num_pat_served_matrix[curr_sur][0];
		++num_pat_served_matrix[0][curr_type];
        ++num_pat_served_matrix[curr_sur][curr_type];
	}

}



/* Return the responsible surgeon index, return (n + 1) if unavailable */
int surgery_busy(int type, int surgeon_index){

	/* CASE1: All surgeons are busy */
	if(num_surgeon_busy >= NUM_SURGEON){
		return NUM_SURGEON + 1;
	}

	/* CASE2: Initial surgeon available */
	if(surgeon_status[surgeon_index] == IDLE){
		return surgeon_index;
	}

	/* CASE3: Look for other idle surgeon */
	for(int i = 1; i < NUM_SURGEON + 1; i++){
		if(surgeon_status[i] == IDLE && available_matrix[i][type]== 1){
			// surgeon is idle and is available for the type
			return i;
		}
	}

	/* No available surgeon right now */
	return NUM_SURGEON + 1;
}




/* surgery completion event function. */
void surgery_completion(int urg_type, int surgeon_index, bool warmup_status){

	double delay;
    int curr_type = urg_type;
    int curr_sur = surgeon_index;

	/* Increment the total # of patients served by the system
	 * (do not count patients that entered surgery before warmup period ends)*/
	if(warmup_status == FALSE) {
		++num_pat_left_matrix[0][0];
		++num_pat_left_matrix[curr_sur][0];
		++num_pat_left_matrix[0][curr_type];
		++num_pat_left_matrix[curr_sur][curr_type];
	}

	/* Current surgeon finished the surgery and become idle */
	surgeon_status[curr_sur] = IDLE;
	num_surgeon_busy--;

    /* Check to see whether the queue is empty. */
	if(wait_list_matrix[0][0] < -WAIT_ENTRIES)
	{
		num_surgeon_busy--;
	}

	/* Queue non-empty, decrement the # of patients in queue */
	else{

		/* Schedule next surgery completion from the wait list */
		// NOTE: When finding next patient, consider those whose initial surgeon is idle first

		// surgery contains: patient_index, urgency type, surgeon
		surgery next_surgery = find_next_surgery();
		int next_patient_index = next_surgery.pat_index;
		int next_urg_type = next_surgery.urg_type;
		int next_sur_type = next_surgery.surgeon_index; // surgeon assigned to
		int init_sur_type = wait_list[next_patient_index].surgeon_index; // initial surgeon

		/* If unable to find a surgery */
		if(next_sur_type == 0){
			return;
		}

        /* Compute and update the delay statistics, generate completion event */
        delay = tnow - wait_list[next_patient_index].arrival_time;
		delays_matrix[0][0] += delay;
		delays_matrix[init_sur_type][0] += delay;
		delays_matrix[0][next_urg_type] += delay;
		delays_matrix[init_sur_type][next_urg_type] += delay;

		// Note: Patient left their initial wait list and served by idle(current) surgeon
		surgeon_status[next_sur_type] = BUSY;
		num_surgeon_busy++;
		generate_event(SUR_COMPLETION, next_urg_type, next_sur_type);

		/* Update statistics */
		++num_pat_served_matrix[0][0];
		++num_pat_served_matrix[init_sur_type][0];
		++num_pat_served_matrix[0][next_urg_type];
		++num_pat_served_matrix[init_sur_type][next_urg_type];


		/* If not last spot, reassign this spot to the last one and free up the last one */
		if(next_patient_index < wait_list_matrix[0][0]){

			wait_list[next_patient_index].arrival_time = wait_list[wait_list_matrix[0][0]].arrival_time;
			wait_list[next_patient_index].urg_type = wait_list[wait_list_matrix[0][0]].urg_type;
			wait_list[next_patient_index].surgeon_index = wait_list[wait_list_matrix[0][0]].surgeon_index;

		}


		/* Update wait list */
		--wait_list_matrix[0][0];
		--wait_list_matrix[init_sur_type][0];
		--wait_list_matrix[0][next_urg_type];
		--wait_list_matrix[init_sur_type][next_urg_type];

	} // end if loop

}



/* Find the next surgery based on the rule given by pri_type */
surgery find_next_surgery(){

    surgery next_surgery;

    if(pri_type == FIFO)
    {
        next_surgery = find_next_surgery_fifo();
    }
    else if (pri_type == FIFO_FLEX)
    {
        next_surgery = find_next_surgery_fifo_flex();
    }
    else if(pri_type == FIFO_PRIORITY)
    {
        next_surgery = find_next_surgery_fifo_pri();
    }
    else if(pri_type == FIFO_PRIORITY_FLEX)
    {
    	next_surgery = find_next_surgery_fifo_pri_flex();
    }

    return next_surgery;
}



/* First in first out for all patients (No Flexibility) */
surgery find_next_surgery_fifo(){

    /* Initialization */
    surgery next_surgery;
    next_surgery.pat_index = 0;
    next_surgery.urg_type = 0;
    next_surgery.surgeon_index = 0;

    int min_index = 1;
    int urg_type = 1;
    int surgeon_index = 0;
	double min_time = MAX_TIME;


    for(int i = 1; i <= wait_list_matrix[0][0]; i++)
    {
        if(surgeon_status[wait_list[i].surgeon_index] == IDLE && wait_list[i].arrival_time < min_time){
        	min_time = wait_list[i].arrival_time;
        	urg_type = wait_list[i].urg_type;
        	surgeon_index = wait_list[i].surgeon_index;
        	min_index = i;
        }
    }

    next_surgery.pat_index = min_index;
    next_surgery.urg_type = urg_type;
    next_surgery.surgeon_index = surgeon_index;

    return next_surgery;
}



/* First in first out with priority (No Flexibility) */
surgery find_next_surgery_fifo_pri(){

    /* Initialization */
    surgery next_surgery;
    next_surgery.pat_index = 0;
    next_surgery.urg_type = 0;
    next_surgery.surgeon_index = 0;

	int min_index = 1;
	int urg_type = 1;
	int surgeon_index = 0;

    for(int i = 1; i <= wait_list_matrix[0][0]; i++)
    {
        if(surgeon_status[wait_list[i].surgeon_index] == IDLE && higher_priority(wait_list[i], wait_list[min_index])){
            urg_type = wait_list[i].urg_type;
            surgeon_index = wait_list[i].surgeon_index;
            min_index = i;
        }
    }

    next_surgery.pat_index = min_index;
    next_surgery.urg_type = urg_type;
    next_surgery.surgeon_index = surgeon_index;

    return next_surgery;
}



/* First in first out for all patients (Flexible) */
surgery find_next_surgery_fifo_flex(){

    surgery next_surgery;

    /* Try to match their initial surgeon first */
    next_surgery = find_next_surgery_fifo();

    /* If unsuccessful, find other idle surgeon */
    if(next_surgery.surgeon_index == 0)
    {

        int min_index = 1;
        int urg_type = 1;
        int surgeon_index = 0;
        double min_time = MAX_TIME;

        for(int i = 1; i <= wait_list_matrix[0][0]; i++)
        {
            for(int j = 1; j < NUM_SURGEON; j++)
            {
                /* Surgeon is able and available to serve the patient */
                if(available_matrix[j][wait_list[i].urg_type] && surgeon_status[j] == IDLE
                && wait_list[i].arrival_time < min_time)
                {
                    min_time = wait_list[i].arrival_time;
                    urg_type = wait_list[i].urg_type;
                    surgeon_index = wait_list[i].surgeon_index;
                    min_index = i;
                }
            }

        }

        next_surgery.pat_index = min_index;
        next_surgery.urg_type = urg_type;
        next_surgery.surgeon_index = surgeon_index;
    }

    return next_surgery;
}



/* First in first out with priority (Flexible) */
/* This should produce same result as No Flexibility if available_matrix has a entry 0
 * (all surgeons cannot operate patients from other's list) */
surgery find_next_surgery_fifo_pri_flex(){

	surgery next_surgery;

	/* Match to their initial surgeon first */
	next_surgery = find_next_surgery_fifo_pri();

    /* If unsuccessful, find other idle surgeon */
	if(next_surgery.surgeon_index == 0)
	{

        int min_index = 1;
        int urg_type = 1;
        int surgeon_index = 0;

		for(int i = 1; i <= wait_list_matrix[0][0]; i++)
		{
			for(int j = 1; j<= NUM_SURGEON; j++)
			{
				if(available_matrix[j][wait_list[i].urg_type] && surgeon_status[j] == IDLE
				&& higher_priority(wait_list[i], wait_list[min_index]))
				{
					urg_type = wait_list[i].urg_type;
					surgeon_index = wait_list[i].surgeon_index;
					min_index = i;
				}
			}
		}

        next_surgery.pat_index = min_index;
        next_surgery.urg_type = urg_type;
        next_surgery.surgeon_index = surgeon_index;
	}

	return next_surgery;
}



/* Compare two patients are determine which one has a higher priority */
bool higher_priority(patient pat1, patient pat2){
    if(pat1.urg_type == pat2.urg_type){
        return (pat1.arrival_time <= pat2.arrival_time);
    } else {
        /* Assuming higher type has higher urgency */
        return (pat1.urg_type >= pat2.urg_type);
    }
}



/* write report heading and input parameters. */
void print_headings(){
	fprintf(sim_outfile, "M/M/n Surgery Wait-list Simulation \n\n") ;
	fprintf(sim_outfile, "Input Parameters:\n") ;
    fprintf(sim_outfile, "Priority Rule: %d\n", pri_type);
	fprintf(sim_outfile, "Number of Surgeons: %d, Completion Rate: %11.3f\n", NUM_SURGEON, sur_completion_rate);
	fprintf(sim_outfile, "Type of Patients: %d, Arrival Rate: %11.3f, %11.3f\n", TYPE_OF_PAT, pat_arrival_rate_type1, pat_arrival_rate_type2);
	fprintf(sim_outfile, "Total Simulation Time %12.f hours\n", total_sim_time);
}



/* write main results */
void print_results(){

	fprintf(sim_outfile, "\nOutput Results:\n") ;

	fprintf(sim_outfile, "================================================================================");

	fprintf(sim_outfile, "\nTotal Number of Patients Entered the System: %d %d %d %d\n",
			num_pat_entered_matrix[0][0], num_pat_entered_matrix[0][1], num_pat_entered_matrix[0][2], num_pat_entered_matrix[0][3]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(sim_outfile, "Total number of Type %d Patients Entered Surgeon %d's Wait List: %d\n",
					j, i,num_pat_entered_matrix[i][j]);
		}
		fprintf(sim_outfile, "\n");
	}

	fprintf(sim_outfile, "================================================================================");

	fprintf(sim_outfile, "\nTotal Number of Patients who Finished Queueing: %d %d %d %d\n",
			num_pat_served_matrix[0][0], num_pat_served_matrix[0][1], num_pat_served_matrix[0][2], num_pat_served_matrix[0][3]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(sim_outfile, "Total Number of Type %d Patients who Left Surgeon %d's Wait List: %d\n",
					j, i, num_pat_served_matrix[i][j]);
		}
		fprintf(sim_outfile, "\n");
	}

	fprintf(sim_outfile, "================================================================================");

	fprintf(sim_outfile, "\nTotal Number of Patients who Finished Surgery: %d %d %d %d\n",
			num_pat_left_matrix[0][0], num_pat_left_matrix[0][1], num_pat_left_matrix[0][2], num_pat_left_matrix[0][3]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(sim_outfile, "Total Number of Type %d Patients Finished by Surgeon %d: %d\n",
					j, i, num_pat_left_matrix[i][j]);
		}
		fprintf(sim_outfile, "\n");
	}

	fprintf(sim_outfile, "================================================================================");

	fprintf(sim_outfile, "\nTime Statistics: \n");

	fprintf(sim_outfile, "================================================================================");

	fprintf(sim_outfile, "\nTotal and Average Wait time in Queue: Total:%11.3f, Avg:%11.3f Hours\n",
			delays_matrix[0][0], delays_matrix[0][0] / num_pat_left_matrix[0][0]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(sim_outfile, "Total and Average Wait Time of Patient Type %d in Surgeon %d's Wait List: Total:%11.3f, Avg:%11.3f Hours\n",
					j, i, delays_matrix[i][j], delays_matrix[i][j]/num_pat_left_matrix[i][j]);
		}
		fprintf(sim_outfile, "\n");
	}

	fprintf(sim_outfile, "================================================================================");

	fprintf(sim_outfile, "\nAverage Number in Queue: %10.3f\n", area_num_waitlist / tnow);
	fprintf(sim_outfile, "Server Utilization: %15.3f\n", area_server_status / (NUM_SURGEON * tnow));

	fprintf(sim_outfile, "================================================================================");

	write_event_list();

	fprintf(sim_outfile, "================================================================================");

	write_wait_list();
}



/* write warm up results */
void print_warmup_results(){

	fprintf(warmup_outfile, "Warm Up Period finished\n\n");
	fprintf(warmup_outfile, "\nOutput Results:\n") ;
	fprintf(warmup_outfile, "Total Number of Patients Entered the System: %d\n", num_pat_entered_matrix[0][0]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(warmup_outfile, "Total number of Type %d Patients Entered Surgeon %d's Wait List: %d\n",
					j, i,num_pat_entered_matrix[i][j]);
		}
	}

	fprintf(warmup_outfile, "\nTotal number of Patients Served by the System: %d\n", num_pat_served_matrix[0][0]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(warmup_outfile, "Total Number of Type %d Patients Served by Surgeon %d: %d\n",
					j, i,num_pat_served_matrix[i][j]);
		}
	}

	fprintf(warmup_outfile, "\nTime Statistics: \n");

	fprintf(warmup_outfile, "Total and Average Wait time in Queue: Total:%11.3f, Avg:%11.3f Hours\n",
			delays_matrix[0][0], delays_matrix[0][0] / num_pat_left_matrix[0][0]);

	for(int i = 1; i < NUM_SURGEON + 1; i++)
	{
		for(int j = 1; j < TYPE_OF_PAT + 1; j++)
		{
			fprintf(warmup_outfile, "Total and Average Wait Time of Patient Type %d in Surgeon %d's Wait List: Total:%11.3f, Avg:%11.3f Hours\n",
					j, i, delays_matrix[i][j], delays_matrix[i][j]/num_pat_left_matrix[i][j]);
		}
	}

	fprintf(warmup_outfile, "\nCurrent Wait List Length: %d\n", wait_list_matrix[0][0]);
	fprintf(warmup_outfile, "Current Event List Length: %d", num_event_list);

}



/* Update area accumulators for time-average statistics. */ 
void update_stats(){

	double time_since_last_event;

    /* Compute time since last event, and update last-event-time marker */
	time_since_last_event = tnow - time_last_event;
	time_last_event = tnow;

    /* Update area under number-in-queue function */
	area_num_waitlist += wait_list_matrix[0][0] * time_since_last_event;

    /* Update area under server-busy indicator function */
	area_server_status += num_surgeon_busy * time_since_last_event;
}



/* Write the current event list */
void write_event_list()
{

	fprintf(sim_outfile, "\nCurrent Event List\n");
	fprintf(sim_outfile, "Number of events to be proceed: %d\n", num_event_list);

	for (int i = 0; i < num_event_list; ++i)
		fprintf(sim_outfile, "Time:%.8f Event Type:%d Urgency Type: %d Surgeon Index: %d\n", event_list[i].time, event_list[i].event_type, event_list[i].urg_type, event_list[i].surgeon_index);
}



/* Write the current wait list */
void write_wait_list(){

	fprintf(sim_outfile, "\nCurrent Wait List\n");
	fprintf(sim_outfile, "Number of patients in the wait list: %d\n", wait_list_matrix[0][0]);

	for(int i = 1; i <= TYPE_OF_PAT; i++)
	{
		fprintf(sim_outfile, "Number of type %d patient in wait list: %d\n", i, wait_list_matrix[0][i]);
	}

	for(int i = 1; i <= NUM_SURGEON; i++)
	{
		fprintf(sim_outfile, "Length of Surgeon %d's List: %d\n", i, wait_list_matrix[i][0]);
	}

	for(int i = 1; i <= wait_list_matrix[0][0]; i++)
	{
	    //note: wait_list[0] is never written or used
		fprintf(sim_outfile, "Patient %d, Arrival Time: %.8f, Urgency Type: %d, Initial Surgeon: %d\n", i, wait_list[i].arrival_time, wait_list[i].urg_type, wait_list[i].surgeon_index);
	}
}



/* Finish writing the output files */
void close_results_files(){
	fclose(warmup_outfile);
	fclose(sim_outfile);
}