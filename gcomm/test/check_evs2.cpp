/*
 * Copyright (C) 2009 Codership Oy <info@codership.com>
 *
 * $Id$
 */

/*!
 * @file Unit tests for refactored EVS
 */



#include "evs_proto.hpp"
#include "evs_input_map2.hpp"
#include "evs_message2.hpp"
#include "evs_seqno.hpp"

#include "check_gcomm.hpp"
#include "check_templ.hpp"
#include "check_trace.hpp"

#include "gcomm/conf.hpp"

#include <stdexcept>
#include <vector>
#include <set>



#include "check.h"


using namespace std;
using namespace std::rel_ops;
using namespace gu;
using namespace gu::datetime;
using namespace gu::net;
using namespace gcomm;
using namespace gcomm::evs;




START_TEST(test_range)
{
    log_info << "START";
    Range r(3, 6);

    check_serialization(r, 2 * gcomm::serial_size(seqno_t()), Range());

}
END_TEST

START_TEST(test_message)
{
    log_info << "START";
    UUID uuid1(0, 0);
    ViewId view_id(V_TRANS, uuid1, 4567);
    seqno_t seq(478), aru_seq(456), seq_range(7);
    
    UserMessage um(uuid1, view_id, seq, aru_seq, seq_range, O_SAFE, 75433, 0xab,
                   Message::F_SOURCE);
    fail_unless(um.serial_size() % 4 == 0);
    check_serialization(um, um.serial_size(), UserMessage());
    
    DelegateMessage dm(uuid1, view_id);
    dm.set_source(uuid1);
    check_serialization(dm, dm.serial_size(), DelegateMessage());
    
    MessageNodeList node_list;
    node_list.insert(make_pair(uuid1, MessageNode()));
    node_list.insert(make_pair(UUID(2), MessageNode(true, false, 1, 
                                                    ViewId(V_REG), 5,
                                                    Range(7, 8))));
    JoinMessage jm(uuid1, view_id, 8, 5, 27, node_list);
    jm.set_source(uuid1);
    check_serialization(jm, jm.serial_size(), JoinMessage());
    
    InstallMessage im(uuid1, view_id, ViewId(V_REG, view_id.get_uuid(),
                                             view_id.get_seq()), 8, 5, 27, node_list);
    im.set_source(uuid1);
    check_serialization(im, im.serial_size(), InstallMessage());
    
    LeaveMessage lm(uuid1, view_id, 45, 88, 3456);
    lm.set_source(uuid1);
    check_serialization(lm, lm.serial_size(), LeaveMessage());
}
END_TEST

START_TEST(test_input_map_insert)
{
    log_info << "START";
    UUID uuid1(1), uuid2(2);
    InputMap im;
    ViewId view(V_REG, uuid1, 0);

    try 
    {
        im.insert(0, UserMessage(uuid1, view, 0));
        fail("");
    } 
    catch (...) 
    {  }

    im.reset(1);
    
    im.insert(0, UserMessage(uuid1, view, 0));
    

    im.clear();
    im.reset(2);

    for (seqno_t s = 0; s < 10; ++s)
    {
        im.insert(0, UserMessage(uuid1, view, s));
        im.insert(1, UserMessage(uuid2, view, s));
    }

    for (seqno_t s = 0; s < 10; ++s)
    {
        InputMap::iterator i = im.find(0, s);
        fail_if(i == im.end());
        fail_unless(InputMapMsgIndex::get_value(i).get_msg().get_source() == uuid1);
        fail_unless(InputMapMsgIndex::get_value(i).get_msg().get_seq() == s);

        i = im.find(1, s);
        fail_if(i == im.end());
        fail_unless(InputMapMsgIndex::get_value(i).get_msg().get_source() == uuid2);
        fail_unless(InputMapMsgIndex::get_value(i).get_msg().get_seq() == s);
    }
    
}
END_TEST

