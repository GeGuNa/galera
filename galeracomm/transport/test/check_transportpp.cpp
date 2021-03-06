

#include "gcomm/transport.hpp"
#include "gcomm/exception.hpp"
#include "gcomm/thread.hpp"

#include <check.h>
#include <cstdlib>
#include <list>
#include <iostream>
#include <map>

const char* async_addr = "asynctcp:127.0.0.1:23456";
const char* sync_addr = "tcp:127.0.0.1:4567";

class Sender : Toplay {
    Transport *tp;
    Poll *poll;
    bool can_send;
public:
    Sender(Poll *p) : tp(0), poll(p), can_send(false) {}
    void handle_up(const int cid, const ReadBuf *rb, const size_t offset, 
		   const ProtoUpMeta *um) {
	if (rb == 0 && tp->get_state() == TRANSPORT_S_CONNECTED)
	    can_send = true;
	else
	    throw DException("");
    }

    bool get_connected() const {
	return can_send;
    }
    void connect() {
	tp = Transport::create(async_addr, poll, this);
	set_down_context(tp);
	tp->connect(async_addr);
	tp->set_max_pending_bytes(1 << 31);
    }
    bool send(const size_t b) {
	unsigned char *buf = new unsigned char[b];
	if (can_send == false)
	    throw DException("");
	
	for (size_t i = 0; i < b; i++)
	    buf[i] = i % 256;
	WriteBuf *wb = new WriteBuf(buf, b);
	int res;
	bool ret = (res = pass_down(wb, 0)) == 0 ? true : false;
	delete wb;
	delete[] buf;
	return ret;
    }
    void send_sync(const size_t b) {
	unsigned char *buf = new unsigned char[b];
	if (can_send == false)
	    throw DException("");
	for (size_t i = 0; i < b; i++)
	    buf[i] = i % 256;
	WriteBuf *wb = new WriteBuf(buf, b);
	tp->send(wb, 0);
	delete wb;
	delete[] buf;
    }
    void close() {
	tp->close();
	delete tp;
	tp = 0;
	can_send = false;
    }
};




class Receiver : public Toplay {
    clock_t cstart;
    clock_t cstop;
    uint64_t recvd;
    Transport *tp;
public:
    Receiver(Transport *t) : recvd(0), tp(t) {
	cstart = clock();
	tp->set_up_context(this);
    }
    ~Receiver() {
	cstop = clock();
	clock_t ct = (cstop - cstart);
	double tput = CLOCKS_PER_SEC*double(recvd)/(cstop - cstart);
	std::cerr << "Reciver: received " << recvd << " bytes\n";
	std::cerr << "         used " << (double(ct)/CLOCKS_PER_SEC) << " secs cputime\n";
	std::cerr << "         throughput " << tput << "bytes/cpusec\n";
    }
    void handle_up(const int cid, const ReadBuf *rb, const size_t offset,
		   const ProtoUpMeta *um) {
	const unsigned char *ptr;
	if (rb == 0) {
	    if (tp->get_state() == TRANSPORT_S_FAILED)
		throw DException(strerror(tp->get_errno()));
	    return;
	}
	for (size_t i = rb->get_len(); i < rb->get_len() - offset;
	     i++) {
	    ptr = reinterpret_cast<const unsigned char *>(rb->get_buf(i));
	    if (*ptr != i % 256)
		throw DException("");
	}
	
	recvd += rb->get_len() - offset;
    }
    void recv() {
	const ReadBuf *rb = tp->recv();
	if (rb == 0)
	    throw DException("");
	const unsigned char *ptr;
	for (size_t i = rb->get_len(); i < rb->get_len(); i++) {
	    ptr = reinterpret_cast<const unsigned char *>(rb->get_buf(i));
	    if (*ptr != i % 256)
		throw DException("");
	}
    }
};

void release(std::pair<Receiver *, Transport *>& p)
{
    delete p.first;
    delete p.second;
}

class Listener : public Toplay {
    Transport *tp;
    Poll *poll;
    std::list<std::pair<Receiver *, Transport *> > tports;
public:
    Receiver *get_first_receiver() {
	return tports.front().first;
    }
    Listener(Poll *_poll) : tp(0), poll(_poll) {}
    ~Listener() {
	delete tp;
	tp = 0;
	for_each(tports.begin(), tports.end(), release);
	tports.clear();
    }
    
    void handle_up(const int cid, const ReadBuf *rb, const size_t offset, 
		   const ProtoUpMeta *um) {
	

	Transport *t = tp->accept(poll);
	Receiver *r = new Receiver(t);
	tports.push_back(std::pair<Receiver *, Transport *>(r, t));
    }

    int handle_down(WriteBuf *wb, const ProtoDownMeta *dm) {
	throw DException("");
	return 0;
    }
    
    void start() {
	if (tp)
	    throw DException("");
	tp = Transport::create(async_addr, poll, this);
	tp->listen(async_addr);
    }
    
    void stop() {
	delete tp;
	tp = 0;
	for_each(tports.begin(), tports.end(), release);
	tports.clear();
    }
};

