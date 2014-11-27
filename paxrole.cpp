// TODO The implemation of opertaions taken by diffrent roles
#include "paxrole.h"
#include "paxserver.h"
#include "log.h"

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

  phase1_to_tick = 0;
  phase2_to_tick = PHASE2_TO_TICK;
  for (int i = 0; i < PROPOSER_ARRAY_SIZE; i++){
    proposer_array[i] = {};
    proposer_array[i].promises.resize(server->get_serv_cnt(server->vc_state.view));
  }
}

void leader_t::execute_phase1() {
  proposer_record_t *rec;

  int from = (p1info.highest_ready + 1);
  int to = (from + PROPOSER_PREEXEC_WIN_SIZE);
  vector<prepare_msg_t> messages;

  LOG(l::DEBUG, "Leader pre-executing phase 1 from " << from << " to " << to);
  // LOG(DBG, ());

  for(int i = from; i <= to; i++) {
    rec = &proposer_array[GET_PRO_INDEX(i)];

    if(rec->iid < i || rec->status == p1_new) {
      p1info.pending_count++;
      rec->iid = i;
      rec->status = p1_pending;
      rec->ballot = FIRST_BALLOT(server->nid);
      rec->promise_count = 0;
      int j;
      for(j = 0; j < N_OF_ACCEPTORS; j++) {
        rec->promises[j].iid = -1;
        rec->promises[j].value_ballot = -1;
        if (rec->promises[j].value != NULL) {
          rec->promises[j].value = NULL;
        }
      }
      //send_prepare_msg_to_acceptor
      prepare_msg_t prepare_msg(rec->iid, rec->ballot);
      messages.push_back(prepare_msg);

      //add_prepare_to_buffer(rec->iid, rec->ballot);
    }
  }

  if(p1info.last_to_check < to) {
    p1info.last_to_check = to;
  }
  if(p1info.first_to_check > from) {
    p1info.first_to_check = from;
  }

  if(!messages.empty()) {
    server->broadcast<prepare_batch_msg_t>(messages);
  }

}

void leader_t::do_leader_timeout(phase12_t phase) {
  switch (phase){
    case phase12_t::phase1:{
      execute_phase1();
      phase1_to_tick = PHASE1_TO_TICK;
      break;
    }
    case phase12_t::phase2:{
      phase2_to_tick = PHASE2_TO_TICK;
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
  has_value = false;
}

void proposer_t::proposer_submit_value(const struct execute_arg& ex_arg) {
  LOG(l::DEBUG, ("Proposer message received from client\n"));
  last_accept_hash = 1;
  last_accept_iid = current_iid;
  server->broadcast<accept_msg_t>(current_iid, fixed_ballot, server->get_nid());
}

void proposer_t::do_proposer_timeout() {
  proposer_to_tick = PROPOSER_TO_TICK;
}

/** acceptor functions **/
acceptor_t::acceptor_t(paxserver *_server){
  server = _server;
}

void acceptor_t::handle_accept(const struct accept_msg_t&) {
  LOG(l::DEBUG, ("********\nMil gaya!\n**********\n"));
}


// TODO(goyalankit) implement this method
void acceptor_t::paxlog_update_record(acceptor_t::acceptor_record_t &rec) {

}


// TODO(goyalankit) define this method
acceptor_t::acceptor_record_t * acceptor_t::paxlog_lookup_record(int iid) {
  return NULL;
}

void acceptor_t::handle_prepare(const struct prepare_msg_t &msg, std::vector<promise_msg_t> &promise_msgs) {
  acceptor_t::acceptor_record_t* rec;

  rec = &acceptor_array[GET_ACC_INDEX(msg.iid)];

    // Handle a new instance,
    // possibly get rid of an old instance
    if (msg.iid > rec->iid) {
        rec->iid = msg.iid;
        rec->ballot = msg.ballot;
        rec->value_ballot = -1;
        rec->value = NULL;

        //LOG(DBG, ("Promising for instance %d with ballot %d, never seen before\n", msg.iid, msg.ballot));
        paxlog_update_record(*rec);

        promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot);
        promise_msgs.push_back(prom_msg);
        return;
    }

    //Handle a previously written instance
    if (msg.iid == rec->iid) {
        if (msg.ballot <= rec->ballot) {
            //LOG(DBG, ("Ignoring prepare for instance %d with ballot %d, already promised to %d\n", msg.iid, msg.ballot, rec->ballot));
            return;
        }
        //Answer if ballot is greater then last one
        //LOG(DBG, ("Promising for instance %d with ballot %d, previously promised to %d\n", msg.iid, msg.ballot, rec->ballot));
        rec->ballot = msg.ballot;
        paxlog_update_record(*rec);

        promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot);
        promise_msgs.push_back(prom_msg);
        return;
    }

    //Record was overwritten in memory, retrieve from disk
    if (msg.iid < rec->iid) {
        rec = paxlog_lookup_record(msg.iid);
        if(rec == NULL) {
            //No record on disk
            rec = new acceptor_record_t();
            rec->iid           = msg.iid;
            rec->ballot        = -1;
            rec->value_ballot  = -1;
            rec->value = NULL;
        }

        if(msg.ballot > rec->ballot) {
            rec->ballot = msg.ballot;
            paxlog_update_record(*rec);
            
            promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot);
            promise_msgs.push_back(prom_msg);
        } else {
            //LOG(DBG, ("Ignoring prepare for instance %d with ballot %d, already promised to %d [info from disk]\n", msg.iid, msg.ballot, rec->ballot));
        }
    }

    if(rec != NULL) {
        delete(rec);
    }

}

void acceptor_t::handle_prepare_batch(const struct prepare_batch_msg_t& prepare_batch_msg) {
  std::vector<prepare_msg_t> messages = prepare_batch_msg.messages;
  std::vector<promise_msg_t> promise_msgs;
  for (auto prep_msg : messages) {
    handle_prepare(prep_msg, promise_msgs);
  }
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
  lsync_to_tick = LSYNC_TICK;
}