START_TEST(test_input_map_find)
{
    log_info << "START";
    InputMap im;
    UUID uuid1(1);
    ViewId view(V_REG, uuid1, 0);
    
    im.reset(1);
    
    im.insert(0, UserMessage(uuid1, view, 0));
    
    fail_if(im.find(0, 0) == im.end());
    

    im.insert(0, UserMessage(uuid1, view, 2));
    im.insert(0, UserMessage(uuid1, view, 4));
    im.insert(0, UserMessage(uuid1, view, 7));

    fail_if(im.find(0, 2) == im.end());
    fail_if(im.find(0, 4) == im.end());
    fail_if(im.find(0, 7) == im.end());

    fail_unless(im.find(0, 3) == im.end());
    fail_unless(im.find(0, 5) == im.end());
    fail_unless(im.find(0, 6) == im.end());
    fail_unless(im.find(0, 8) == im.end());
}
END_TEST

START_TEST(test_input_map_safety)
{
    log_info << "START";
    InputMap im;
    UUID uuid1(1);
    size_t index1(0);
    ViewId view(V_REG, uuid1, 0);
    
    im.reset(1);
    
    im.insert(index1, UserMessage(uuid1, view, 0));
    fail_unless(im.get_aru_seq() == 0);
    im.insert(index1, UserMessage(uuid1, view, 1));
    fail_unless(im.get_aru_seq() == 1);
    im.insert(index1, UserMessage(uuid1, view, 2));
    fail_unless(im.get_aru_seq() == 2);
    im.insert(index1, UserMessage(uuid1, view, 3));
    fail_unless(im.get_aru_seq() == 3);
    im.insert(index1, UserMessage(uuid1, view, 5));
    fail_unless(im.get_aru_seq() == 3);    
    
    im.insert(index1, UserMessage(uuid1, view, 4));
    fail_unless(im.get_aru_seq() == 5);
    
    InputMap::iterator i = im.find(index1, 0);
    fail_unless(im.is_fifo(i) == true);
    fail_unless(im.is_agreed(i) == true);
    fail_if(im.is_safe(i) == true);
    im.set_safe_seq(index1, 0);
    fail_unless(im.is_safe(i) == true);
    
    im.set_safe_seq(index1, 5);
    i = im.find(index1, 5);
    fail_unless(im.is_safe(i) == true);
    
    im.insert(index1, UserMessage(uuid1, view, 7));
    im.set_safe_seq(index1, im.get_aru_seq());
    i = im.find(index1, 7);
    fail_if(im.is_safe(i) == true);

}
END_TEST

START_TEST(test_input_map_erase)
{
    log_info << "START";
    InputMap im;
    size_t index1(0);
    UUID uuid1(1);
    ViewId view(V_REG, uuid1, 1);

    im.reset(1);

    for (seqno_t s = 0; s < 10; ++s)
    {
        im.insert(index1, UserMessage(uuid1, view, s));
    }
    
    for (seqno_t s = 0; s < 10; ++s)
    {
        InputMap::iterator i = im.find(index1, s);
        fail_unless(i != im.end());
        im.erase(i);
        i = im.find(index1, s);
        fail_unless(i == im.end());
        (void)im.recover(index1, s);
    }
    im.set_safe_seq(index1, 9);
    try
    {
        im.recover(index1, 9);
        fail("");
    }
    catch (...) { }
}
END_TEST

START_TEST(test_input_map_overwrap)
{
    log_info << "START";
    InputMap im;
    const size_t n_nodes(5);
    ViewId view(V_REG, UUID(1), 1);
    vector<UUID> uuids;
    for (size_t n = 0; n < n_nodes; ++n)
    {
        uuids.push_back(UUID(static_cast<int32_t>(n + 1)));
    }
    
    im.reset(n_nodes);
    
    
    Date start(Date::now());
    size_t cnt(0);
    seqno_t last_safe(-1);
    for (seqno_t seq = 0; seq < 100000; ++seq)
    {
        for (size_t i = 0; i < n_nodes; ++i)
        {
            UserMessage um(uuids[i], view, seq);
            (void)im.insert(i, um);
            if ((seq + 5) % 10 == 0)
            {
                last_safe = um.get_seq() - 3;
                im.set_safe_seq(i, last_safe);
                for (InputMap::iterator ii = im.begin(); 
                     ii != im.end() && im.is_safe(ii) == true;
                     ii = im.begin())
                {
                    im.erase(ii);
                }
            }
            cnt++;
        }
        gcomm_assert(im.get_aru_seq() == seq);
        gcomm_assert(im.get_safe_seq() == last_safe);
    }
    Date stop(Date::now());
    
    double div(double(stop.get_utc() - start.get_utc())/gu::datetime::Sec);
    log_info << "input map msg rate " << double(cnt)/div;
}
END_TEST


