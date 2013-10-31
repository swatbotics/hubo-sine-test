#include <hubo.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#define NSEC_PER_SEC 1000000000
#define SEC_PER_NSEC 1e-9

#define MY_PRIORITY (49)/* we use 49 as the PRREMPT_RT use 50
                            as the priority of kernel tasklets
                            and interrupt handler by default */

#define MAX_SAFE_STACK (1024*1024) /* The maximum stack size which is
                                   guaranteed safe to access without
                                   faulting */

void stack_prefault() {

  unsigned char dummy[MAX_SAFE_STACK];
  memset( dummy, 0, MAX_SAFE_STACK );

}

int setupHighPriority() {

  int rval = 0;

  struct sched_param param;

  /* Declare ourself as a real time task */
  param.sched_priority = MY_PRIORITY;
  if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("sched_setscheduler failed");
    rval = -1;
  }
  
  /* Lock memory */
  if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
    perror("mlockall failed");
    rval = -1;
  }
    
  /* Pre-fault our stack */
  stack_prefault();

  return rval;

}


int64_t gettime_nsec_int64() {

  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);

  return (int64_t)t.tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)t.tv_nsec;
  
}

double nsec_to_double_sec(int64_t ns) {
  return (double)ns * SEC_PER_NSEC;
}

FILE* open_logfile(const char* joint_name) {

  const char* fmt = "sine_test_%s_%04d.txt";
  char filename[1024];

  struct stat statinfo;

  int number, status;

  FILE* fp;

  for (number=0; number<10000; ++number) {

    snprintf(filename, 1024, fmt, joint_name, number);

    status = stat(filename, &statinfo);

    if (status < 0 && errno == ENOENT) {

      fp = fopen(filename, "w");

      if (!fp) { 
        fprintf(stderr, "Error opening %s for output\n", filename);
        exit(1);
      }

      printf("Writing log results to %s\n", filename);

      return fp;

    } else if (status < 0) {

      perror("stat()");
      exit(1);

    } 

  }

  fprintf(stderr, "Error finding available log file name\n");
  exit(1);
  
  // not reached
  return NULL;

}

