// TODO The implemation of opertaions taken by diffrent roles
#include "paxrole.h"
#include "paxserver.h"

/** leader functions **/
leader_t::leader_t(paxserver *_server) {
  server = _server;

  //initialize p1info for prepare phase
  p1info.pending_count = 0;
  p1info.ready_count = 0;
  p1info.highest_ready = -1;
  p1info.first_to_check = 0;
  p1info.last_to_check = 0;

  //initialize p2info
  p2info.current_iid = -1;

  phase1_to_tick = PHASE1_TO_TICK;
  phase2_to_tick = PHASE2_TO_TICK;
  for (int i = 0; i < PROPOSER_ARRAY_SIZE; i++){
    proposer_array[i] = {};
    proposer_array[i].promises.resize(server->get_serv_cnt(server->vc_state.view));
  }
}

void leader_t::do_leader_timeout(phase1ab_t phase) {
  switch (phase){
    case phase1ab_t::phase1:{
      break;
    }
    case phase1ab_t::phase2:{
      break;
    }
    default: {
      MASSERT(0, "Phase not recognized in leader timeout.\n Destruct sequence initiated. 3..2..1..run!\n");
    }

  }
}

/** proposer functions **/
proposer_t::proposer_t(paxserver *_server) {
  server = _server;
  fixed_ballot = MAX_PROPOSERS + server->get_nid(); //TODO: add MAX_PROPOSER to config 
  current_iid = 0;

}

void proposer_t::do_proposer_timeout() {
}

/** acceptor functions **/
acceptor_t::acceptor_t(paxserver *_server){
  server = _server;
}

/** learner functions **/
learner_t::learner_t(paxserver *_server){
  server = _server;
  lsync_to_tick = LSYNC_TICK;
  for (int i = 0; i < LEARNER_ARRAY_SIZE; i++){
    learner_array[i] = {};
    learner_array[i].learns.resize(server->get_serv_cnt(server->vc_state.view));
  }
}

void learner_t::do_learner_timeout() {
}