class InputMapInserter
{
public:
    InputMapInserter(InputMap& im_) : im(im_) { }
    
    void operator()(const pair<size_t, UserMessage>& p) const
    {
        im.insert(p.first, p.second);
    }
private:
    InputMap& im;
};

START_TEST(test_input_map_random_insert)
{
    log_info << "START";
    seqno_t window(1024);
    seqno_t n_seqnos(1024);
    size_t n_uuids(4);
    vector<UUID> uuids(n_uuids);
    vector<pair<size_t, UserMessage> > msgs(static_cast<size_t>(n_uuids*n_seqnos));
    ViewId view_id(V_REG, UUID(1), 1);
    InputMap im;
    
    for (size_t i = 0; i < n_uuids; ++i)
    {
        uuids[i] = (static_cast<int32_t>(i + 1));
    }
    
    im.reset(n_uuids, window);
    
    for (seqno_t j = 0; j < n_seqnos; ++j)
    {
        for (size_t i = 0; i < n_uuids; ++i)
        {
            msgs[static_cast<size_t>(j*n_uuids) + i] =
                make_pair(i, UserMessage(uuids[i], view_id, j));
        }
    }
    
    vector<pair<size_t, UserMessage> > random_msgs(msgs);
    random_shuffle(random_msgs.begin(), random_msgs.end());
    for_each(random_msgs.begin(), random_msgs.end(), InputMapInserter(im));
    
    size_t n = 0;
    for (InputMap::iterator i = im.begin(); i != im.end(); ++i)
    {
        const InputMapMsg& msg(InputMapMsgIndex::get_value(i));
        fail_unless(msg.get_msg() == msgs[n].second);
        fail_if(im.is_safe(i) == true);
        ++n;
    }
    
    fail_unless(im.get_aru_seq() == n_seqnos - 1);
    fail_unless(im.get_safe_seq() == -1);
    
    for (size_t i = 0; i < n_uuids; ++i)
    {
        fail_unless(im.get_range(i) == 
                    Range(n_seqnos,
                          n_seqnos - 1));
        
        im.set_safe_seq(i, n_seqnos - 1);
    }
    fail_unless(im.get_safe_seq() == n_seqnos - 1);
    
}
END_TEST




static Datagram* get_msg(DummyTransport* tp, Message* msg, bool release = true)
{
    Datagram* rb = tp->get_out();
    if (rb != 0)
    {
        gu_trace(Proto::unserialize_message(tp->get_uuid(), *rb, msg));
        if (release == true)
        {
            delete rb;
        }
    }
    return rb;
}

static void single_join(DummyTransport* t, Proto* p)
{
    Message jm, im, gm;
    
    // Initial state is joining
    p->shift_to(Proto::S_JOINING);
    
    // Send join must produce emitted join message
    p->send_join();
    
    Datagram* rb = get_msg(t, &jm);
    fail_unless(rb != 0);
    fail_unless(jm.get_type() == Message::T_JOIN);
    
    // Install message is emitted at the end of JOIN handling
    // 'cause this is the only instance and is always consistent
    // with itself
    rb = get_msg(t, &im);
    fail_unless(rb != 0);
    fail_unless(im.get_type() == Message::T_INSTALL);
    
    // Handling INSTALL message must emit two gap messages,
    // one for receiving install message and one for 
    // shift to operational
    rb = get_msg(t, &gm);
    fail_unless(rb != 0);
    fail_unless(gm.get_type() == Message::T_GAP);
    
    rb = get_msg(t, &gm);
    fail_unless(rb != 0);
    fail_unless(gm.get_type() == Message::T_GAP);
    
    // State must have evolved JOIN -> S_RECOVERY -> S_OPERATIONAL
    fail_unless(p->get_state() == Proto::S_OPERATIONAL);
    
    // Handle join message again, must stay in S_OPERATIONAL, must not
    // emit anything
    p->handle_msg(jm);
    rb = get_msg(t, &gm);
    fail_unless(rb == 0);
    fail_unless(p->get_state() == Proto::S_OPERATIONAL);
    
}

