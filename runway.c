// Jacob Louanlavong, 100216948
/* Copyright (c) 2025 Trevor Bakker
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILTY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/license/>.
*/


#define _GNU_SOURCE

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

/*** Constants that define parameters of the simulation ***/

#define MAX_RUNWAY_CAPACITY 2    /* Number of aircraft that can use runway simultaneously */
#define CONTROLLER_LIMIT 8       /* Number of aircraft the controller can manage before break */
#define MAX_AIRCRAFT 1000        /* Maximum number of aircraft in the simulation */
#define FUEL_MIN 20              /* Minimum fuel reserve in seconds */
#define FUEL_MAX 60              /* Maximum fuel reserve in seconds */
#define EMERGENCY_TIMEOUT 30     /* Max wait time for emergency aircraft in seconds */
#define DIRECTION_SWITCH_TIME 5  /* Time required to switch runway direction */
#define DIRECTION_LIMIT 3        /* Max consecutive aircraft in same direction */

#define COMMERCIAL 0
#define CARGO 1
#define EMERGENCY 2

#define NORTH 0
#define SOUTH 1
#define EAST  2
#define WEST  4

/* TODO */
/* Add your synchronization variables here */

/* basic information about simulation.  they are printed/checked at the end 
 * and in assert statements during execution.
 *
 * you are responsible for maintaining the integrity of these variables in the 
 * code that you develop. 
 */

sem_t runway_sem;  // controls how mnay aircrafts can be on runway
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // protects shared variables
pthread_cond_t cond_check = PTHREAD_COND_INITIALIZER;

static int aircraft_on_runway = 0;       /* Total number of aircraft currently on runway */
static int commercial_on_runway = 0;     /* Total number of commercial aircraft on runway */
static int cargo_on_runway = 0;          /* Total number of cargo aircraft on runway */
static int emergency_on_runway = 0;      /* Total number of emergency aircraft on runway */
static int aircraft_since_break = 0;     /* Aircraft processed since last controller break */
static int current_direction = NORTH;    /* Current runway direction (NORTH or SOUTH) */
static int consecutive_direction = 0;    /* Consecutive aircraft in current direction */
static int commercial_waiting = 0;
static int cargo_waiting = 0;
static int controller_break = 0; 
static int switching_direction = 0;


typedef struct 
{
  int arrival_time;         // time between the arrival of this aircraft and the previous aircraft
  int runway_time;          // time the aircraft needs to spend on the runway
  int aircraft_id;
  int aircraft_type;        // COMMERCIAL, CARGO, or EMERGENCY
  int fuel_reserve;         // Randomly assigned fuel reserve (FUEL_MIN to FUEL_MAX seconds)
  time_t arrival_timestamp; // timestamp when aircraft thread was created
} aircraft_info;

/* Called at beginning of simulation.  
 * TODO: Create/initialize all synchronization
 * variables and other global variables that you add.
 */
static int initialize(aircraft_info *ai, char *filename) 
{
  aircraft_on_runway    = 0;
  commercial_on_runway  = 0;
  cargo_on_runway       = 0;
  emergency_on_runway   = 0;
  aircraft_since_break  = 0;
  current_direction     = NORTH;
  consecutive_direction = 0;
  commercial_waiting = 0;
  cargo_waiting = 0;
  controller_break = 0;
  switching_direction = 0;

  /* Initialize your synchronization variables (and 
   * other variables you might use) here
   */
  sem_init(&runway_sem, 0, MAX_RUNWAY_CAPACITY);

  /* seed random number generator for fuel reserves */
  srand(time(NULL));

  /* Read in the data file and initialize the aircraft array */
  FILE *fp;

  if((fp=fopen(filename, "r")) == NULL) 
  {
    printf("Cannot open input file %s for reading.\n", filename);
    exit(1);
  }

  int i = 0;
  char line[256];
  while (fgets(line, sizeof(line), fp) && i < MAX_AIRCRAFT) 
  {
    /* Skip comment lines and empty lines */
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
      continue;
    }
    
    /* Parse the line */
    if (sscanf(line, "%d%d%d", &(ai[i].aircraft_type), &(ai[i].arrival_time), 
               &(ai[i].runway_time)) == 3) {
      /* Assign random fuel reserve between FUEL_MIN and FUEL_MAX */
      ai[i].fuel_reserve = FUEL_MIN + (rand() % (FUEL_MAX - FUEL_MIN + 1));
      i = i + 1;
    }
  }

  fclose(fp);
  return i;
}

