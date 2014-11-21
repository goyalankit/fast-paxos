// paxrole.h - Defines for role classes for the fast Paxos algorithm
#pragma once

#include "config.h"
#include "paxmsg.h"
#include "paxobj.h"

class learner_t {
  public:
    struct learner_record_t {
      int         iid;
      int         ballot;
      int         proposer_id;
      //char *      final_value;
      //int         final_value_size;
      paxobj::request request;
      learn_msg_t*  learns[N_OF_ACCEPTORS];
    }; 

    unsigned int quorum;

    int highest_delivered = -1;
    int highest_seen = -1;
    struct learner_record_t learner_array[LEARNER_ARRAY_SIZE];
    int learner_ready = 0;

    int is_closed(learner_record_t *rec);
    int add_learn_to_record(learner_record_t *rec, learn_msg_t* lmsg);
    int update_record(learn_msg_t *lmsg);
    int check_quorum(learn_msg_t *lmsg);
    void learner_handle_learn_msg(learn_msg_t *lmsg);
    void learner_handle_learner_msg(/*XXX need to decide arguments*/);
    void ask_retransmission();
    void lsync_check(/*XXX need to decide arguments*/);
    void learner_wait_ready();

};

class leader_t {
  public:
    int quorum;
    int proposer_id;
    int current_iid = 0;
    struct phase1_info_t {
      int pending_count;
      int ready_count;
      int highest_ready;
      int first_to_check;
      int last_to_check;
    };
    phase1_info_t p1info;

    struct phase2_info_t {
      int current_iid;
    };
    phase2_info_t p2info;

    //char leader_send_buffer[MAX_UDP_MSG_SIZE];
    // This should probably be removed or be convert into a method
    // paxos_msg* msg = (paxos_msg*) leader_send_buffer;

    typedef enum status_flag_t {
      p1_new,
      p1_pending,
      p1_ready,
      p1_finished
    } status_flag;

    struct promise_info_t {
      int     iid;
      int     value_ballot;
      int     value_size;
      char *  value;
    };

    struct proposer_record_t {
      int iid;
      int ballot;
      status_flag status;
      int promise_count;
      promise_info_t promises[N_OF_ACCEPTORS];
      promise_msg_t * reserved_promise;
    };

    proposer_record_t proposer_array[PROPOSER_ARRAY_SIZE];
};

class proposer_t {
  public:
    int fixed_ballot;
    int proposer_id;
    struct timeval proposer_to_interval;

    int client_waiting = 0;
    int current_iid;
    int value_delivered;
    int last_accept_iid;
    int last_accept_hash;

    //char proposer_send_buffer[MAX_UDP_MSG_SIZE];
    int msg_size;
    struct timeout_info_t {
      int instance_id;
      int hash;
      // struct event to_event;
    };

};

class acceptor_t {
  public:
  struct acceptor_record_t {
    int     iid;
    int     proposer_id;
    int     ballot;
    int     value_ballot;
    int     value_size;
    int     any_enabled;
    char*   value;
    };

    int acceptor_id;
    acceptor_record_t acceptor_array[ACCEPTOR_ARRAY_SIZE];
    int send_buffer_size;

    int min_ballot = 2 * MAX_PROPOSERS;
};