class DummyUser : public Toplay
{
public:
    void handle_up(int, const Datagram&, const ProtoUpMeta&)
    {

    }
};

START_TEST(test_proto_single_join)
{
    log_info << "START";
    Protonet net;
    UUID uuid(1);
    DummyTransport t(net, uuid);
    DummyUser u;
    Proto p(uuid);
    gcomm::connect(&t, &p);
    gcomm::connect(&p, &u);
    single_join(&t, &p);
}
END_TEST

static void double_join(DummyTransport* t1, Proto* p1,
                        DummyTransport* t2, Proto* p2)
{

    Message jm;
    Message im;
    Message gm;
    Message gm2;
    Message msg;

    Datagram* rb;

    // Initial states check
    p2->shift_to(Proto::S_JOINING);
    fail_unless(p1->get_state() == Proto::S_OPERATIONAL);
    fail_unless(p2->get_state() == Proto::S_JOINING);

    // Send join message, don't self handle immediately
    // Expected output: one join message
    p2->send_join(false);
    fail_unless(p2->get_state() == Proto::S_JOINING);
    rb = get_msg(t2, &jm);
    fail_unless(rb != 0);
    fail_unless(jm.get_type() == Message::T_JOIN);
    rb = get_msg(t2, &msg);
    fail_unless(rb == 0);
    
    // Handle node 2's join on node 1
    // Expected output: shift to S_RECOVERY and one join message
    p1->handle_msg(jm);
    fail_unless(p1->get_state() == Proto::S_RECOVERY);
    rb = get_msg(t1, &jm);
    fail_unless(rb != 0);
    fail_unless(jm.get_type() == Message::T_JOIN);
    rb = get_msg(t1, &msg);
    fail_unless(rb == 0);
    
    // Handle node 1's join on node 2
    // Expected output: shift to S_RECOVERY and one join message
    p2->handle_msg(jm);
    fail_unless(p2->get_state() == Proto::S_RECOVERY);
    rb = get_msg(t2, &jm);
    fail_unless(rb != 0);
    fail_unless(jm.get_type() == Message::T_JOIN);
    rb = get_msg(t2, &msg);
    fail_unless(rb == 0);
    
    // Handle node 2's join on node 1
    // Expected output: Install and gap messages, state stays in S_RECOVERY
    p1->handle_msg(jm);
    fail_unless(p1->get_state() == Proto::S_RECOVERY);
    rb = get_msg(t1, &im);
    fail_unless(rb != 0);
    fail_unless(im.get_type() == Message::T_INSTALL);
    rb = get_msg(t1, &gm);
    fail_unless(rb != 0);
    fail_unless(gm.get_type() == Message::T_GAP);
    rb = get_msg(t1, &msg);
    fail_unless(rb == 0);
    
    // Handle install message on node 2
    // Expected output: Gap message and state stays in S_RECOVERY
    p2->handle_msg(im);
    fail_unless(p2->get_state() == Proto::S_RECOVERY);
    rb = get_msg(t2, &gm2);
    fail_unless(rb != 0);
    fail_unless(gm2.get_type() == Message::T_GAP);
    rb = get_msg(t2, &msg);
    fail_unless(rb == 0);
    
    // Handle gap messages
    // Expected output: Both nodes shift to S_OPERATIONAL,
    // both send gap messages
    p1->handle_msg(gm2);
    fail_unless(p1->get_state() == Proto::S_OPERATIONAL);
    rb = get_msg(t1, &msg);
    fail_unless(rb != 0);
    fail_unless(msg.get_type() == Message::T_GAP);
    rb = get_msg(t1, &msg);
    fail_unless(rb == 0);
    p2->handle_msg(gm);
    fail_unless(p2->get_state() == Proto::S_OPERATIONAL);
    rb = get_msg(t2, &msg);
    fail_unless(rb != 0);
    fail_unless(msg.get_type() == Message::T_GAP);
    rb = get_msg(t2, &msg);
    fail_unless(rb == 0);
}


