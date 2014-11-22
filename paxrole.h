// paxrole.h - Defines for role classes for the fast Paxos algorithm
#pragma once

#include "config.h"
#include "paxserver.h"
#include "paxmsg.h"
#include "paxobj.h"
#include "node.h"
#include <vector>

using std::vector;
class paxserver;

class learner_t {
  public:
    paxserver *server;
    struct learner_record_t {
      int         iid;
      int         ballot;
      int         proposer_id;
      paxobj::request final_value;
      //learn_msg_t*  learns[N_OF_ACCEPTORS]; // set to vector
      vector <learn_msg_t *> learns;
    };

    tick_t lsync_to_tick; // used to check the lsync_to

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
    void timeout_check();
    void do_learner_timeout();
    learner_t(paxserver *_server);
};

class leader_t {
  public:
    paxserver * server;
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
      paxobj::request value;
    };

    struct proposer_record_t {
      int iid = 0;
      int ballot = -1;
      status_flag status;
      int promise_count = 0;
      vector<promise_info_t> promises; //set it to [N_OF_ACCEPTORS];
      promise_msg_t * reserved_promise = nullptr;
    };

    proposer_record_t proposer_array[PROPOSER_ARRAY_SIZE];

    // timeout ticks
    tick_t phase1_to_tick;
    tick_t phase2_to_tick;

    /*** functions for a leader ***/
    //constuctors
    leader_t(paxserver * _server);
    void do_leader_timeout(phase12_t);
};

class proposer_t {
  public:
    paxserver *server;
    int fixed_ballot;
    //struct timeval proposer_to_interval;
    tick_t proposer_to_tick;

    int current_iid;
    int value_delivered;
    int last_accept_iid;
    int last_accept_hash;
    bool has_value;

    //char proposer_send_buffer[MAX_UDP_MSG_SIZE];
    int msg_size;
    struct timeout_info_t {
      int instance_id;
      int hash;
    };

    proposer_t(paxserver *_server);
    void do_proposer_timeout();
    void proposer_submit_value(const struct execute_arg&);
};

class acceptor_t {
  public:
    paxserver *server;
    struct acceptor_record_t {
      int     iid;
      int     proposer_id;
      int     ballot;
      int     value_ballot;
      int     any_enabled;
      paxobj::request  value;
    };

    acceptor_record_t acceptor_array[ACCEPTOR_ARRAY_SIZE]; 
    int min_ballot = 2 * MAX_PROPOSERS;
    acceptor_t(paxserver *_server);
    void handle_accept(const struct accept_msg_t&);
};
