/* Created by Jingyuan Hu */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


/* MACROS */
#define INITIAL_PAT_SEED 15232
#define INITIAL_SUR_SEED 10086
#define EVENT_ENTRIES 10
#define WAIT_ENTRIES 50000
#define START_SIZE 100 // depends on the actual data
#define TOTAL_SIM_TIME 1000 // total simulation time (hours)
#define MAX_TIME TOTAL_SIM_TIME * 10


/* Event index */
#define PAT_ARRIVAL 0 // index for event: patient arrival
#define SUR_COMPLETION 1 // index for event: surgery completion


/* Patients */
#define TYPE_OF_PAT 3
#define TYPE1 1
#define TYPE2 2
#define TYPE3 3
#define PROB_PAT_TYPE1 0.9
#define PROB_PAT_TYPE2 0.08
#define PROB_PAT_TYPE3 0.02


/* Surgeon */
#define NUM_SURGEON 4 // number of surgeon
#define IDLE 0
#define BUSY 1


/* Structures & Rules */

// patients can only be performed by their initial surgeon
// priority based on arrival time
#define FIFO 0

// patients can be performed by their initial surgeon as well as other idle surgeon (depend on available_matrix)
// priority based on arrival time
#define FIFO_FLEX 1

// patients can only be performed by their initial surgeon
// priority based on both arrival time and urgency
#define FIFO_PRIORITY 2

// patients can be performed by their initial surgeon as well as other idle surgeon (depend on available_matrix)
// priority based on both arrival time and urgency
#define FIFO_PRIORITY_FLEX 3


/* Global Variables */
int num_surgeon_busy;
int surgeon_status[NUM_SURGEON+1];

/* Statistical Counters */
// REMARK1: A surgeon * patient matrix that represents the accessibility, total patient served, total wait time etc.
// REMARK2: column/row 0 for total, patient and surgeon index starts from 1
// For example:
// num_pat_entered_matrix[0][1]: number of type 1 patients entered the system
// num_pat_entered_matrix[3][2]: number of type 2 patients entered the 3rd surgeon's wait list
// available_matrix[1][3] == 1: 1st surgeon can serve type 3 patients from other surgeons' list
// (Note: they can serve all types of patients on their own list)
int available_matrix[NUM_SURGEON+1][TYPE_OF_PAT+1]; // types of patient they can serve from OTHERS wait list
int num_pat_entered_matrix[NUM_SURGEON+1][TYPE_OF_PAT+1]; // number of patients entered the queueing system
int num_pat_served_matrix[NUM_SURGEON+1][TYPE_OF_PAT+1]; //number of patients finished queueing (might be in surgery)
int num_pat_left_matrix[NUM_SURGEON+1][TYPE_OF_PAT+1]; // number of patients finished surgery and left the system
int wait_list_matrix[NUM_SURGEON+1][TYPE_OF_PAT+1]; // number of patients on the wait list
double delays_matrix[NUM_SURGEON+1][TYPE_OF_PAT+1]; // total delays
double tnow, time_last_event, total_sim_time, area_num_waitlist, area_server_status;


/* Rates */
double pat_arrival_rate = 2.4;
double sur_completion_rate = 0.3;
double pat_arrival_rate_type1 = pat_arrival_rate * PROB_PAT_TYPE1;
double pat_arrival_rate_type2 = pat_arrival_rate * PROB_PAT_TYPE2;
double pat_arrival_rate_type3 = pat_arrival_rate * PROB_PAT_TYPE3;


/* Seed for generating random number */
long int patient_seed, surgery_seed, patient_type_seed, surgeon_type_seed;


/* Functions */
void initialize_vars(void);
void initialize_postwarmup_vars(void);
void label_patient(void);
void get_next_event (int *etype, int *index, int *surgeon_index, bool *warmup_status);
int select_event(void);
int surgery_busy(int type, int surgeon_index);
void patient_arrival (int urg_type, int surgeon_index);
void surgery_completion(int urg_type, int surgeon_index, bool warmup_status);
struct surgery find_next_surgery(void);
struct surgery find_next_surgery_fifo(void);
struct surgery find_next_surgery_fifo_flex(void);
struct surgery find_next_surgery_fifo_pri(void);
struct surgery find_next_surgery_fifo_pri_flex(void);
bool higher_priority(struct patient, struct patient);
void print_headings(void);
void print_results(void);
void print_warmup_results(void);
void update_stats(void);
void generate_init_event(void);
void generate_event(int etype, int urg_type, int surgeon_index);
void insert_event(double time, int etype, int urg_type, int surgeon_index);
int get_urg_type(void);
int get_sur_type(void);


/* Output functions */
void close_results_files(void);
void write_event_list(void);
void write_wait_list(void);


/* Event: time of event, event type, urgency type, surgeon index, whether in warmup period or not */
typedef struct {
    double time;
    int event_type;
    int urg_type;
    int surgeon_index;
    bool warmup_status;
} event;


/* Patient: urgency type, arrival time, initial surgeon */
typedef struct patient{
    int urg_type;
    double arrival_time;
    int surgeon_index;
} patient;


/* Surgery: index of the patient in the wait list, urgency type, corresponding surgeon */
typedef struct surgery{
    int pat_index;
    int urg_type;
    int surgeon_index;
} surgery;