START_TEST(test_proto_double_join)
{
    log_info << "START";
    Protonet net;
    UUID uuid1(1), uuid2(2);
    DummyTransport t1(net, uuid1), t2(net, uuid2);
    DummyUser u1, u2;
    Proto p1(uuid1), p2(uuid2);

    gcomm::connect(&t1, &p1);
    gcomm::connect(&p1, &u1);

    gcomm::connect(&t2, &p2);
    gcomm::connect(&p2, &u2);

    single_join(&t1, &p1);
    double_join(&t1, &p1, &t2, &p2);

}
END_TEST


static DummyNode* create_dummy_node(size_t idx, 
                                    const string& inactive_timeout = "PT1H",
                                    const string& retrans_period = "PT20M")
{
    string conf = "evs://?" + Conf::EvsViewForgetTimeout + "=PT1H&"
        + Conf::EvsInactiveCheckPeriod + "=" + to_string(Period(inactive_timeout)/3) + "&"
        + Conf::EvsSuspectTimeout + "=" + inactive_timeout + "&"
        + Conf::EvsInactiveTimeout + "=" + inactive_timeout + "&"
        + Conf::EvsConsensusTimeout + "=" + to_string(Period(inactive_timeout)*2) + "&"
        + Conf::EvsKeepalivePeriod + "=" + retrans_period + "&"
        + Conf::EvsJoinRetransPeriod + "=" + retrans_period + "&"
        + Conf::EvsInfoLogMask + "=0x3";
    if (::getenv("EVS_DEBUG_MASK") != 0)
    {
        conf += "&" + Conf::EvsDebugLogMask + "=" 
            + ::getenv("EVS_DEBUG_MASK");
    }
    list<Protolay*> protos;
    Protonet net;
    try
    {
        UUID uuid(static_cast<int32_t>(idx));
        protos.push_back(new DummyTransport(net, uuid, false));
        protos.push_back(new Proto(uuid, conf));
        return new DummyNode(idx, protos);
    }
    catch (...)
    {
        for_each(protos.begin(), protos.end(), DeleteObject());
        throw;
    }
}

static void join_node(PropagationMatrix* p, 
                      DummyNode* n, bool first = false)
{
    gu_trace(p->insert_tp(n));
    gu_trace(n->connect(first));
}


static void send_n(DummyNode* node, const size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        gu_trace(node->send());
    }
}

static void set_cvi(vector<DummyNode*>& nvec, size_t i_begin, size_t i_end,
                    size_t seq)
{
    for (size_t i = i_begin; i <= i_end; ++i)
    {
        nvec[i]->set_cvi(ViewId(V_REG, nvec[i_begin]->get_uuid(), 
                                static_cast<uint32_t>(seq)));
    }
}

START_TEST(test_proto_join_n)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;

    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


START_TEST(test_proto_join_n_w_user_msg)
{
    gu_conf_self_tstamp_on();
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    // @todo This test should terminate without these timeouts
    const string inactive_timeout("PT1H");
    const string retrans_period("PT0.1S");

    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(true));
        for (size_t j = 0; j <= i; ++j)
        {
            gu_trace(send_n(dn[j], 5 + ::rand() % 4));
        }
        gu_trace(prop.propagate_until_empty());
        for (size_t j = 0; j <= i; ++j)
        {
            gu_trace(send_n(dn[j], 5 + ::rand() % 4));
        }
    }
    
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


START_TEST(test_proto_join_n_lossy)
{
    gu_conf_self_tstamp_on();    
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT1H");
    const string retrans_period("PT0.1S");


    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        for (size_t j = 1; j < i + 1; ++j)
        {
            prop.set_loss(i + 1, j, 0.9);
            prop.set_loss(j, i + 1, 0.9);
        }
        gu_trace(prop.propagate_until_cvi(true));
    }
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


START_TEST(test_proto_join_n_lossy_w_user_msg)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT1H");
    const string retrans_period("PT0.1S");    

    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }

    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        for (size_t j = 1; j < i + 1; ++j)
        {
            prop.set_loss(i + 1, j, 0.9);
            prop.set_loss(j, i + 1, 0.9);
            
        }
        gu_trace(prop.propagate_until_cvi(true));
        for (size_t j = 0; j < i; ++j)
        {
            gu_trace(send_n(dn[j], 5 + ::rand() % 4));
        }
    }
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST

