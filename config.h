// config.h - Defines for configuration parameters
#pragma once

/* 
    Number of phase1 instances
    to be pre-executed by leader
*/
#define PROPOSER_PREEXEC_WIN_SIZE 50

/* 
   ACCEPTOR_ARRAY_SIZE is chosen to be at least 
   10 times larger than PROPOSER_PREEXEC_WIN_SIZE
   IMPORTANT: This must be a power of 2
*/
#define ACCEPTOR_ARRAY_SIZE 1024

/* 
   LEARNER_ARRAY_SIZE can be any size, to ensure proper
   buffering it can be set to a multiple of the 
   PROPOSER_PREEXEC_WIN_SIZE
   IMPORTANT: This must be a power of 2
*/
#define LEARNER_ARRAY_SIZE 512


/* 
   PROPOSER_ARRAY_SIZE can be any size, to ensure proper
   buffering it can be set to a multiple of the 
   PROPOSER_PREEXEC_WIN_SIZE
   IMPORTANT: This must be a power of 2
*/
#define PROPOSER_ARRAY_SIZE 512


#define MAX_UDP_MSG_SIZE 9000
#define PAXOS_MAX_VALUE_SIZE 8960

#define PAXOS_MAX_VALUE_SIZE 8960

#define N_OF_ACCEPTORS  3

#define MAX_PROPOSERS 10

/*
 PHASE1_TO_TICK is the timeout value for phase 1
 to happen in case there are no messages in the system
 TODO(goyalankit) Set appropriate values
*/
#define PHASE1_TO_TICK 12
#define PHASE2_TO_TICK 12

// TODO(goyalankit) change the value
#define LSYNC_TICK 12

#define PROPOSER_TO_TICK 9

// Server related timeout values
#define DEFAULT_SWITCH_TIMO 3 * 2
#define SERV_VCA_TIMO 50 * 2
#define SERV_DEAD_TIMO 200 * 2
#define SERV_HEARTBEAT_TIMO 75 * 2

enum phase12_t {
  phase1,
  phase2
};


#define GET_PRO_INDEX(n) (n & (PROPOSER_ARRAY_SIZE-1))
