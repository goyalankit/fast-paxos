// paxmsg.h - Defines for messages sent in the Paxos algorithm
#pragma once

#include <stdint.h> 
#include <vector>
#include <string>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <iostream>

#include "node.h"
#include "net.h"
#include "paxtypes.h"
#include "make_unique.h"
#include "massert.h"
#include "paxobj.h"
#include "paxlog.h"

// Abstract paxos message type, with an RPC id.  Not very object oriented
// but different request/response messages don't really exist in a single
// type hierarchy
struct paxmsg_t : public net_msg_t {
  paxmsg_t(const char* _descr, int _rpc_id) : 
    net_msg_t(_descr) {
      rpc_id = _rpc_id;
    }
  int rpc_id;
};

/* Fast Paxos Messages START */

struct prepare_msg_t {
  int iid;
  int ballot;

  prepare_msg_t(int _iid, int _ballot) {
    iid = _iid;
    ballot = _ballot;
  }

  void pr(std::ostream& os) const {
    os << "{" << iid << ", " << ballot << "}";
  }
};

struct prepare_batch_msg_t : public paxmsg_t {
  static const int  ID = 20;
  static constexpr const char* _descr = "prepareBatchMsg";

  std::vector<prepare_msg_t> messages;

  prepare_batch_msg_t(std::vector<prepare_msg_t> & _messages) : paxmsg_t(_descr, ID) {
    messages = _messages;
  }

  void pr(std::ostream& os) const {
    for (auto msg : messages) {
      msg.pr(os);
    }
  }
};

struct promise_msg_t {
  int     iid;
  int     ballot;
  int     value_ballot;
  int     cid;
  int     rid;
  paxobj::request value;

  promise_msg_t(int _iid, int _ballot, int _value_ballot, int _cid, int _rid, paxobj::request _value) {
    iid          = _iid;
    ballot       = _ballot;
    value_ballot = _value_ballot;
    cid          = _cid;
    rid          = _rid;
    value        = _value;
  }

  void pr(std::ostream& os) const {
    os << "{" << iid << ", " << ballot  << ", " <<
      value_ballot << "}";
  }
};

struct promise_batch_msg_t : public paxmsg_t {
  static const int ID = 21;
  static constexpr const char* _descr = "promiseBatchMsg";
  std::vector<promise_msg_t> messages;
  int acceptor_id;
  promise_batch_msg_t(std::vector<promise_msg_t> & _messages, int _id) : paxmsg_t(_descr, ID) {
    messages = _messages;
    acceptor_id = _id;
  }

  void pr(std::ostream& os) const {
    for (auto msg : messages) {
      msg.pr(os);
    }
  }
};

struct accept_msg_t : public paxmsg_t {
  static const int ID = 22;
  static constexpr const char* _descr = "acceptMsg";
  int     iid;
  int     ballot;
  int     proposer_id;
  int     cid;
  int     rid;
  paxobj::request value;
  accept_msg_t(int _iid, int _ballot, int _proposer_id, int _cid, int _rid, paxobj::request _value) :
    paxmsg_t(_descr, ID) {
      iid         = _iid;
      ballot      = _ballot;
      cid         = _cid;
      rid         = _rid;
      proposer_id = _proposer_id;
      value       = _value;
    }

  void pr(std::ostream& os) const {
    os << "{" << iid << ", " << ballot  << ", " <<
      proposer_id << "}";
  }
};

struct learner_sync_msg_t : public paxmsg_t {
  static const int ID = 23;
  static constexpr const char* _descr = "learnerSyncMsg";
  int     count;

  learner_sync_msg_t(int _count) : paxmsg_t(_descr, ID) {
    count = _count;
  }

  void pr(std::ostream& os) const {
    os << "{" << count << "}";
  }
};

struct learn_msg_t : public paxmsg_t {
  static const int ID = 24;
  static constexpr const char* _descr = "learnerMsg";
  int     acceptor_id;
  int     iid;
  int     ballot;
  int     proposer_id;
  paxobj::request value;
  int cid;
  int rid;

  learn_msg_t(int _acceptor_id, int _iid, int _ballot, int _proposer_id, int _cid, int _rid, paxobj::request _value) :
    paxmsg_t(_descr, ID) {
      acceptor_id = _acceptor_id;
      iid = _iid;
      ballot = _ballot;
      proposer_id = _proposer_id;
      value = _value;
    }

  void pr(std::ostream& os) const {
    os << "{" << acceptor_id << ", " << iid << ", " << ballot << ", "
      << proposer_id << "}";
  }
};

struct anyval_batch_msg_t : public paxmsg_t {
  static const int ID = 25;
  static constexpr const char* _descr = "anyvalMsg";
  std::vector<int> messages;
  int     ballot;
  anyval_batch_msg_t(std::vector<int> & _messages, int _ballot) : paxmsg_t(_descr, ID) {
    messages = _messages;
    ballot = _ballot;
  }

