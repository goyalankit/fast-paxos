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