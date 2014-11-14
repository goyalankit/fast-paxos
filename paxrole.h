// paxrole.h - Defines for role classes for the fast Paxos algorithm
#pragma once

#include "config.h"
#include "paxmsg.h"

class learner {
  public:
    typedef struct learner_record_t {
      int         iid;
      int         ballot;
      int         proposer_id;
      char *      final_value;
      int         final_value_size;
      learn_msg_t*  learns[N_OF_ACCEPTORS];
    } learner_record;

    unsigned int quorum;

    int highest_delivered = -1;
    int highest_seen = -1;
    learner_record learner_array[LEARNER_ARRAY_SIZE];
    int learner_ready = 0;
};

class leader {
  public:
    int quorum;
    int proposer_id;
    int current_iid = 0;
    typedef struct phase1_info_t {
      int pending_count;
      int ready_count;
      int highest_ready;
      int first_to_check;
      int last_to_check;
    } phase1_info;
    phase1_info p1info;

    typedef struct phase2_info_t {
      int current_iid;
    } phase2_info;
    phase2_info p2info;

    char leader_send_buffer[MAX_UDP_MSG_SIZE];
    // This should probably be removed or be convert into a method
    // paxos_msg* msg = (paxos_msg*) leader_send_buffer;

    typedef enum status_flag_t {
      p1_new,
      p1_pending,
      p1_ready,
      p1_finished
    } status_flag;

    typedef struct promise_info_t {
      int     iid;
      int     value_ballot;
      int     value_size;
      char *  value;
    } promise_info;

    typedef struct proposer_record_t {
      int iid;
      int ballot;
      status_flag status;
      int promise_count;
      promise_info promises[N_OF_ACCEPTORS];
      promise_msg_t * reserved_promise;
    } proposer_record;

    proposer_record proposer_array[PROPOSER_ARRAY_SIZE];
};

class proposer {
  public:
    int fixed_ballot;
    int proposer_id;
    struct timeval proposer_to_interval;

    int client_waiting = 0;
    int current_iid;
    int value_delivered;
    int last_accept_iid;
    int last_accept_hash;

    char proposer_send_buffer[MAX_UDP_MSG_SIZE];
    int msg_size;
    typedef struct timeout_info_t {
      int instance_id;
      int hash;
      // struct event to_event;
    } timeout_info;

};

class acceptor {
  public:
    typedef struct acceptor_record_t {
    int     iid;
    int     proposer_id;
    int     ballot;
    int     value_ballot;
    int     value_size;
    int     any_enabled;
    char*   value;
    } acceptor_record;

    int acceptor_id;
    acceptor_record acceptor_array[ACCEPTOR_ARRAY_SIZE];
    int send_buffer_size;

    int min_ballot = 2 * MAX_PROPOSERS;
};