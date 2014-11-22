// TODO The implemation of opertaions taken by diffrent roles
#include "paxrole.h"
#include "paxserver.h"

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

  //promises.resize(server->get_acceptor_cnt()); //TODO implement get_acceptor_cnt in paxserver class


}

