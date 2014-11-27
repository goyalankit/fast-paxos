// TODO The implemation of opertaions taken by diffrent roles
#include "paxrole.h"
#include "paxserver.h"
#include "log.h"
#include <assert.h>

/** leader functions **/
leader_t::leader_t(paxserver *_server) {
  server = _server;
  do_first_TO = true;

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

void leader_t::phase1_check_cb(){
    //Check answer from last time
    std::vector<prepare_msg_t> messages;
    for(int i = p1info.first_to_check; i <= p1info.last_to_check; i++) {
        proposer_record_t & rec = proposer_array[GET_PRO_INDEX(i)];

        if (rec.iid == i && rec.status == p1_pending) {
            //add prepare to sendbuf
            rec.ballot = BALLOT_NEXT(rec.ballot);
            rec.promise_count = 0;
            for(auto promise : rec.promises) {
                promise.iid = -1;
                promise.value_ballot = -1;
                promise.value = NULL;
            }
            prepare_msg_t msg (rec.iid,rec.ballot);
            messages.push_back(msg);
        }
    }

    // LOG(V_VRB, ("pending count: %d\n", p1info.pending_count));
    // LOG(V_VRB, ("ready count: %d\n", p1info.ready_count));

    if (p1info.pending_count + p1info.ready_count < (PROPOSER_PREEXEC_WIN_SIZE/2)) {
        execute_phase1(); //add some stuff to sendbuf
    }

    if(!messages.empty()) {
      server->broadcast<prepare_batch_msg_t> (messages);
    }
}


leader_t::promise_info_t* leader_t::phase2_getMax_promise(proposer_record_t &rec) {
  int max_ballot = -1;
  promise_info_t* pi = NULL;

  for (auto prom_info : rec.promises) {
    if (prom_info.iid == -1 )
      continue;

    if (prom_info.value_ballot != -1) {
      if(prom_info.value_ballot > max_ballot) {
        max_ballot = prom_info.value_ballot;
        pi = &prom_info;
      }
    }
  }

  return pi;
}

void leader_t::phase2_check_cb(){
    // LOG(DBG, ("Leader periodic p2 check\n"));

    // TODO leaner should update leader::current_iid
    if (current_iid == p2info.current_iid) {
        // LOG(DBG, ("No progress from last time!\n"));

        //Check in learner:
        //Quorum has my ballot?

        // Y -> directly 2a with v (collision)

        // N -> (my any was lost?
                // another leader?
                // no proposer sending value?)
        //Restart from p1
        // (collect promises and send any/accept)
        std::vector<prepare_msg_t>  messages;

        proposer_record_t * rec = &proposer_array[GET_PRO_INDEX(current_iid)];
        MASSERT(rec->iid == current_iid, "phase2_check_cb(): current_iid Mismatch\n");

        if(rec->status == p1_finished) {
            p1info.ready_count--;
            p1info.pending_count++;
            rec->status = p1_pending;
        }

        rec->ballot = BALLOT_NEXT(rec->ballot);
        rec->promise_count = 0;

        for(auto promise : rec->promises) {
            promise.iid = -1;
            promise.value_ballot = -1;
            promise.value = NULL;
        }
        messages.emplace_back(rec->iid,rec->ballot);
        server->broadcast<prepare_batch_msg_t>(messages);
    }
}

void leader_t::resolve_cflt_send_accept
(proposer_record_t & rec, promise_info_t * pi){
  server->broadcast<accept_msg_t>(rec.iid,rec.ballot,VALUE_OWNER(pi->value_ballot),pi->value);
}

void leader_t::execute_phase2(int first_iid, int last_iid) {

  int ballot = -1;
  promise_info_t* pi = NULL;
  std::vector<int> messages;

  for(int i = first_iid; i <= last_iid; ++i) {
    proposer_record_t &rec = proposer_array[GET_PRO_INDEX(i)];
    if (rec.status != p1_ready)
      continue;

    if ((pi = phase2_getMax_promise(rec)) == NULL) {
      if (rec.ballot > ballot) {
        ballot = rec.ballot;
      }
      messages.push_back(i);
    }
    else {
      // TODO resolve conflict
      resolve_cflt_send_accept(rec, pi);
    }

    rec.status = p1_finished;
  }

  if (!messages.empty()) {
    server->broadcast<anyval_batch_msg_t>(messages, ballot);
  }
}

void leader_t::handle_promise(const struct promise_msg_t &msg, int acceptor_id, struct proposer_record_t &rec){

  //Ignore because there is no info about
  //this iid in proposer_array
  if(rec.iid != msg.iid) {
    //LOG(DBG, ("P: Promise for %d ignored, no info in array\n", prom->iid));
    return;
  }

  //This promise is not relevant, because
  //we are not waiting for promises for instance iid
  if(rec.status != p1_pending) {
    //LOG(DBG, ("P: Promise for %d ignored, instance is not p1_pending\n", prom->iid));
    return;
  }

  //This promise is old, or belongs to another
  if(rec.ballot != msg.ballot) {
    //LOG(DBG, ("P: Promise for %d ignored, not our ballot\n", prom->iid));
    return;
  }

  //Already received this promise
  if(rec.promises[acceptor_id].iid != -1 && rec.ballot == msg.ballot) {
    //LOG(DBG, ("P: Promise for %d ignored, already received\n", prom->iid));
    return;
  }

  rec.promises[acceptor_id] = msg;
  rec.promise_count++;
  if (rec.promise_count < server->get_quorum()) {
    //LOG that quorum not reached yet.
    return;
  }

  // quorum reached.
  rec.status = p1_ready;
  p1info.pending_count--;
  p1info.ready_count++;
  if (p1info.highest_ready < rec.iid) {
    p1info.highest_ready = rec.iid;
  }
  if (p1info.last_to_check < rec.iid) {
    p1info.last_to_check = rec.iid;
  }
  return;
}

void leader_t::handle_promise_batch(const struct promise_batch_msg_t &promise_batch_msg) {
  int first_iid = promise_batch_msg.messages.front().iid;
  int last_iid = promise_batch_msg.messages.back().iid;
  proposer_record_t rec;

  for (auto prom_msg : promise_batch_msg.messages){
    handle_promise(prom_msg, promise_batch_msg.acceptor_id, proposer_array[GET_PRO_INDEX(prom_msg.iid)]);
  }
  execute_phase2(first_iid, last_iid);
}

void leader_t::do_leader_timeout(phase12_t phase) {
  switch (phase){
    case phase12_t::phase1:{

      if(do_first_TO) {
        execute_phase1();
        do_first_TO = false;
      } else {
        // check phase1 cb
        phase1_check_cb();
      }
      phase1_to_tick = PHASE1_TO_TICK;

      break;
    }
    case phase12_t::phase2:{
      phase2_to_tick = PHASE2_TO_TICK;
      phase2_check_cb();
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
  server->broadcast<accept_msg_t>(current_iid, fixed_ballot, server->get_nid(),ex_arg.request);
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


void acceptor_t::paxlog_update_record(acceptor_t::acceptor_record_t &rec) {
  
}


acceptor_t::acceptor_record_t * acceptor_t::paxlog_lookup_record(int iid) {
  const Paxlog::tup* tup = (server->paxlog).get_tup(iid);
  if (tup == nullptr) {
    return nullptr;
  } else {
    return (new acceptor_record_t(tup));
  }
}

void acceptor_t::apply_anyval(acceptor_record_t * rec, int iid, int ballot) {
}

int acceptor_t::handle_anyval_batch(const struct anyval_batch_msg_t& anyval_msg) {
  std::vector<int> iids = anyval_msg.messages;
  int ballot = anyval_msg.ballot;
  for (int iid : iids) {
    // found in array ->
    // not in array
    // -> new
    // -> old
    // fetch from disk
    //  -> found
    //  -> not found

    // found  -> check ballot and enabled
    // not found -> return 0

    int ret;
    acceptor_record_t* rec;

    //Lookup
    rec = &acceptor_array[GET_ACC_INDEX(iid)];

    //Found record previously written
    if (iid == rec->iid) {
      if (rec->any_enabled || (ballot >= rec->ballot)) {
        //LOG(DBG, ("Accepting anyval for instance %d with ballot %d\n", iid, ballot));
        apply_anyval(rec, iid, ballot);
        return 1;
      }

      //LOG(DBG, ("Ignoring anyval for instance %d with ballot %d.\n", iid, ballot));
      return 0;
    }

    //Record not found in acceptor array
    if (iid > rec->iid) {
      //LOG(DBG, ("Accepting anyvalue instance %d with ballot %d, never seen before\n", iid, ballot));
      apply_anyval(rec, iid, ballot);
      return 1;
    }

    //Record was overwritten in acceptor array
    //We must scan the logfile before accepting or not
    if (iid < rec->iid) {
      rec = paxlog_lookup_record(iid);
      if (rec == NULL) {
        // LOG(DBG, ("Accepting anyvalue instance %d with ballot %d, never seen before\n", iid, ballot));
        apply_anyval(rec, iid, ballot);
        return 1;
      }

      if (rec->any_enabled || (ballot >= rec->ballot)) {
        // LOG(DBG, ("Accepting anyval for instance %d with ballot %d [info from disk]\n", iid, ballot));
        apply_anyval(rec, iid, ballot);
        ret = 1;
      } else {
        // LOG(DBG, ("Ignoring anyval for instance %d with ballot %d, already given to ballot %d [info from disk]\n", iid, ballot, rec->ballot));
        ret = 0;
      }

      delete rec;
      return ret;
    }
    return 0;
  }
  return 1;
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

        promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot, rec->value);
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

        promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot, rec->value);
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

            promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot, rec->value);
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
  
  if(!promise_msgs.empty()) {
    node_id_t leader_id = server->vc_state.view.primary;
    auto promise_msg = std::make_unique<promise_batch_msg_t>(promise_msgs, server->nid);
    server->send_msg(leader_id, std::move(promise_msg));
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
