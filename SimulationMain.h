// Created by Hu Jingyuan on 2018-12-21.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


// General Settings
#define INITIAL_SEED 15232
#define EVENT_ENTRIES 10
#define WAIT_ENTRIES 50000
#define START_SIZE 100 // depends on the actual data
#define TOTAL_SIM_TIME 1000 // total simulation time (hours)


// Event
#define PAT_ARRIVAL 0 // index for event: patient arrival
#define SUR_COMPLETION 1 // index for event: surgery completion

// Patients
#define TYPE_OF_PAT 2
#define TYPE1 1
#define TYPE2 2
#define PROB_TYPE1 0.8 // the probability of type 1 patient
#define PROB_TYPE2 0.2 // the probability of type 2 patient

// Surgeon
#define NUM_SURGEON 1 // number of surgeon


// Structures & Rules
#define FIFO 0
#define FIFO_PRIORITY 1



/* Global Variables */
int num_surgeon_busy;
int total_patient_entered[TYPE_OF_PAT+1], total_patient_served[TYPE_OF_PAT+1], num_waitlist[TYPE_OF_PAT+1];

double total_of_delays[TYPE_OF_PAT+1];
double tnow, time_last_event, total_sim_time, area_num_waitlist, area_server_status;

/* Rates */
double pat_arrival_rate = 1.6;
double sur_completion_rate = 0.4;
double pat_arrival_rate_type1 = pat_arrival_rate * PROB_TYPE1;
double pat_arrival_rate_type2 = pat_arrival_rate * PROB_TYPE2;

/* Seed */
long int patient_seed, surgery_seed, patient_type_seed;

int priority_set[] = {TYPE2};


/* Functions */
void initialize_vars(void);
void initialize_postwarmup_vars(void);
void get_next_event (int *etype, int *index);

int select_event(void);
int find_next_patient(void);
int find_next_patient_fifo(void);
int find_next_patient_fifo_pri(void);

bool higher_priority(struct patient, struct patient);
void patient_arrival (int);
void surgery_completion(int);

void print_headings(void);
void print_results(void);
void print_warmup_results(void);
void update_stats(void);

void generate_init_event(void);
void generate_event(int etype, int);
void insert_event(double, int, int);
int get_urg_type(void);


// Output functions
void close_results_files(void);
void write_event_list(void);
void write_wait_list(void);



/* Event */
typedef struct {
    double time;
    int event_type;
    int urg_type;
} event;

/* Patient */
typedef struct patient{
    int urg_type;
    double arrival_time;
} patient;

/* Statistics */
typedef struct {
    double avg_wait_time;
    double total_wait_time;
} statistics;

/* Surgeon */
//TODO: Subject to changes
typedef struct {

    // surgeon's own wait list
    patient *wait_list[WAIT_ENTRIES];

    // other wait lists that are accessible
    int accessible_wait_list;

    // list of 0, 1;
    // Indicates whether the surgeon is able to serve type i patient from other wait list
    int accessible_urg_type[TYPE_OF_PAT - 1];
} surgeon;

