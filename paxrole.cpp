// TODO The implemation of opertaions taken by diffrent roles
#include "paxrole.h"
#include "paxserver.h"
#include "log.h"
#include <assert.h>
#include <stdio.h>
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
    proposer_array[i].promises.resize(server->get_serv_cnt(server->vc_state.view)+1);
  }
}

void leader_t::execute_phase1() {
  proposer_record_t *rec;

  int from = (p1info.highest_ready + 1);
  int to = (from + PROPOSER_PREEXEC_WIN_SIZE);
  vector<prepare_msg_t> messages;

  LOG(l::DEBUG, "(tick "<< server->net->now() <<")Leader pre-executing phase 1 from " << from << " to " << to << "\n");

  for(int i = from; i <= to; i++) {
    rec = &proposer_array[GET_PRO_INDEX(i)];

    if(rec->iid < i || rec->status == p1_new) {
      p1info.pending_count++;
      rec->iid = i;
      rec->status = p1_pending;
      rec->ballot = FIRST_BALLOT(server->nid);
      rec->promise_count = 0;
      for(auto promise : rec->promises) {
        promise.iid = -1;
        promise.value_ballot = -1;
        promise.value = NULL;
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
    LOG(l::DEBUG, "Leader periodic p2 check at tick " <<  server->net->now() << "\n");
    LOG(l::DEBUG, "No progress from last time, current_iid "<< current_iid << 
      " p2info.current_iid " << p2info.current_iid <<"\n");
    // TODO leaner should update leader::current_iid
   // int x;
   // std::cin >> x;

    if (current_iid == p2info.current_iid) {


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
  LOG(l::DEBUG, "Leader is resolving conflict for iid " << rec.iid << ", accepted ballot " 
    << rec.ballot << "accepted cid " << rec.cid <<" rid " << rec.rid <<"\n");
  server->broadcast<accept_msg_t>(rec.iid, rec.ballot, VALUE_OWNER(pi->value_ballot), rec.cid, rec.rid, pi->value);
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
    LOG(l::DEBUG, "(tick "<< server->net->now() <<")handle_promise: Promise for " << msg.iid << " ignored, no info in array\n");
    return;
  }

  //This promise is not relevant, because
  //we are not waiting for promises for instance iid
  if(rec.status != p1_pending) {
    LOG(l::DEBUG, "(tick "<< server->net->now() <<")handle_promise: Promise for " << msg.iid << " ignored, instance is not p1_pending\n");
    return;
  }

  //This promise is old, or belongs to another
  if(rec.ballot != msg.ballot) {
    LOG(l::DEBUG, "(tick "<< server->net->now() <<")handle_promise: Promise for " << msg.iid << " ignored, not our ballot\n");
    return;
  }

  //Already received this promise
  if(rec.promises[acceptor_id].iid != -1 && rec.ballot == msg.ballot) {
    LOG(l::DEBUG, "(tick "<< server->net->now() <<")handle_promise: Already received this promise " << server->nid << "\n");
    return;
  }

  rec.promises[acceptor_id] = msg;
  rec.promise_count++;

  if (rec.promise_count < server->get_quorum()) {
    LOG(l::DEBUG, "(tick "<< server->net->now() <<")handle_promise: Quorum not reached\n");
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
      phase2_check_cb();
      phase2_to_tick = PHASE2_TO_TICK;
      p2info.current_iid = current_iid;
      break;
    }
    default: {
      MASSERT(0, "Phase not recognized in leader timeout.\n Destruct sequence initiated. 3..2..1..run!\n");
    }
  }
}


void leader_t::leader_deliver_value(int iid) {
  current_iid = iid + 1;
  p1info.ready_count--;
}

/** proposer functions **/
proposer_t::proposer_t(paxserver *_server) {
  server = _server;
  fixed_ballot = MAX_PROPOSERS + server->get_nid(); //TODO: add MAX_PROPOSER to config
  current_iid = 0;
  has_value = false;
}

// handling single submit request at the same time
// TODO(drop the message if already working)
void proposer_t::proposer_submit_value(const struct execute_arg& ex_arg) {
  LOG(l::DEBUG, "Execute received from cid " << ex_arg.nid << ",rid " << ex_arg.rid <<" \n");
  if(has_value)
    LOG(l::DEBUG, "Current working iid" << current_iid << " cid " << current_cid <<",rid " << current_rid <<" \n");
  // TODO(goyalankit) UNCOMMENT THIS FOR MULTIPLE CLIENTS
  // HANDLE LEARN SHOULD UPDATE

  if (has_value) {
    // TODO send fail message so that client can send to another proposer
    
    server->net->drop(server,ex_arg,"");
    return;
  }
  has_value = true;

  last_accept_hash = 1;
  last_accept_iid = current_iid;
  current_cid = ex_arg.nid;
  current_rid = ex_arg.rid;

  ti.instance_id = current_iid;
  ti.hash = 1; // first_hash
  
  current_request = std::move(ex_arg.request);
  server->broadcast<accept_msg_t>(current_iid, fixed_ballot, server->get_nid(), ex_arg.nid, ex_arg.rid, current_request);
}

void proposer_t::do_proposer_timeout() {

  //Nothing new since our last accept, retry to send it
  if( has_value && ti.instance_id == last_accept_iid && ti.hash == last_accept_hash) {
    LOG(l::DEBUG, "S0" << server->nid <<" Instance: " << ti.instance_id <<" timed-out ( " << last_accept_hash << " times)"
      ", re-sending accept, cid " << current_cid << " rid " << current_rid <<"\n");
    last_accept_hash++;
    ti.hash = last_accept_hash;
    //Resend in same instance
    LOG(l::DEBUG, "Proposer: " << server->get_nid() << "\n");
    server->broadcast<accept_msg_t>(current_iid, fixed_ballot, server->get_nid(), current_cid, current_rid, current_request);
    proposer_to_tick = PROPOSER_TO_TICK;
  } else {
    LOG(l::DEBUG, "Timeout out-of-date, removed\n");
    //Something changed, stop
    // (decided or outdated timeout)
    // TODO: need to check how to handle this.
    //free(ti);
  }


}

void proposer_t::deliver_function(paxobj::request req, int iid, int ballot, node_id_t cid, rid_t rid, int proposer) {
    
    int just_accepted;

    if (server->primary()) {
        server->leader->leader_deliver_value(iid);
    }

    just_accepted = current_iid;
    current_iid++;

    assert(just_accepted == iid);
    if(!has_value) {
      LOG(l::DEBUG, "Value delivered for iid"<< just_accepted << "for client: " << cid << " rid: " << rid <<" accepted, client is not waiting\n");
      return;
    }

    if (has_value && current_cid == cid && current_rid == rid) {

      auto result = server->paxop_on_paxobj(req, cid, rid);
      has_value = false;

      // Send response back to client
      LOG(l::DEBUG, "Sending response back for client: " << cid << " for rid: " << rid << " iid: " << iid << "\n");
      auto ex_success = std::make_unique<struct execute_success>(result, rid);
      
      server->send_msg(cid, std::move(ex_success));
    } else {
        //Send for next instance
        LOG(l::DEBUG, "Someone else's value delivered my id" << server->nid <<" (iid: "<< current_iid << "), try next instance.\n");
        
        last_accept_iid = just_accepted;
        last_accept_hash++;
        server->broadcast<accept_msg_t>(current_iid, fixed_ballot, server->get_nid(), current_cid, current_rid, current_request);
        proposer_to_tick = PROPOSER_TO_TICK;

        ti.instance_id = current_iid;
        ti.hash = last_accept_hash;

    }
}

/** acceptor functions **/
acceptor_t::acceptor_t(paxserver *_server){
  server = _server;
}

void acceptor_t::apply_accept(acceptor_record_t *rec, const accept_msg_t *amsg) {
  //Update record
  rec->iid = amsg->iid;
  rec->ballot = amsg->ballot;
  rec->value_ballot = amsg->ballot;
  rec->proposer_id = amsg->proposer_id;
  rec->value = amsg->value;
  rec->cid = amsg->cid;
  rec->rid = amsg->rid;
  paxlog_update_record(*rec);
}

void acceptor_t::handle_accept(const struct accept_msg_t& amsg) {
  LOG(l::DEBUG, ("Received accept message\n"));
  acceptor_record_t* rec;
  //Lookup
  rec = &acceptor_array[GET_ACC_INDEX(amsg.iid)];

  //Found record previously written
  if (amsg.iid == rec->iid) {
    LOG(l::DEBUG, "|HANDLE_ACCEPT| " << "amsg.iid" << amsg.iid << " rec.iid" << rec->iid  << " any"<< rec->any_enabled << "; Ballot: " << amsg.ballot << "; rec->ballot" << rec->ballot << "; Min ballot" << min_ballot<< "\n");
    if (rec->any_enabled || (amsg.ballot >= rec->ballot && amsg.ballot >= min_ballot)) {
      if (rec->any_enabled) {
        rec->any_enabled = 0;
        LOG(l::DEBUG, "Accepting value for instance " << amsg.iid << ", any message enabled\n");
      } else {
        LOG(l::DEBUG, "Accepting value for instance " << amsg.iid << " with ballot "<< amsg.ballot << "\n");
      }
      apply_accept(rec, &amsg);
      server->broadcast<learn_msg_t>(server->nid, rec->iid, rec->ballot,
          rec->proposer_id, rec->cid, rec->rid, rec->value );
      return;
    }

    LOG(l::DEBUG, "Ignoring value for instance " << amsg.iid << " with ballot "<< amsg.ballot << "\n");
    return;
  }

  //Record not found in acceptor array
  if (amsg.iid > rec->iid) {
    if (amsg.ballot >= min_ballot) {
      LOG(l::DEBUG, "Accepting value for instance " << amsg.iid << " with ballot, "<< amsg.ballot << " never seen before\n");
      apply_accept(rec, &amsg);
      server->broadcast<learn_msg_t>(server->nid, rec->iid, rec->ballot,
          rec->proposer_id, rec->cid, rec->rid, rec->value );
      return;
    } else {
      LOG(l::DEBUG, "Ignoring value for instance " << amsg.iid << " with ballot "<< amsg.ballot << " never seen before\n");
      return;
    }
  }

  //Record was overwritten in acceptor array
  //We must scan the logfile before accepting or not
  if (amsg.iid < rec->iid) {
    rec = paxlog_lookup_record(amsg.iid);
    if (rec == NULL) {
      if (amsg.ballot >= min_ballot) {
        LOG(l::DEBUG, "Accepting value for instance " << amsg.iid << " with ballot, "<< amsg.ballot << " never seen before[infor from disk]\n");
        apply_accept(rec, &amsg);

        server->broadcast<learn_msg_t>(server->nid, rec->iid, rec->ballot,
            rec->proposer_id, rec->cid, rec->rid, rec->value );
        return;
      } else {
        LOG(l::DEBUG, "Ignoring value for instance " << amsg.iid << " with ballot, "<< amsg.ballot << " never seen before[infor from disk]\n");
        return;
      }
    }

    if (rec->any_enabled || (amsg.ballot >= rec->ballot && amsg.ballot >= min_ballot)) {
      if (rec->any_enabled) {
        rec->any_enabled = 0;
        LOG(l::DEBUG, "Accepting value for instance " << amsg.iid << ", any message enabled" << "\n");
      } else {
        LOG(l::DEBUG, "Accepting value for instance " << amsg.iid << " with ballot, "<< amsg.ballot << " never seen before[infor from disk]\n");
      }

      apply_accept(rec, &amsg);
      server->broadcast<learn_msg_t>(server->nid, rec->iid, rec->ballot,
          rec->proposer_id, rec->cid, rec->rid, rec->value );
    } else {
      LOG(l::DEBUG, "Ignoring accept for instance " << amsg.iid << " with ballot, "<< amsg.ballot << " already given to ballot" << rec->ballot << " [infor from disk]\n");
    }
    delete rec;
  }
  return;
}


void acceptor_t::paxlog_update_record(acceptor_t::acceptor_record_t &rec) {
  (server->paxlog).fastlog(rec.cid, rec.rid, rec.iid, rec.ballot, rec.value_ballot,
      rec.value, server->net->now());
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
  //Update record
  rec->iid = iid;
  rec->ballot = ballot;
  rec->value_ballot = -1;
  rec->proposer_id = -1;
  rec->value = NULL;
  rec->any_enabled = 1;

  //Write to disk
  paxlog_update_record(*rec);
  LOG(l::DEBUG, "Anyval for iid: "<< rec->iid << " applied\n" );
}

void acceptor_t::handle_anyval_batch(const struct anyval_batch_msg_t& anyval_msg) {
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

    acceptor_record_t* rec;
    //Lookup
    rec = &acceptor_array[GET_ACC_INDEX(iid)];

    //Found record previously written
    if (iid == rec->iid) {
      if (rec->any_enabled || (ballot >= rec->ballot)) {
        LOG(l::DEBUG, "Accepting anyval for instance" << iid <<  " with ballot " << ballot << "\n" );
        apply_anyval(rec, iid, ballot);
        continue;
      }

      LOG(l::DEBUG, "Ignoring anyval for instance" << iid <<  " with ballot " << ballot << "\n" );
      continue;
    }

    //Record not found in acceptor array
    if (iid > rec->iid) {
      LOG(l::DEBUG, "Accepting anyval for instance" << iid <<  " with ballot " << ballot << ", never seen before\n" );
      apply_anyval(rec, iid, ballot);
      continue;
    }

    //Record was overwritten in acceptor array
    //We must scan the logfile before accepting or not
    if (iid < rec->iid) {
      rec = paxlog_lookup_record(iid);
      if (rec == NULL) {
        LOG(l::DEBUG, "Accepting anyval for instance" << iid <<  " with ballot " << ballot << ", never seen before\n" );
        apply_anyval(rec, iid, ballot);
        continue;
      }

      if (rec->any_enabled || (ballot >= rec->ballot)) {
        LOG(l::DEBUG, "Accepting anyval for instance" << iid <<  " with ballot " << ballot << " [info from disk]\n" );
        apply_anyval(rec, iid, ballot);
      } else {
        LOG(l::DEBUG, "Ignoring anyval for instance" << iid <<  " with ballot " << ballot << ", already given to ballot " << rec->ballot << " [info from disk]\n" );
      }

      delete rec;
      continue;
    }
    continue;
  }
  return;
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
        rec->cid = -1;
        rec->rid = -1;
        //LOG(DBG, ("Promising for instance %d with ballot %d, never seen before\n", msg.iid, msg.ballot));
        paxlog_update_record(*rec);

        promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot, rec->cid, rec->rid, rec->value);
        promise_msgs.push_back(prom_msg);
        return;
    }

    //Handle a previously written instance
    if (msg.iid == rec->iid) {
        if (msg.ballot <= rec->ballot) {
            LOG(l::DEBUG, "Ignoring prepare for instance " << msg.iid <<" with ballot "
                << msg.ballot <<", already promised to " << rec->ballot << "\n");
            return;
        }
        //Answer if ballot is greater then last one
        LOG(l::DEBUG, "Promising for instance " << msg.iid <<" with ballot " << 
            msg.ballot << ", previously promised to " << rec->ballot << "\n");
        rec->ballot = msg.ballot;
        paxlog_update_record(*rec);

        promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot, rec->cid, rec->rid, rec->value);


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
            rec->cid = -1;
            rec->value = NULL;
        }

        if(msg.ballot > rec->ballot) {
            rec->ballot = msg.ballot;
            paxlog_update_record(*rec);

            promise_msg_t prom_msg(rec->iid, rec->ballot, rec->value_ballot, rec->cid, rec->rid, rec->value);
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

void acceptor_t::handle_lsync(const learner_sync_msg_t & lsmsg) {
  for (auto iid : lsmsg.iids) {
    acceptor_record_t* rec;
    //Lookup
    rec = &acceptor_array[GET_ACC_INDEX(iid)];

    //Found record in memory
    if (iid == rec->iid) {
      //A value was accepted
      if (rec->value != NULL) {
        LOG(l::DEBUG, "Answering to lsync for instance: " << iid << "\n");
        // broadcast learn_msg
        server->broadcast<learn_msg_t>(server->get_nid(), rec->iid, rec->ballot, rec->proposer_id, rec->cid, rec->rid, rec->value);
        continue;
      }
      //No value was accepted
      LOG(l::DEBUG, "Ignoring lsync for instance: " << iid <<" ,record present but no value accepted\n");
      continue;
    }

    //Record was overwritten in acceptor array
    //We must scan the logfile before answering
    if (iid < rec->iid) {
      rec = paxlog_lookup_record(iid);
      //A record was found, we accepted something 
      if (rec != NULL) {
        LOG(l::DEBUG, "Answering to lsync for instance: " << iid <<" [info from disk]\n");
        server->broadcast<learn_msg_t>(server->get_nid(), rec->iid, rec->ballot, rec->proposer_id, rec->cid, rec->rid, rec->value);
        delete rec;
        continue;
      } else {
        //Nothing from disk, nothing accepted
        LOG(l::DEBUG, "Ignoring lsync for instance: " << iid <<" , no value accepted [info from disk]\n");
      }
    }

    //Record not found in acceptor array
    //Nothing was accepted
    LOG(l::DEBUG, "Ignoring lsync for instance: "<< iid <<" , no info is available\n");
    continue;
  }
return;
}


/** learner functions **/
learner_t::learner_t(paxserver *_server){
  server = _server;
  lsync_to_tick = LSYNC_TICK;
  for (int i = 0; i < LEARNER_ARRAY_SIZE; i++){
    learner_array[i] = {};
    learner_array[i].learns.resize(server->get_serv_cnt(server->vc_state.view)+1);
  }
}

void learner_t::do_learner_timeout() {
  // reset the timer
  lsync_to_tick = LSYNC_TICK;
  if (highest_seen > highest_delivered) {
    learner_record_t* rec;
    std::vector<int> lsync_iids;
    for(int i = highest_delivered+1; i < highest_seen; i++) {
      rec = &learner_array[GET_LEA_INDEX(i)];

      //Not closed or never seen, request sync
      if((rec->iid == i && !is_closed(rec)) || rec->iid < i) {
        LOG(l::DEBUG, "Adding" << i << "to next lsync message" << "\n");
        lsync_iids.push_back(i);
      }
    }

    //Flush send buffer if not empty
    if(!lsync_iids.empty()) {
      server->broadcast<learner_sync_msg_t>(lsync_iids);
      LOG(l::DEBUG, "Requested " << lsync_iids.size() <<" lsyncs from " << highest_delivered+1 << " to " << highest_seen << "\n");
    }
  }
}


int learner_t::add_learn_to_record(learner_record_t *rec, const learn_msg_t* lmsg) {
  learn_msg_t * old_learn, * new_learn;

  //Check that is in bounds
  if(lmsg->acceptor_id < 0 || lmsg->acceptor_id >= (int)(server->get_serv_cnt(server->vc_state.view) + 1)) {
    MASSERT(0,"Error: received faulty acceptor_id\n");
    LOG(l::DEBUG, "Dropping learn for instance too far in future: " <<  lmsg->acceptor_id << "\n");
    return 0;
  }

  old_learn = rec->learns[lmsg->acceptor_id];

  //First learn from this acceptor for this instance
  if(old_learn == NULL) {
    //LOG(DBG, "Got first learn for instance:%d, acceptor:%d\n", rec->iid,  lmsg->acceptor_id));
    //LOG(l::DEBUG, "Accepting anyval for instance" << iid <<  " with ballot " << ballot << ", never seen before\n" );
    new_learn = new learn_msg_t(*lmsg);
    rec->learns[lmsg->acceptor_id] = new_learn;
    return 1;
  }

  //This message is old, drop
  if(old_learn->ballot >= lmsg->ballot) {
    // LOG(DBG, ("Dropping learn for instance:%d, more recent ballot already seen\n", rec->iid));
    return 0;
  }

  //This message is relevant! Overwrite previous learn
  // LOG(DBG, ("Overwriting previous learn for instance %d\n", rec->iid));
  delete(old_learn);
  new_learn = new learn_msg_t (*lmsg);
  rec->learns[lmsg->acceptor_id] = new_learn;
  return 1; 

}

bool learner_t::is_closed(learner_record_t* rec) {
    return (rec->final_value != NULL);
}

int learner_t::update_record(const learn_msg_t *lmsg) {
  learner_record_t * rec;

  //We are late, drop to not overwrite
  if(lmsg->iid >= highest_delivered + LEARNER_ARRAY_SIZE) {
    LOG(l::DEBUG, "Dropping learn for instance too far in future: " <<  lmsg->iid << "\n");
    return 0;
  }

  //Already sent to client, drop
  if(lmsg->iid <= highest_delivered) {
    LOG(l::DEBUG, "Dropping learn for already enqueued instance: " <<  lmsg->iid << "\n");
    return 0;
  }

  //Retrieve record for this iid
  rec = &learner_array[GET_LEA_INDEX(lmsg->iid)];

  //First message for this iid
  if(rec->iid != lmsg->iid) {
    LOG(l::DEBUG, "Received first message for instance: " <<  lmsg->iid << "\n");

    //Clean record
    rec->iid = lmsg->iid;
    rec->final_value.reset();
    for (auto iter = rec->learns.begin(); iter!=rec->learns.end();++iter) {
      *iter = NULL;
    }

    //Add
    return add_learn_to_record(rec, lmsg);

    //Found record containing some info
  } else {
    if(is_closed(rec)) {
      //LOG(DBG, ("Dropping learn for closed instance:%d\n", lmsg->iid));
      return 0;
    }
    //Add
    return add_learn_to_record(rec, lmsg);
  }
}

//Checks if we have a quorum and returns 1 if the case
bool learner_t::check_quorum(const learn_msg_t* lmsg) {
    unsigned int count = 0;
    learner_record_t* rec;
    rec = &learner_array[GET_LEA_INDEX(lmsg->iid)];
    for (auto learn : rec->learns) {
        //No value
        if(learn == NULL)
            continue;
        //Different value
        if(learn->ballot != lmsg->ballot)
            continue;
        //Same ballot
        count++;
    }
    //Mark as closed, we have a value
    if (count >= server->get_quorum()) {
      LOG(l::DEBUG, "Reached quorum, instance: " << rec->iid << "is now closed!\n");
      rec->ballot = lmsg->ballot;
      rec->proposer_id = lmsg->proposer_id;
      rec->final_value = lmsg->value;
      rec->rid = lmsg->rid;
      rec->cid = lmsg->cid;
      return true;
    }
    return false;
}


void learner_t::deliver_values(int iid) {
  while(1) {
    learner_record_t* rec = &learner_array[GET_LEA_INDEX(iid)];

    if(!is_closed(rec)) break;



    server->proposer->deliver_function(rec->final_value, rec->iid, rec->ballot, rec->cid, rec->rid, rec->proposer_id);
    
    //void proposer_t::deliver_function(paxobj::request *req, int iid, int ballot, node_id_t cid, rid_t rid, int proposer) {
    //delfun(deliver_buffer, rec->final_value_size, instance_id, rec->ballot, rec->proposer_id);
    rec->final_value.reset();
    highest_delivered = iid;
    iid++;
  }
}

void learner_t::handle_learn_msg(const struct learn_msg_t& lmsg) {
  if (lmsg.iid > highest_seen) {
    highest_seen = lmsg.iid;
  }

  //for each message update instance record
  int relevant = update_record(&lmsg);
  if(!relevant) {
    LOG(l::DEBUG, "Learner discarding learn for instance " << lmsg.iid << "\n");
    return;
  }

  //if quorum reached, close the instance
  int closed = check_quorum(&lmsg);
  if(!closed) {
    LOG(l::DEBUG, "Not yet a quorum for instance " << lmsg.iid << "\n");
    return;
  }

  //If the closed instance is last delivered + 1
  if (lmsg.iid == highest_delivered+1) {
    deliver_values(lmsg.iid);
  }
}