int main(int argc, char** argv) {


  ach_status_t result;
  ach_channel_t ref_chan, state_chan;

  hubo_ref_t H_ref;
  hubo_state_t H_state;

  const double start_stop_time = 2.0; // seconds
  const double total_time = 8.0; // seconds
  
  const double period = 1.0; // seconds
  const double amplitude = 5.0 * M_PI/180; // radians
  const double initial_joint_tol = 1e-7; // radians
  
  const int num_init_state_tries = 3;

  int active_joint_index = -1;

  int64_t start_time, cur_time;

  double elapsed_time, u;

  int state_try, j;

  size_t fs;

  FILE* fp;

  const char* active_joint_name;

  // Get active joint and open log
  if (argc != 2) {
    fprintf(stderr, "usage: hubo-sine-test JOINTNAME\n");
    exit(1);
  } 

  active_joint_name = argv[1];

  for (j=0; j<HUBO_JOINT_COUNT; ++j) {
    if (strcmp(jointNames[j], active_joint_name) == 0) {
      active_joint_index = j;
      break;
    }
  }

  if (active_joint_index < 0 || active_joint_index >= HUBO_JOINT_COUNT) {
    fprintf(stderr, "Invalid joint name: %s\n", 
            active_joint_name);
    exit(0);
  }

  if (active_joint_index == LHY || active_joint_index == RHY ||
      active_joint_index == LKN || active_joint_index == RKN || 
      active_joint_index == LEB || active_joint_index == REB ||
      active_joint_index == LSR || active_joint_index == RSR || 
      active_joint_index == LWR || active_joint_index == RWR) {
    fprintf(stderr, "Joint %s not allowed.\n",
            active_joint_name);
    exit(1);
  }

  fp = open_logfile(active_joint_name);

  fprintf(fp, "# time %s.cmd %s.ref %s.pos\n",
          active_joint_name,
          active_joint_name,
          active_joint_name);

  // Set up high priority
  setupHighPriority();

  // open ref channel
  result = ach_open(&ref_chan, HUBO_CHAN_REF_NAME, NULL);
  if (result != ACH_OK) {
    fprintf(stderr, "Error opening ref channel: %s\n", 
            ach_result_to_string(result));
    exit(1);
  }

  // open state channel
  result = ach_open(&state_chan, HUBO_CHAN_STATE_NAME, NULL);
  if (result != ACH_OK) {
    fprintf(stderr, "Error opening state channel: %s\n",
            ach_result_to_string(result));
    exit(1);
  }
  
  // flush state channel
  result = ach_flush(&state_chan);
  if (result != ACH_OK) {
    fprintf(stderr, "Error flushing state channel: %s\n",
            ach_result_to_string(result));
    exit(1);
  }

  // get initial state
  for (state_try=0; state_try<num_init_state_tries; ++state_try) {

    result = ach_get(&state_chan, &H_state, sizeof(H_state), &fs, NULL, 
                     ACH_O_WAIT | ACH_O_LAST);

    if (result == ACH_OK) {

      break;

    } else if (result != ACH_MISSED_FRAME) {

      fprintf(stderr, "Error getting initial state: %s\n",
              ach_result_to_string(result));
      exit(1);
      
    }

  }

  if (result != ACH_OK) {
    fprintf(stderr, "Error getting initial state: %s\n",
            ach_result_to_string(result));
    exit(1);
  }

  // set first ref
  memset(&H_ref, 0, sizeof(H_ref));

  for (j=0; j<HUBO_JOINT_COUNT; ++j) {
    if (fabs(H_state.joint[j].ref) > initial_joint_tol) {
      fprintf(stderr, "Not running because initial ref for joint %s is %g\n",
              jointNames[j], H_state.joint[j].ref);
      exit(1);
    }
    H_ref.mode[j] = HUBO_REF_MODE_REF;
  }

  // initialize loop
  start_time = gettime_nsec_int64();
  cur_time = start_time;
  elapsed_time = 0;

  while (elapsed_time < total_time) {

    // get state
    result = ach_get(&state_chan, &H_state, sizeof(H_state), &fs, NULL, 
                     ACH_O_WAIT | ACH_O_LAST);
    
    if (result == ACH_MISSED_FRAME) {
      printf("Warning: missed frame at t=%f\n", elapsed_time);
      continue; 
    } else if (result != ACH_OK) {
      fprintf(stderr, "Error getting state at time %f: %s\n",
              elapsed_time, ach_result_to_string(result));
      exit(1);
    }

    // get scaling factor in [0,1]
    if (elapsed_time < start_stop_time) {
      u = elapsed_time / start_stop_time;
    } else if (elapsed_time > total_time - start_stop_time) {
      u = (total_time - elapsed_time) / start_stop_time;
    } else {
      u = 1.0;
    }

    // send through cubic spline
    u = 3*u*u - 2*u*u*u;

    // set joint
    H_ref.ref[active_joint_index] = 
      sin(elapsed_time * 2 * M_PI / period) * amplitude * u;

    // send ref
    result = ach_put(&ref_chan, &H_ref, sizeof(H_ref));
    
    if (result != ACH_OK) {
      fprintf(stderr, "Error writing ref at time %f: %s\n",
              elapsed_time, ach_result_to_string(result));
      exit(1);
    }

    // log things
    fprintf(fp, "%f %f %f %f\n",
            elapsed_time,
            H_ref.ref[active_joint_index],
            H_state.joint[active_joint_index].ref,
            H_state.joint[active_joint_index].pos);

    // update time
    cur_time = gettime_nsec_int64();
    elapsed_time = nsec_to_double_sec( cur_time - start_time );

  }

  // return refs to 0

  // set joint
  H_ref.ref[active_joint_index] = 0;

  // send ref
  result = ach_put(&ref_chan, &H_ref, sizeof(H_ref));
  
  if (result != ACH_OK) {
    fprintf(stderr, "Error writing ref at time %f: %s\n",
            elapsed_time, ach_result_to_string(result));
    exit(1);
  }

  fclose(fp);
  
  return 0;
  

}
