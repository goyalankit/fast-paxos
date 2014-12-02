// paxrole.h - Defines for role classes for the fast Paxos algorithm
#pragma once

#include "config.h"
#include "paxserver.h"
#include "paxmsg.h"
#include "paxobj.h"
#include "node.h"
#include "paxlog.h"
#include <vector>
#include "paxtypes.h"

using std::vector;
class paxserver;

class learner_t {
  public:
    paxserver *server;
    struct learner_record_t {
      int         iid;
      int         ballot;
      int         proposer_id;
      node_id_t   cid;
      rid_t       rid;
      paxobj::request final_value;
      //learn_msg_t*  learns[N_OF_ACCEPTORS]; // set to vector
      vector <learn_msg_t *> learns;
    };

    tick_t lsync_to_tick; // used to check the lsync_to

    int highest_delivered = -1;
    int highest_seen = -1;
    struct learner_record_t learner_array[LEARNER_ARRAY_SIZE];
    int learner_ready = 0;

    learner_t(paxserver *_server);
    void do_learner_timeout();
    void handle_learn_msg(const struct learn_msg_t&);
    int update_record(const learn_msg_t *);
    int add_learn_to_record(learner_record_t *, const learn_msg_t*);
    bool is_closed(learner_record_t* rec);
    bool check_quorum(const learn_msg_t *lmsg);
    void deliver_values(int);

    /*
     *int is_closed(learner_record_t *rec);
     *void ask_retransmission();
     *void lsync_check([>XXX need to decide arguments<]);
     *void learner_wait_ready();
     *void timeout_check();
     */

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
      node_id_t cid;
      rid_t   rid;
      paxobj::request value;
      promise_info_t(){
        iid = -1;
        rid = -1;
        value_ballot = -1;
        value.reset();
      }
      promise_info_t & operator= (const promise_msg_t &rhs){
        iid = rhs.iid;
        value_ballot = rhs.value_ballot;
        value = rhs.value;
        cid  = rhs.cid;
        rid  = rhs.rid;
        return *this;
      }
    };

    struct proposer_record_t {
      int iid = 0;
      int ballot = -1;
      node_id_t cid;
      rid_t rid;
      status_flag status;
      unsigned int promise_count = 0;
      vector<promise_info_t> promises; //set it to [N_OF_ACCEPTORS];
      promise_msg_t * reserved_promise = nullptr;
    };

    proposer_record_t proposer_array[PROPOSER_ARRAY_SIZE];

    // timeout ticks
    tick_t phase1_to_tick;
    tick_t phase2_to_tick;

    bool do_first_TO;
    /*** functions for a leader ***/
    //constuctors
    leader_t(paxserver * _server);
    void do_leader_timeout(phase12_t);
    void execute_phase1();
    void phase1_check_cb();
    void phase2_check_cb();
    void handle_promise_batch(const struct promise_batch_msg_t &);
    void handle_promise(const struct promise_msg_t &, int, struct proposer_record_t &);
    void execute_phase2(int first_iid, int last_iid);
    promise_info_t* phase2_getMax_promise(proposer_record_t &);
    void resolve_cflt_send_accept(proposer_record_t &, promise_info_t *);
    void leader_deliver_value(int);
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
    bool has_value; // XXX unused as of now
    node_id_t current_cid;
    rid_t current_rid;
    paxobj::request current_request;

    //char proposer_send_buffer[MAX_UDP_MSG_SIZE];
    int msg_size;
    struct timeout_info_t {
      int instance_id;
      int hash;
    };

    struct timeout_info {
      int instance_id;
      int hash;
    };

    struct timeout_info ti;
    proposer_t(paxserver *_server);
    void do_proposer_timeout();
    void proposer_submit_value(const struct execute_arg&);
    void deliver_function(paxobj::request req, int iid, int ballot, node_id_t cid, rid_t rid, int proposer);
    void exit_if_done();
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
      node_id_t cid;
      rid_t rid;
      paxobj::request  value;

      acceptor_record_t() {
        iid = -1;
        ballot = -1;
        value = NULL;
        cid = -1;
        rid = -1;
        proposer_id = -1;
        any_enabled = 0;
      }
      acceptor_record_t(const Paxlog::tup *tup) {
        iid          = tup->iid;
        ballot       = tup->ballot;
        value_ballot = tup->value_ballot;
        value        = tup->request;
        cid          = tup->src;
        rid          = tup->rid;
        any_enabled = 0;
      }
    };

    acceptor_t(paxserver *_server);
    acceptor_record_t acceptor_array[ACCEPTOR_ARRAY_SIZE]; 
    int min_ballot = 2 * MAX_PROPOSERS;
    void handle_accept(const struct accept_msg_t&);
    void handle_prepare_batch(const struct prepare_batch_msg_t&);
    void handle_prepare(const struct prepare_msg_t &, std::vector<promise_msg_t> &);
    void paxlog_update_record(acceptor_record_t &);
    acceptor_record_t * paxlog_lookup_record(int);
    void handle_anyval_batch(const struct anyval_batch_msg_t&);
    void apply_anyval(acceptor_record_t*, int, int);
    void apply_accept(acceptor_record_t *, const accept_msg_t *);
    void handle_lsync(const learner_sync_msg_t &);
};