  void pr(std::ostream& os) const {
    for (auto msg : messages) {
      os << "{" << msg <<"}";
    }
  }
};
/* Fast Paxos Messages END */

// nop message, used as heartbeat
struct nop_msg : public paxmsg_t {
  static const int ID = 1;
  static constexpr const char* _descr = "nop";
  nop_msg() : paxmsg_t(_descr, ID) {
  }

  void pr(std::ostream& os) const {
    os << "{" << _descr << "}";
  }
};

struct execute_arg : public paxmsg_t {
  static const int ID = 3;
  static constexpr const char* _descr = "executeARG";
  execute_arg(node_id_t _nid, rid_t _rid, viewid_t _vid,
      paxobj::request _request)
    : paxmsg_t(_descr, ID),
    request(_request)
  {
    nid = _nid;
    rid = _rid;
    vid = _vid;
    request = _request;
  }
  execute_arg() = delete;
  node_id_t nid;
  rid_t rid;
  viewid_t vid;
  paxobj::request request;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " nid:" << nid
      << "\n    rid:" << rid << " vid: " << vid << "}";
  }
};

struct replicate_arg : public paxmsg_t {
  static const int ID = 6;
  static constexpr const char* _descr = "replicaARG";
  replicate_arg(const viewstamp_t& _vs, const execute_arg& _arg,
      const viewstamp_t& _committed) : 
    paxmsg_t(_descr, ID),
    vs(_vs),
    arg(_arg.nid, _arg.rid, _arg.vid, _arg.request),
    committed(_committed)
  {
  }
  viewstamp_t vs;
  execute_arg arg;
  viewstamp_t committed;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " vs: " << vs
      << "\n    arg: " << arg << "\n    comm: " << committed << "}";
  }
};

struct replicate_res : public paxmsg_t {
  static const int ID = 7;
  static constexpr const char* _descr = "replicaRES";
  replicate_res(const viewstamp_t& _vs) : 
    paxmsg_t(_descr, ID),
    vs(_vs)
  {
  }
  viewstamp_t vs;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " vs: " << vs
      << "}";
  }
};

// The committed viewstamp is piggybacked on replicate_arg,
// but when there are no more requests, we need an explicit 
// accept message.  Backups need not reply
struct accept_arg : public paxmsg_t {
  static const int ID = 8;
  static constexpr const char* _descr = "accept ARG";
  accept_arg(const viewstamp_t& _committed) : 
    paxmsg_t(_descr, ID),
    committed(_committed)
  {
  }
  viewstamp_t committed;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " committed: " << committed << "}";
  }
};

struct execute_success : public paxmsg_t {
  static const int ID = 4;
  static constexpr const char* _descr = "executeSUC";
  execute_success(std::string _reply, rid_t _rid) :
    paxmsg_t(_descr, ID),
    reply(_reply),
    rid(_rid)
  {
  }
  // Not quite the binary blob of Mazieres, but close enough in C++
  std::string reply;
  // This is not in Mazieres, but I find it convenient for client
  rid_t rid;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " rid: " << rid
      << " reply: " << reply << "}";
  }
};

struct execute_fail : public paxmsg_t {
  static const int ID = 5;
  static constexpr const char* _descr = "executeFAI";
  execute_fail(const viewid_t& _vid, node_id_t _primary, rid_t _rid) : 
    paxmsg_t(_descr, ID),
    vid(_vid),
    primary(_primary),
    rid(_rid)
  {
  }
  viewid_t vid;
  node_id_t primary;
  // Not in Mazieres, but my clients use it
  rid_t rid;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " vid: " << vid
      << " pr: " << primary << "}";
  }
};

struct view_change_arg : public paxmsg_t {
  static const int ID = 9;
  static constexpr const char* _descr = "view_chARG";
  view_change_arg(const view_t& _oldview, const viewid_t& _newvid) :
    paxmsg_t(_descr, ID),
    oldview(_oldview),
    newvid(_newvid)
  {
  }
  view_t oldview;
  viewid_t newvid;
  void pr(std::ostream& os) const override {
    os << "{" << _descr << "\n    oldview: " << oldview
      << "\n    newvid: " << newvid << "}";
  }
};