/* Code executed by controller to simulate taking a break 
 * You do not need to add anything here.  
 */
__attribute__((unused)) static void take_break() 
{
  printf("The air traffic controller is taking a break now.\n");
  sleep(5);
  assert( aircraft_on_runway == 0 );
  aircraft_since_break = 0;
}

/* Code executed to switch runway direction
 * You do not need to add anything here.
 */
__attribute__((unused)) static void switch_direction()
{
  printf("Switching runway direction from %s to %s\n",
         current_direction == NORTH ? "NORTH" : "SOUTH",
         current_direction == NORTH ? "SOUTH" : "NORTH");
  
  assert( aircraft_on_runway == 0 );  // Runway must be empty to switch
  
  sleep(DIRECTION_SWITCH_TIME);
  
  current_direction = (current_direction == NORTH) ? SOUTH : NORTH;
  consecutive_direction = 0;
  
  printf("Runway direction switched to %s\n",
         current_direction == NORTH ? "NORTH" : "SOUTH");
}

/* Code for the air traffic controller thread. This is fully implemented except for 
 * synchronization with the aircraft. See the comments within the function for details.
 */
void *controller_thread(void *arg) 
{
  // Suppress the warning for now
 (void)arg;

  printf("The air traffic controller arrived and is beginning operations\n");

  /* Loop while waiting for aircraft to arrive. */
  while (1) 
  {
    /* TODO */
    /* Add code here to handle aircraft requests, controller breaks,      */
    /* and runway direction switches.                                     */
    /* Currently the body of the loop is empty.  There's no communication */
    /* between controller and aircraft, i.e. all aircraft are admitted    */
    /* without regard for runway capacity, aircraft type, direction,      */
    /* priorities, and whether the controller needs a break.              */
    /* You need to add all of this.                                       */
    pthread_mutex_lock(&lock);

    int should_switch = 0;

    if (consecutive_direction >= DIRECTION_LIMIT) {
      should_switch = 1;
    } else if (aircraft_on_runway == 0) {
        if (current_direction == NORTH && cargo_waiting > 0) {
          should_switch = 1;
      } else if (current_direction == SOUTH && commercial_waiting > 0) {
        should_switch = 1;
      }
    }
    

      if (should_switch && aircraft_on_runway == 0) {
        switching_direction = 1;
        sem_wait(&runway_sem);
        switch_direction();
        consecutive_direction = 0;
        switching_direction = 0;
        pthread_cond_broadcast(&cond_check);
      }
    
    if (aircraft_since_break >= CONTROLLER_LIMIT) {
      controller_break = 1;
      while (aircraft_on_runway > 0) {
        pthread_cond_wait(&cond_check, &lock);
      }

      pthread_mutex_unlock(&lock);
      take_break();
      pthread_mutex_lock(&lock);
      controller_break = 0; 
      aircraft_since_break = 0;
    }
    
    pthread_cond_broadcast(&cond_check);
    pthread_mutex_unlock(&lock);
    /* Allow thread to be cancelled */
    pthread_testcancel();
    usleep(100000); // 100ms sleep to prevent busy waiting
  }
  pthread_exit(NULL);
}


/* 
* Function: commercial_enter
* Parameters: aircraft_info - pointer to aircraft structure
* Returns: void
* Description: handles the sync for a commercial airplane requesting runway access. Blocks until
*              the aircraft can safetly use the runway according to all constraints
 */
void commercial_enter(aircraft_info *arg) 
{
  // Suppress the compiler warning
  (void)arg;

  /* TODO */
  /* Request permission to use the runway. You might also want to add      */
  /* synchronization for the simulation variables below.                   */
  /* Consider: runway capacity, direction (commercial prefer NORTH),       */
  /* controller breaks, fuel levels, emergency priorities, and fairness.   */
  /*  YOUR CODE HERE.                                                      */ 
  usleep(100000);
  pthread_mutex_lock(&lock);
  commercial_waiting = commercial_waiting + 1;

  while (aircraft_on_runway >= MAX_RUNWAY_CAPACITY || current_direction == SOUTH  
  || switching_direction || controller_break) {
    pthread_cond_wait(&cond_check, &lock);
  }

  commercial_waiting = commercial_waiting - 1;
  aircraft_on_runway    = aircraft_on_runway + 1;
  aircraft_since_break  = aircraft_since_break + 1;
  commercial_on_runway  = commercial_on_runway + 1;
  consecutive_direction = consecutive_direction + 1;

  pthread_cond_broadcast(&cond_check);
  pthread_mutex_unlock(&lock);
}