START_TEST(check_async_transport)
{
    Poll *p = Poll::create("Def");
    Listener l(p);
    Sender s(p);
    l.start();
    s.connect();
    p->poll(1);

    while (s.get_connected() == false) {
	p->poll(1);
    }
    
    for (size_t i = 1; i <= (1 << 24);) {
	if (s.send(i) == true)
	    i *= 2;
	p->poll(1);
    }
        
    while (p->poll(1) > 0) {}

    std::cerr << "Terminating\n";

    s.close();
    l.stop();
    delete p;
}
END_TEST

START_TEST(check_async_multitransport)
{
    Poll *p = Poll::create("Def");
    Listener l(p);
    Sender s1(p), s2(p), s3(p);
    l.start();
    s1.connect();
    s2.connect();
    s3.connect();
    p->poll(1);
    
    while (s1.get_connected() == false || 
	   s2.get_connected() == false ||
	   s3.get_connected() == false) {
	p->poll(1);
    }
    
    for (int i = 0; i < 10000; i++) {
	s1.send(rand()%10000);
	s2.send(rand()%10000);
	s3.send(rand()%10000);
	p->poll(1);
    }
    
    while (p->poll(1) > 0) {}
    
    std::cerr << "Terminating\n";
    
    s1.close();
    s2.close();
    s3.close();
    l.stop();
    delete p;
}
END_TEST




class SyncSender : public Thread {
    const char* addr;
    Transport* tp;
    size_t sent;
    unsigned char* buf;
public:
    
    SyncSender(const char* a) : addr(a), tp(0), sent(0), buf(0) {}
    
    ~SyncSender() {
	delete[] buf;
	std::cerr << "SyncSender: sent " << sent << " bytes\n";
    }
    
    void send(size_t len) {
	buf = new unsigned char[len];
	for (size_t i = 0; i < len; i++)
	    buf[i] = i % 256;
	WriteBuf wb(buf, len);
	tp->send(&wb, 0);
	delete[] buf;
	buf = 0;
	sent += len;
	// std::cerr << "sent " << len;
    }
    
    void run() {
	tp->connect(addr);
	for (int i = 0; i < 40000; i++) {
	    send(::rand()%1024);
	}
	tp->close();
    }
    
    void start() {
	tp = Transport::create(addr, 0);
	Thread::start();
    }

    void stop() {
	Thread::stop();
	delete tp;
    }

};

class SyncReceiver : public Thread {
    Transport* tp;
    size_t recvd;
public:
    SyncReceiver(Transport* t) : tp(t), recvd(0) {}
    ~SyncReceiver() {
	std::cerr << "SyncReceiver: received " << recvd << " bytes\n";
    }

    void run() {
	const ReadBuf* rb;
	while ((rb = tp->recv()) != 0) {
	    size_t len = rb->get_len();
	    const unsigned char *buf = reinterpret_cast<const unsigned char *>(rb->get_buf());
	    for (size_t i = 0; i < len; i++) 
		if (buf[i] != i % 256)
		    fail_unless(buf[i] == i % 256);
	    recvd += len;
	    // std::cerr << " recv " << len;
	}
    }


};

class SyncListener : public Thread {
    Transport *listener;
    const char *addr;
    std::list<SyncReceiver*> recvrs;
public:
    SyncListener(const char *a) : addr(a) {
    }


    void run() {
	Transport* tp;
	while ((tp = listener->accept(0))) {
	    SyncReceiver* r = new SyncReceiver(tp);
	    recvrs.push_back(r);
	    r->start();
	}
    }
    
    void start() {
	listener = Transport::create(addr, 0);
	listener->listen(addr);
	Thread::start();
    }

    void stop() {
	Thread::stop();
	delete listener;

	for (std::list<SyncReceiver*>::iterator i = recvrs.begin();
	     i != recvrs.end(); ++i) {
	    (*i)->stop();
	    delete *i;
	}

    }
};

START_TEST(check_sync_transport)
{
    SyncListener l(sync_addr);
    l.start();

    SyncSender s(sync_addr);
    s.start();
    ::sleep(2);

    s.stop();
    l.stop();

}
END_TEST

const char* tipc_addr = "tipc:100.1";

class Mcaster : public Toplay {
    Transport *tp;
    unsigned char buf[128];
    uint32_t seq;
    size_t recvd;
    size_t hc;
    double recvd_b;
    std::map<Sockaddr,uint32_t> seq_map;
public:
    Mcaster(Poll *p) : seq(0), recvd(0), hc(0), recvd_b(0) {
	for (size_t i = 0; i < 128; ++i) {
	    buf[i] = i;
	}
	tp = Transport::create(tipc_addr, p, this);
	set_down_context(tp);
    }
    ~Mcaster() {
	LOG_INFO(std::string("Received ") + to_string(recvd) + " msgs " + 
		 to_string(recvd_b) + " bytes hc " + to_string(hc));
	for (std::map<Sockaddr,uint32_t>::iterator i = seq_map.begin(); 
	     i != seq_map.end(); ++i) {
	    LOG_INFO(std::string("seqno ") + to_string(i->second));;
	}
	delete tp;
    }