struct view_change_reject : public paxmsg_t {
  static const int ID = 10;
  static constexpr const char* _descr = "view_chREJ";
  view_change_reject(const view_t& _oldview, const viewid_t& _newvid) :
    paxmsg_t(_descr, ID),
    oldview(_oldview),
    newvid(_newvid)
  {
  }
  view_t oldview;
  viewid_t newvid;
  void pr(std::ostream& os) const {
    os << "{" << _descr << "\n    oldview: " << oldview
      << "\n     newvid: " << newvid << "}";
  }
};
struct view_change_accept : public paxmsg_t {
  static const int ID = 11;
  static constexpr const char* _descr = "view_chACC";
  view_change_accept() : paxmsg_t(_descr, ID) {}
  view_change_accept(const view_change_accept& o) :
    paxmsg_t(_descr, ID) {
      if(this != &o) {
        mynid = o.mynid;
        include_me = o.include_me;
        latest_exec = o.latest_exec;
        newvid = o.newvid;
        if(o.newview == nullptr) {
          newview = nullptr;
        } else {
          newview = std::make_unique<view_t>(*o.newview);
        }
      }
    }
  node_id_t mynid;
  bool include_me;
  // Latest executed entry
  viewstamp_t latest_exec;
  std::unique_ptr<view_t> newview;
  // Not in Mazieres, but I need it to make sure I only count responses to THIS vc
  viewid_t newvid;
  void pr(std::ostream& os) const {
    // Cannot figure out why compiler barfed 
    // ((newview==nullptr)? "NULL" : *newview)
    if(newview==nullptr) {
      os << "{" << _descr << "\n    mynid:"
        << mynid << " me?:" << include_me << " latest_exec: " << latest_exec
        << "\n   " "NULL" << "\n    newvid: " << newvid << "}";
    } else {
      os << "{" << _descr << "\n    mynid:"
        << mynid << " me?:" << include_me << " latest_exec: " << latest_exec
        << "\n   newview: " << *newview << "\n    newvid: " << newvid << "}";
    }
  }
  // Less than operator so we can order sets of view_change_accept messages
  // This means two different vca messages from the same node id can't reside
  // in a set, the latter will not be inserted
  bool operator<(const view_change_accept& o) {
    return mynid < o.mynid;
  }
};
// Function object to delegate ordering of unique_ptr of view_change_accept
// to the underlying object (order by mynid, not by address of pointer)
struct vca_less {
  bool operator()(const std::unique_ptr<struct view_change_accept> &a,
      const std::unique_ptr<struct view_change_accept> &b) const {
    return *a < *b;
  }
};


struct new_view_arg : public paxmsg_t {
  static const int ID = 12;
  static constexpr const char* _descr = "new_vieARG";
  new_view_arg() : paxmsg_t( _descr, ID) {}
  // Deleted highest logged viewstamp of any server in old view 
  //  (called latest in Mazieres)
  // Last executed viewstamp of primary in new view (not in Mazieries)
  viewstamp_t latest_exec;
  view_t view;
  void pr(std::ostream& os) const {
    os << "{" << _descr << "\n    latest_exec: " << latest_exec
      << "\n    view: " << view << "}";
  }
};

struct new_view_res : public paxmsg_t {
  static const int ID = 13;
  static constexpr const char* _descr = "new_vieRES";
  explicit new_view_res(bool _accepted) : 
    paxmsg_t( _descr, ID),
    accepted(_accepted)
  {
  }
  bool accepted;
};

struct init_view_arg : public paxmsg_t {
  static const int ID = 14;
  static constexpr const char* _descr = "init_viARG";
  init_view_arg(const view_t& _view) :
    paxmsg_t( _descr, ID),
    view(_view)
  {
  }
  view_t view;
};

// In Mazieres, this is a special argument to replicate_arg, here it is
// its own message type
struct init_view_request : public paxmsg_t {
  static const int ID = 15;
  static constexpr const char* _descr = "init_viREQ";
  init_view_request() : paxmsg_t( _descr, ID) {}
  view_t newview;
  // Wow, finally found a need for this field to detect an out of
  // date log in the view_change_reject to init_view_arg vc path
  // By checking this field, I know if I need to catchup_state
  viewstamp_t prev;
  void pr(std::ostream& os) const {
    os << "{" << _descr << "\n    newview: " << newview
      << "\n    prev: " << prev << "}";
  }
};

// We have chosen one method to catch up cohorts that might not have
// logged all of the operations of the new primary.
struct getstate_arg : public paxmsg_t {
  static const int ID = 16;
  static constexpr const char* _descr = "getsta_ARG";
  getstate_arg() : paxmsg_t( _descr, ID) {}
  void pr(std::ostream& os) const {
    os << "{" << _descr << "}";
  }
};

struct getstate_res : public paxmsg_t {
  static const int ID = 17;
  static constexpr const char* _descr = "getsta_RES";
  getstate_res() : paxmsg_t( _descr, ID) {}
  viewstamp_t vs; // Viewstamp of last_exec_vs for po
  std::unique_ptr<paxobj> po;
  void pr(std::ostream& os) const {
    os << "{" << _descr << " vs: " << vs
      << "\n\tPAXOBJ\n" << po->to_str();
  }
};
// For modules (like net) who want to map between RPC number and string
extern std::unordered_map<int, std::string> paxrpc2str;