/* 
* Function: cargo_enter
* Parameters: aircraft_info - pointer to aircraft structure
* Returns: void
* Description: handles the sync for a cargo airplane requesting runway access. Blocks until
*              the aircraft can safetly use the runway according to all constraints
 */
void cargo_enter(aircraft_info *ai) 
{
  (void)ai;

  /* TODO */
  /* Request permission to use the runway. You might also want to add      */
  /* synchronization for the simulation variables below.                   */
  /* Consider: runway capacity, direction (cargo prefer SOUTH),            */
  /* controller breaks, fuel levels, emergency priorities, and fairness.   */
  /*  YOUR CODE HERE.                                                      */ 

  pthread_mutex_lock(&lock);

  cargo_waiting = cargo_waiting + 1;

   while (aircraft_on_runway >= MAX_RUNWAY_CAPACITY || current_direction == NORTH
  || switching_direction || controller_break) {
    pthread_cond_wait(&cond_check, &lock);
  }

  cargo_waiting = cargo_waiting - 1;
  aircraft_on_runway    = aircraft_on_runway + 1;
  aircraft_since_break  = aircraft_since_break + 1;
  cargo_on_runway       = cargo_on_runway + 1;
  consecutive_direction = consecutive_direction + 1;

  pthread_cond_broadcast(&cond_check);
  pthread_mutex_unlock(&lock);
}

/* 
* Function: emergency_enter
* Parameters: aircraft_info - pointer to aircraft structure
* Returns: void
* Description: handles the sync for a emergency airplane requesting runway access. Blocks until
*              the aircraft can safetly use the runway according to all constraints
 */
void emergency_enter(aircraft_info *ai) 
{
  (void)ai;

  /* TODO */
  /* Request permission to use the runway. You might also want to add      */
  /* synchronization for the simulation variables below.                   */
  /* Emergency aircraft have priority and must be admitted within 30s,     */
  /* but still respect runway capacity and controller breaks.              */
  /* Emergency aircraft can use either direction.                          */
  /*  YOUR CODE HERE.                                                      */ 

  pthread_mutex_lock(&lock);
  while(aircraft_on_runway >= MAX_RUNWAY_CAPACITY || controller_break == 1 
  || switching_direction == 1) {
    pthread_cond_wait(&cond_check, &lock);
  }

  aircraft_on_runway = aircraft_on_runway + 1;
  aircraft_since_break = aircraft_since_break + 1;
  emergency_on_runway = emergency_on_runway + 1;
  consecutive_direction = consecutive_direction + 1;

  pthread_cond_broadcast(&cond_check);
  pthread_mutex_unlock(&lock);
}

/* Code executed by an aircraft to simulate the time spent on the runway
 * You do not need to add anything here.  
 */
static void use_runway(int t) 
{
  sleep(t);
}


/* Code executed by a commercial aircraft when leaving the runway.
 * You need to implement this.  Do not delete the assert() statements,
 * but feel free to add as many of your own as you like.
 */
static void commercial_leave() 
{
  /* 
   *  TODO
   *  YOUR CODE HERE.
   */
  pthread_mutex_lock(&lock);

  aircraft_on_runway = aircraft_on_runway - 1;
  commercial_on_runway = commercial_on_runway - 1;

  pthread_cond_broadcast(&cond_check);
  pthread_mutex_unlock(&lock);
}

/* Code executed by a cargo aircraft when leaving the runway.
 * You need to implement this.  Do not delete the assert() statements,
 * but feel free to add as many of your own as you like.
 */
static void cargo_leave() 
{
  /* 
   * TODO
   * YOUR CODE HERE. 
   */
  pthread_mutex_lock(&lock);

  aircraft_on_runway = aircraft_on_runway - 1;
  cargo_on_runway = cargo_on_runway - 1;

  pthread_cond_broadcast(&cond_check);
  pthread_mutex_unlock(&lock);
}

/* Code executed by an emergency aircraft when leaving the runway.
 * You need to implement this.  Do not delete the assert() statements,
 * but feel free to add as many of your own as you like.
 */
static void emergency_leave() 
{
  /* 
   * TODO
   * YOUR CODE HERE. 
   */
  pthread_mutex_lock(&lock);

  aircraft_on_runway = aircraft_on_runway - 1;
  emergency_on_runway = emergency_on_runway - 1;

  pthread_cond_broadcast(&cond_check);
  pthread_mutex_unlock(&lock);
}