    void connect() {
	tp->connect(tipc_addr);
    }

    void close() {
	tp->close();
    }

    void handle_up(const int cid, const ReadBuf *rb, 
		   const size_t roff, const ProtoUpMeta *um) {
	// LOG_DEBUG(std::string("Mcaster::handle_up()"));
	hc++;
	const TransportNotification *tn = static_cast<const TransportNotification *>(um);
	if (rb) {
/*
	    for (size_t i = 0; i < tn->sa_size; ++i)
		fprintf(stderr, "%2.2x ", ((unsigned char*)&tn->local_sa)[i]);
	    fprintf(stderr, "\n");
	    for (size_t i = 0; i < tn->sa_size; ++i)
		fprintf(stderr, "%2.2x ", ((unsigned char*)&tn->source_sa)[i]);
	    fprintf(stderr, "\n");
*/    
	    uint32_t rs;
	    read_uint32(rb->get_buf(), rb->get_len(), 0, &rs);
	    // printf("%u\n", rs);

	    Sockaddr cmp(&tn->source_sa, tn->sa_size);	    
	    std::map<Sockaddr,uint32_t>::iterator i = seq_map.find(cmp);
	    if (i == seq_map.end()) {
		LOG_INFO(std::string("Unknown source, seqno start ") +
			 to_string(rs));
		seq_map.insert(std::pair<Sockaddr, uint32_t>(
				   Sockaddr(&tn->source_sa, 
					    tn->sa_size), rs));
	    } else {
		if (i->second + 1 != rs) {
		    LOG_WARN(std::string("Seqno gap: ") + to_string(i->second) + 
			     " -> " + to_string(rs));
		}
		i->second = rs;
	    }
	    
	    LOG_TRACE(std::string("seqno: ") + to_string(rs));
	    recvd++;
	    recvd_b += rb->get_len();
	} else if (um) {
	    if (tn->ntype == TRANSPORT_N_SUBSCRIBED) {
		fprintf(stderr, "subs: ");
		seq_map.insert(std::pair<Sockaddr, uint32_t>(
				   Sockaddr(&tn->source_sa, 
					    tn->sa_size), 0xffffffffU));
	    } else if (tn->ntype == TRANSPORT_N_WITHDRAWN) {

		Sockaddr cmp(&tn->source_sa, tn->sa_size);
		std::map<Sockaddr,uint32_t>::iterator i = seq_map.find(cmp);
		if (i != seq_map.end()) {
		    fprintf(stderr, "with (seq = %u): ", i->second);
		    seq_map.erase(i);
		}
	    }
	    for (size_t i = 0; i < tn->sa_size; ++i)
		fprintf(stderr, "%2.2x ", ((unsigned char*)&tn->source_sa)[i]);
	    fprintf(stderr, "\n");
	} else {
	    LOG_WARN("Empty");
	}
    }
    
    void send() {
	
	WriteBuf wb(buf, sizeof(buf));
	unsigned char buf[sizeof(seq)];
	write_uint32(seq, buf, sizeof(buf), 0);
	wb.prepend_hdr(buf, sizeof(buf));
	int ret;
	if ((ret = pass_down(&wb, 0)) == 0)
	    seq++;
	else
	    LOG_WARN(std::string("Pass down: ") + to_string(ret));
    }
};

START_TEST(check_tipc_multicast)
{
    std::cerr << "check_tipc_multicast\n";
    Poll *p = Poll::create("def");
    Mcaster m1(p);
    Mcaster m2(p);
    int cnt = 0;
    m1.connect();
    p->poll(1000);
    m2.connect();
    p->poll(1000);
    int pcnt = 0;
    while (cnt++ < 10000) {
	m1.send();
	m2.send();
	if (::rand() % 6 == 0)
	    while (p->poll(0) > 0) {}
    }
    LOG_INFO(std::string("pcnt: ") + to_string(pcnt));
    while (p->poll(100) > 0) {}
    m2.send();
    while (p->poll(100) > 0) {}

    m1.close();
    p->poll(1000);
    m2.close();
    delete p;
}
END_TEST

static Suite *suite()
{
    Suite *s = suite_create("transportpp");
    TCase *tc;

    tc = tcase_create("check_async_transport");
    tcase_add_test(tc, check_async_transport);
    // suite_add_tcase(s, tc);

    tc = tcase_create("check_async_multitransport");
    tcase_add_test(tc, check_async_multitransport);
    // suite_add_tcase(s, tc);

    tc = tcase_create("check_sync_transport");
    tcase_add_test(tc, check_sync_transport);
    // suite_add_tcase(s, tc);

    tc = tcase_create("check_tipc_multicast");
    tcase_add_test(tc, check_tipc_multicast);
    suite_add_tcase(s, tc);    
    
    return s;
}

int main()
{
    Suite *s;
    SRunner *sr;

    s = suite();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int n_fail = srunner_ntests_failed(sr);
    srunner_free(sr);
    return n_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
