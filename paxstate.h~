#define FIRST_BALLOT (2 * MAX_PROPOSERS + proposer_id)
#define BALLOT_NEXT(lastBal) (lastBal + MAX_PROPOSERS)
#define VALUE_OWNER(bal) (bal % MAX_PROPOSERS)

class learner_state {
  public:
    typedef struct learner_record_t {
      int         iid;
      int         ballot;
      int         proposer_id;
      char*       final_value;
      int         final_value_size;
      learn_msg*  learns[N_OF_ACCEPTORS];
    } learner_record;

    static unsigned int quorum;

    static int highest_delivered = -1;
    static int highest_seen = -1;
    static learner_record learner_array[LEARNER_ARRAY_SIZE];
    static int learner_ready = 0;
};

class leader_state {
  public:
    static int quorum;
    static int proposer_id;
    static int current_iid = 0;
    typedef struct phase1_info_t {
      int pending_count;
      int ready_count;
      int highest_ready;
      int first_to_check;
      int last_to_check;
    } phase1_info;
    static phase1_info p1info;

    typedef struct phase2_info_t {
      int current_iid;
    } phase2_info;
    static phase2_info p2info;

    static char leader_send_buffer[MAX_UDP_MSG_SIZE];
    static paxos_msg* msg = (paxos_msg*) leader_send_buffer;

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
      // promise_msg* reserved_promise;
    } proposer_record;

    static proposer_record proposer_array[PROPOSER_ARRAY_SIZE];
};

class proposer_state {
  public:
    static int fixed_ballot;
    static int proposer_id;
    static struct timeval proposer_to_interval;

    //Lock
    static int client_waiting = 0;
    static int current_iid;
    static int value_delivered;
    static int last_accept_iid;
    static int last_accept_hash;
    //Lock

    static char proposer_send_buffer[MAX_UDP_MSG_SIZE];
    static int msg_size;
    typedef struct timeout_info_t {
      int instance_id;
      int hash;
      struct event to_event;
    } timeout_info;

};