/* Main code for commercial aircraft threads.  
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void* commercial_aircraft(void *ai_ptr) 
{
  aircraft_info *ai = (aircraft_info*)ai_ptr;
  
  /* Record arrival time for fuel tracking */
  ai->arrival_timestamp = time(NULL);

  /* Request runway access */
  commercial_enter(ai);

  printf("Commercial aircraft %d (fuel: %ds) is now on the runway (direction: %s)\n", 
         ai->aircraft_id, ai->fuel_reserve,
         current_direction == NORTH ? "NORTH" : "SOUTH");

  assert(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0);
  assert(commercial_on_runway >= 0 && commercial_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway >= 0 && cargo_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(emergency_on_runway >= 0 && emergency_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway == 0 ); // Commercial and cargo cannot mix
  
  /* Use runway  --- do not make changes to the 3 lines below*/
  printf("Commercial aircraft %d begins runway operations for %d seconds\n", 
         ai->aircraft_id, ai->runway_time);
  use_runway(ai->runway_time);
  printf("Commercial aircraft %d completes runway operations and prepares to depart\n", 
         ai->aircraft_id);

  /* Leave runway */
  commercial_leave();  

  printf("Commercial aircraft %d has cleared the runway\n", ai->aircraft_id);

  if (!(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0)) {
    printf("ASSERT FAILURE: aircraft_on_runway=%d (should be 0-%d)\n", aircraft_on_runway, MAX_RUNWAY_CAPACITY);
    printf("Runway state: commercial=%d, cargo=%d, emergency=%d, direction=%s\n", 
           commercial_on_runway, cargo_on_runway, emergency_on_runway,
           current_direction == NORTH ? "NORTH" : "SOUTH");
  }
  assert(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0);
  assert(commercial_on_runway >= 0 && commercial_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway >= 0 && cargo_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(emergency_on_runway >= 0 && emergency_on_runway <= MAX_RUNWAY_CAPACITY);

  pthread_exit(NULL);
}

/* Main code for cargo aircraft threads.
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void* cargo_aircraft(void *ai_ptr) 
{
  aircraft_info *ai = (aircraft_info*)ai_ptr;
  
  /* Record arrival time for fuel tracking */
  ai->arrival_timestamp = time(NULL);

  /* Request runway access */
  cargo_enter(ai);

  printf("Cargo aircraft %d (fuel: %ds) is now on the runway (direction: %s)\n", 
         ai->aircraft_id, ai->fuel_reserve,
         current_direction == NORTH ? "NORTH" : "SOUTH");

  if (!(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0)) {
    printf("ASSERT FAILURE: aircraft_on_runway=%d (should be 0-%d)\n", aircraft_on_runway, 
            MAX_RUNWAY_CAPACITY);
    printf("Runway state: commercial=%d, cargo=%d, emergency=%d, direction=%s\n", 
           commercial_on_runway, cargo_on_runway, emergency_on_runway,
           current_direction == NORTH ? "NORTH" : "SOUTH");
  }
  assert(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0);
  assert(commercial_on_runway >= 0 && commercial_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway >= 0 && cargo_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(emergency_on_runway >= 0 && emergency_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(commercial_on_runway == 0 ); 

  printf("Cargo aircraft %d begins runway operations for %d seconds\n", 
         ai->aircraft_id, ai->runway_time);
  use_runway(ai->runway_time);
  printf("Cargo aircraft %d completes runway operations and prepares to depart\n", 
         ai->aircraft_id);

  /* Leave runway */
  cargo_leave();        

  printf("Cargo aircraft %d has cleared the runway\n", ai->aircraft_id);

  if (!(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0)) {
    printf("ASSERT FAILURE: aircraft_on_runway=%d (should be 0-%d)\n", 
           aircraft_on_runway, MAX_RUNWAY_CAPACITY);
    printf("Runway state: commercial=%d, cargo=%d, emergency=%d, direction=%s\n", 
           commercial_on_runway, cargo_on_runway, emergency_on_runway,
           current_direction == NORTH ? "NORTH" : "SOUTH");
  }
  assert(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0);
  assert(commercial_on_runway >= 0 && commercial_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway >= 0 && cargo_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(emergency_on_runway >= 0 && emergency_on_runway <= MAX_RUNWAY_CAPACITY);

  pthread_exit(NULL);
}

/* Main code for emergency aircraft threads.
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void* emergency_aircraft(void *ai_ptr) 
{
  aircraft_info *ai = (aircraft_info*)ai_ptr;
  
  /* Record arrival time for fuel and emergency timeout tracking */
  ai->arrival_timestamp = time(NULL);

  /* Request runway access */
  emergency_enter(ai);

  printf("EMERGENCY aircraft %d (fuel: %ds) is now on the runway (direction: %s)\n", 
         ai->aircraft_id, ai->fuel_reserve,
         current_direction == NORTH ? "NORTH" : "SOUTH");

  if (!(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0)) {
    printf("ASSERT FAILURE: aircraft_on_runway=%d (should be 0-%d)\n", aircraft_on_runway, 
            MAX_RUNWAY_CAPACITY);
    printf("Runway state: commercial=%d, cargo=%d, emergency=%d, direction=%s\n", 
           commercial_on_runway, cargo_on_runway, emergency_on_runway,
           current_direction == NORTH ? "NORTH" : "SOUTH");
  }
  assert(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0);
  assert(commercial_on_runway >= 0 && commercial_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway >= 0 && cargo_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(emergency_on_runway >= 0 && emergency_on_runway <= MAX_RUNWAY_CAPACITY);

  printf("EMERGENCY aircraft %d begins runway operations for %d seconds\n", 
         ai->aircraft_id, ai->runway_time);
  use_runway(ai->runway_time);
  printf("EMERGENCY aircraft %d completes runway operations and prepares to depart\n", 
         ai->aircraft_id);

  /* Leave runway */
  emergency_leave();        

  printf("EMERGENCY aircraft %d has cleared the runway\n", ai->aircraft_id);

  if (!(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0)) {
    printf("ASSERT FAILURE: aircraft_on_runway=%d (should be 0-%d)\n", 
           aircraft_on_runway, MAX_RUNWAY_CAPACITY);
    printf("Runway state: commercial=%d, cargo=%d, emergency=%d, direction=%s\n", 
           commercial_on_runway, cargo_on_runway, emergency_on_runway,
           current_direction == NORTH ? "NORTH" : "SOUTH");
  }
  assert(aircraft_on_runway <= MAX_RUNWAY_CAPACITY && aircraft_on_runway >= 0);
  assert(commercial_on_runway >= 0 && commercial_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(cargo_on_runway >= 0 && cargo_on_runway <= MAX_RUNWAY_CAPACITY);
  assert(emergency_on_runway >= 0 && emergency_on_runway <= MAX_RUNWAY_CAPACITY);

  pthread_exit(NULL);
}

/* Main function sets up simulation and prints report
 * at the end.
 * GUID: 355F4066-DA3E-4F74-9656-EF8097FBC985
 */
int main(int nargs, char **args) 
{
  int i;
  int result;
  int num_aircraft;
  void *status;
  pthread_t controller_tid;
  pthread_t aircraft_tid[MAX_AIRCRAFT];
  aircraft_info ai[MAX_AIRCRAFT];

  if (nargs != 2) 
  {
    printf("Usage: runway <name of inputfile>\n");
    return EINVAL;
  }

  num_aircraft = initialize(ai, args[1]);
  if (num_aircraft > MAX_AIRCRAFT || num_aircraft <= 0) 
  {
    printf("Error:  Bad number of aircraft threads. "
           "Maybe there was a problem with your input file?\n");
    return 1;
  }

  printf("Starting runway simulation with %d aircraft ...\n", num_aircraft);

  result = pthread_create(&controller_tid, NULL, controller_thread, NULL);

  if (result) 
  {
    printf("runway:  pthread_create failed for controller: %s\n", strerror(result));
    exit(1);
  }

  for (i=0; i < num_aircraft; i++) 
  {
    ai[i].aircraft_id = i;
    sleep(ai[i].arrival_time);
                
    if (ai[i].aircraft_type == COMMERCIAL)
    {
      result = pthread_create(&aircraft_tid[i], NULL, commercial_aircraft, 
                             (void *)&ai[i]);
    }
    else if (ai[i].aircraft_type == CARGO)
    {
      result = pthread_create(&aircraft_tid[i], NULL, cargo_aircraft, 
                             (void *)&ai[i]);
    }
    else 
    {
      result = pthread_create(&aircraft_tid[i], NULL, emergency_aircraft, 
                             (void *)&ai[i]);
    }

    if (result) 
    {
      printf("runway: pthread_create failed for aircraft %d: %s\n", 
            i, strerror(result));
      exit(1);
    }
  }

  /* wait for all aircraft threads to finish */
  for (i = 0; i < num_aircraft; i++) 
  {
    pthread_join(aircraft_tid[i], &status);
  }

  /* tell the controller to finish. */
  pthread_cancel(controller_tid);
  pthread_join(controller_tid, &status);

  printf("Runway simulation done.\n");

  return 0;
}