START_TEST(test_proto_leave_n)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;

    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(true));
    }
    
    uint32_t last_view_seq = dn[0]->get_trace().get_current_view_trace().get_view().get_id().get_seq();
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        dn[i]->close();
        dn[i]->set_cvi(V_REG);
        set_cvi(dn, i + 1, n_nodes - 1, last_view_seq + i + 1);
        gu_trace(prop.propagate_until_cvi(true));
    }
    
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST

START_TEST(test_proto_leave_n_w_user_msg)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT1H");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));        
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    
    uint32_t last_view_seq = dn[0]->get_trace().get_current_view_trace().get_view().get_id().get_seq();
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        for (size_t j = i; j < n_nodes; ++j)
        {
            gu_trace(send_n(dn[j], 5 + ::rand() % 4));
        }
        dn[i]->close();
        dn[i]->set_cvi(V_REG);
        set_cvi(dn, i + 1, n_nodes - 1, last_view_seq + i + 1);
        gu_trace(prop.propagate_until_cvi(true));
    }
    
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


START_TEST(test_proto_leave_n_lossy)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT1S");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    
    uint32_t last_view_seq = dn[0]->get_trace().get_current_view_trace().get_view().get_id().get_seq();
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        for (size_t j = 1; j < i + 1; ++j)
        {
            prop.set_loss(i + 1, j, 0.9);
            prop.set_loss(j, i + 1, 0.9);
        }
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        dn[i]->set_cvi(V_REG);
        set_cvi(dn, i + 1, n_nodes - 1, last_view_seq + i + 1);
        dn[i]->close();
        gu_trace(prop.propagate_until_cvi(true));
    }
    
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST



START_TEST(test_proto_leave_n_lossy_w_user_msg)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT1S");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    
    uint32_t last_view_seq = dn[0]->get_trace().get_current_view_trace().get_view().get_id().get_seq();
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        for (size_t j = 1; j < i + 1; ++j)
        {
            prop.set_loss(i + 1, j, 0.9);
            prop.set_loss(j, i + 1, 0.9);
        }
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        for (size_t j = i; j < n_nodes; ++j)
        {
            gu_trace(send_n(dn[j], 5 + ::rand() % 4));
        }
        dn[i]->set_cvi(V_REG);
        set_cvi(dn, i + 1, n_nodes - 1, last_view_seq + i + 1);
        dn[i]->close();
        gu_trace(prop.propagate_until_cvi(true));
    }
    
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


// Generic test code for split/merge cases
static void test_proto_split_merge_gen(const size_t n_nodes,
                                       const bool send_msgs,
                                       const double loss)
{
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT1.2S");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        for (size_t j = 1; j < i + 1; ++j)
        {
            prop.set_loss(i + 1, j, loss);
            prop.set_loss(j, i + 1, loss);
        }
    }    
    
    vector<int32_t> split;
    for (size_t i = 0; i < n_nodes; ++i)
    {
        split.push_back(static_cast<int32_t>(i + 1));
    }
    
    uint32_t view_seq = dn[0]->get_trace().get_current_view_trace().get_view().get_id().get_seq();
    uint32_t view_seq_inc = 0;
    
    for (size_t i = 1; i < n_nodes; ++i)
    {
        if (send_msgs == true)
        {
            for (size_t k = 0; k < 5; ++k)
            {
                for (size_t j = 0; j < n_nodes; ++j)
                {
                    gu_trace(send_n(dn[j], 1 + j));        
                }
                gu_trace(prop.propagate_n(7));
            }
        }
        
        log_info << "split " << i;
        for (size_t j = 0; j < i; ++j)
        {
            for (size_t k = i; k < n_nodes; ++k)
            {
                gu_trace(prop.set_loss(split[j], split[k], 0.));
                gu_trace(prop.set_loss(split[k], split[j], 0.));
            }
        }

        ++view_seq_inc;
        set_cvi(dn, 0, i - 1, view_seq + view_seq_inc);
        set_cvi(dn, i, n_nodes - 1, view_seq + view_seq_inc);

        if (send_msgs == true)
        {
            for (size_t j = 0; j < n_nodes; ++j)
            {
                gu_trace(send_n(dn[j], 5 + rand() % 4));        
            }
        }
        
        gu_trace(prop.propagate_until_cvi(true));
        
        log_info << "merge " << i;
        for (size_t j = 0; j < i; ++j)
        {
            for (size_t k = i; k < n_nodes; ++k)
            {
                gu_trace(prop.set_loss(split[j], split[k], loss));
                gu_trace(prop.set_loss(split[k], split[j], loss));
            }
        }

        ++view_seq_inc;
        set_cvi(dn, 0, n_nodes - 1, view_seq + view_seq_inc);
        
        if (send_msgs == true)
        {
            for (size_t j = 0; j < n_nodes; ++j)
            {
                gu_trace(send_n(dn[j], 5 + rand() % 4));
            }
        }
        gu_trace(prop.propagate_until_cvi(true));
    }

    gu_trace(prop.propagate_until_empty());
    
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}



START_TEST(test_proto_split_merge)
{
    log_info << "START";
    test_proto_split_merge_gen(4, false, 1.);
}
END_TEST


START_TEST(test_proto_split_merge_lossy)
{
    log_info << "START";
    test_proto_split_merge_gen(4, false, .9);
}
END_TEST



START_TEST(test_proto_split_merge_w_user_msg)
{
    log_info << "START";
    for (size_t i = 0; i < 10; ++i)
    {
        test_proto_split_merge_gen(4, true, 1.);
    }
}
END_TEST


START_TEST(test_proto_split_merge_lossy_w_user_msg)
{
    log_info << "START";
    test_proto_split_merge_gen(4, true, .9);
}
END_TEST

START_TEST(test_proto_stop_cont)
{
    log_info << "START";
    const size_t n_nodes(4);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT0.31S");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    uint32_t view_seq = n_nodes + 1;
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        for (size_t j = 0; j < n_nodes; ++j)
        {
            if (j != i)
            {
                dn[j]->close(dn[i]->get_uuid());
            }
        }
        set_cvi(dn, 0, n_nodes - 1, view_seq + 1);
        gu_trace(prop.propagate_until_cvi(true));
        view_seq += 2;

    }
    gu_trace(check_trace(dn));
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


START_TEST(test_proto_arbitrate)
{
    log_info << "START";
    const size_t n_nodes(3);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT0.5S");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    uint32_t view_seq = n_nodes + 1;
    
    dn[0]->close(dn[1]->get_uuid());
    dn[1]->close(dn[0]->get_uuid());
    dn[0]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq));
    dn[2]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq));
    dn[1]->set_cvi(ViewId(V_REG, dn[1]->get_uuid(), view_seq));
    gu_trace(prop.propagate_until_cvi(true));
    
    dn[0]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq + 1));
    dn[1]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq + 1));
    dn[2]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq + 1));
    gu_trace(prop.propagate_until_cvi(true));

    gu_trace(check_trace(dn));

    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST


START_TEST(test_proto_split_two)
{
    log_info << "START";
    const size_t n_nodes(2);
    PropagationMatrix prop;
    vector<DummyNode*> dn;
    const string inactive_timeout("PT0.31S");
    const string retrans_period("PT0.1S");
    
    for (size_t i = 1; i <= n_nodes; ++i)
    {
        gu_trace(dn.push_back(create_dummy_node(i, inactive_timeout, retrans_period)));
    }
    
    for (size_t i = 0; i < n_nodes; ++i)
    {
        gu_trace(join_node(&prop, dn[i], i == 0 ? true : false));
        set_cvi(dn, 0, i, i + 1);
        gu_trace(prop.propagate_until_cvi(false));
    }
    uint32_t view_seq = n_nodes + 1;
    
    dn[0]->close(dn[1]->get_uuid());
    dn[1]->close(dn[0]->get_uuid());
    dn[0]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq));
    dn[1]->set_cvi(ViewId(V_REG, dn[1]->get_uuid(), view_seq));
    
    gu_trace(prop.propagate_until_cvi(true));
    
    dn[0]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq + 1));
    dn[1]->set_cvi(ViewId(V_REG, dn[0]->get_uuid(), view_seq + 1));
    gu_trace(prop.propagate_until_cvi(true));
    
    gu_trace(check_trace(dn));
    
    for_each(dn.begin(), dn.end(), DeleteObject());
}
END_TEST



static bool skip(false);

Suite* evs2_suite()
{
    Suite* s = suite_create("gcomm::evs");
    TCase* tc;


    if (skip == false)
    {
        tc = tcase_create("test_range");
        tcase_add_test(tc, test_range);
        suite_add_tcase(s, tc);
    
        tc = tcase_create("test_message");
        tcase_add_test(tc, test_message);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_input_map_insert");
        tcase_add_test(tc, test_input_map_insert);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_input_map_find");
        tcase_add_test(tc, test_input_map_find);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_input_map_safety");
        tcase_add_test(tc, test_input_map_safety);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_input_map_erase");
        tcase_add_test(tc, test_input_map_erase);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_input_map_overwrap");
        tcase_add_test(tc, test_input_map_overwrap);
        tcase_set_timeout(tc, 15);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_input_map_random_insert");
        tcase_add_test(tc, test_input_map_random_insert);
        suite_add_tcase(s, tc);
        
        tc = tcase_create("test_proto_single_join");
        tcase_add_test(tc, test_proto_single_join);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_proto_double_join");
        tcase_add_test(tc, test_proto_double_join);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_proto_join_n");
        tcase_add_test(tc, test_proto_join_n);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_proto_join_n_w_user_msg");
        tcase_add_test(tc, test_proto_join_n_w_user_msg);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_proto_join_n_lossy");
        tcase_add_test(tc, test_proto_join_n_lossy);
        suite_add_tcase(s, tc);
        
        tc = tcase_create("test_proto_join_n_lossy_w_user_msg");
        tcase_add_test(tc, test_proto_join_n_lossy_w_user_msg);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_proto_leave_n");
        tcase_add_test(tc, test_proto_leave_n);
        tcase_set_timeout(tc, 20);
        suite_add_tcase(s, tc);
    
        tc = tcase_create("test_proto_leave_n_w_user_msg");
        tcase_add_test(tc, test_proto_leave_n_w_user_msg);
        tcase_set_timeout(tc, 20);
        suite_add_tcase(s, tc);
    
        tc = tcase_create("test_proto_leave_n_lossy");
        tcase_add_test(tc, test_proto_leave_n_lossy);
        tcase_set_timeout(tc, 20);
        suite_add_tcase(s, tc);

        tc = tcase_create("test_proto_leave_n_lossy_w_user_msg");
        tcase_add_test(tc, test_proto_leave_n_lossy_w_user_msg);
        tcase_set_timeout(tc, 20);
        suite_add_tcase(s, tc);
    }
    
    tc = tcase_create("test_proto_split_merge");
    tcase_add_test(tc, test_proto_split_merge);
    tcase_set_timeout(tc, 15);
    suite_add_tcase(s, tc);
    
    tc = tcase_create("test_proto_split_merge_lossy");
    tcase_add_test(tc, test_proto_split_merge_lossy);
    tcase_set_timeout(tc, 15);
    suite_add_tcase(s, tc);

    tc = tcase_create("test_proto_split_merge_w_user_msg");
    tcase_add_test(tc, test_proto_split_merge_w_user_msg);
    tcase_set_timeout(tc, 150);
    suite_add_tcase(s, tc);

    tc = tcase_create("test_proto_split_merge_lossy_w_user_msg");
    tcase_add_test(tc, test_proto_split_merge_lossy_w_user_msg);
    tcase_set_timeout(tc, 15);
    suite_add_tcase(s, tc);
    
    tc = tcase_create("test_proto_stop_cont");
    tcase_add_test(tc, test_proto_stop_cont);
    suite_add_tcase(s, tc);
    
    tc = tcase_create("test_proto_arbitrate");
    tcase_add_test(tc, test_proto_arbitrate);
    suite_add_tcase(s, tc);
    
    tc = tcase_create("test_proto_split_two");
    tcase_add_test(tc, test_proto_split_two);
    suite_add_tcase(s, tc);
        

    return s;
}