extern "C" {
#include "gcs_vs.h"
#include "gu_mutex.h"
#include "gu_log.h"
}
#include <gcomm/vs.hpp>
// We access data comp msg struct directly
extern "C" {
#define GCS_COMP_MSG_ACCESS 1
#include "gcs_comp_msg.h"
}

#include <limits>

struct vs_ev {
    ReadBuf *rb;
    VSMessage *msg;
    VSView *view;
    size_t msg_size;
    vs_ev(const ReadBuf *r, const size_t ms, const VSMessage *m, const VSView *v) :
	rb(0), msg_size(ms), msg(0), view(0) {
	if (r)
	    rb = r->copy(m->get_data_offset());
	if (m)
	    msg = new VSMessage(*m);
	if (v)
	    view = new VSView(*v);
    }
};

class gcs_vs : public Toplay {
public:
    VS *vs;
    Poll *po;
    std::deque<vs_ev> eq;
    void *waiter_buf;
    size_t waiter_buf_len;
    gu_mutex_t mutex;
    gu_cond_t cond;
    Monitor monitor;
    enum State {JOINING, JOINED, LEFT} state;
    gcs_vs() : vs(0), po(0), waiter_buf(0), waiter_buf_len(0), state(JOINING) {
	gu_mutex_init(&mutex, 0);
	gu_cond_init(&cond, 0);
    }
    ~gcs_vs() {
	for (std::deque<vs_ev>::iterator i = eq.begin(); i != eq.end(); ++i) {
	    if (i->rb)
		i->rb->release();
	    if (i->msg)
		delete i->msg;
	    if (i->view)
		delete i->view;
	}
	gu_cond_destroy(&cond);
	gu_mutex_destroy(&mutex);
    }
    
    void handle_up(const int cid, const ReadBuf *rb, const size_t roff, 
		   const ProtoUpMeta *um) {
	const VSUpMeta *vum = static_cast<const VSUpMeta *>(um);
	// null rb and um denotes eof (broken connection)
	if (!(rb || vum)) {
	    gu_mutex_lock(&mutex);
	    eq.push_back(vs_ev(0, 0, 0, 0));
	    gu_cond_signal(&cond);
	    gu_mutex_unlock(&mutex);
	    return;
	}

	assert((rb && vum->msg) || vum->view);
	if (state == JOINING) {
	    assert(vum->view);
	    assert(vum->view->is_trans());
	    assert(vum->view->get_addr().size() == 0);
	    state = JOINED;
	    return;
	} else if (vum->view && vum->view->is_trans()) {
	    gu_debug("trans view: size = %u", 
		     vum->view->get_addr().size());
	    // Reached the end
	    // Todo: add gu_thread_exit() to gu library
	    if (vum->view->get_addr().size() == 0) {
		state = LEFT;
		gu_mutex_lock(&mutex);
		gu_cond_signal(&cond);
		gu_mutex_unlock(&mutex);
		pthread_exit(0);
	    }
	}
	gu_mutex_lock(&mutex);
	if (vum->msg && eq.empty() && rb->get_len(roff) <= waiter_buf_len) {
	    memcpy(waiter_buf, rb->get_buf(roff), rb->get_len(roff));
	    eq.push_back(vs_ev(0, rb->get_len(roff), vum->msg, vum->view));
	    // Zero pointer/len here to avoid rewriting the buffer if 
	    // waiter does not wake up before next message
	    waiter_buf = 0;
	    waiter_buf_len = 0;
	} else {
	    eq.push_back(vs_ev(rb, 0, vum->msg, vum->view));
	}
	gu_cond_signal(&cond);
	gu_mutex_unlock(&mutex);
    }
    std::pair<vs_ev, bool> wait_event(void* wb, size_t wb_len) {
	gu_mutex_lock(&mutex);
	while (eq.size() == 0 && state != LEFT) {
	    waiter_buf = wb;
	    waiter_buf_len = wb_len;
	    gu_cond_wait(&cond, &mutex);
	}
	std::pair<vs_ev, bool> ret(eq.front(), eq.size() && state != LEFT 
				   ? true : false);
	gu_mutex_unlock(&mutex);
	return ret;
    }
    void release_event() {
	assert(eq.size());
	gu_mutex_lock(&mutex);
	vs_ev ev = eq.front();
	eq.pop_front();
	if (ev.rb)
	    ev.rb->release();
	delete ev.msg;
	delete ev.view;
	gu_mutex_unlock(&mutex);
    }
};

typedef struct gcs_backend_conn {
    size_t last_view_size;
    size_t max_msg_size;
    unsigned long long n_received;
    unsigned long long n_copied;
    gcs_vs vs_ctx;
    gu_thread_t thr;
    gcs_comp_msg_t *comp_msg;
    std::map<Address, long> comp_map;
    gcs_backend_conn() : last_view_size(0), max_msg_size(1 << 20),
			 n_received(0), n_copied(0),
			 comp_msg(0) {}
} conn_t;



static GCS_BACKEND_MSG_SIZE_FN(gcs_vs_msg_size)
{
    return backend->conn->max_msg_size;
}

static GCS_BACKEND_SEND_FN(gcs_vs_send)
{
    conn_t *conn = backend->conn;

    if (conn == 0)
	return -EBADFD;
    if (conn->vs_ctx.vs == 0)
	return -ENOTCONN;
    if (msg_type < 0 || msg_type > 0xff)
	return -EINVAL;
    int err = 0;
    WriteBuf wb(buf, len);
    try {
	VSDownMeta vdm (0, msg_type);
	err = conn->vs_ctx.pass_down(&wb, &vdm);
    } catch (Exception e) {
	return -ENOTCONN;
    }

    
    return err == 0 ? len : -err;
}



static void fill_comp(gcs_comp_msg_t *msg, 
		      std::map<Address, long> *comp_map, 
		      Aset addrs, Address self)
{
    size_t n = 0;
    assert(msg != 0 && static_cast<size_t>(msg->memb_num) == addrs.size());
    if (comp_map)
	comp_map->clear();
    for (Aset::iterator i = addrs.begin(); i != addrs.end(); ++i) {
	snprintf(msg->memb[n].id, sizeof(msg->memb[n].id), "%4.4x.%2.2x.%2.2x",
		 i->get_proc_id().to_uint(), 
		 i->get_segment_id().to_uint(),		 
		 i->get_service_id().to_uint());
	if (*i == self)
	    msg->my_idx = n;
	if (comp_map)
	    comp_map->insert(std::pair<Address, long>(*i, n));
	n++;	    
    }
}


static GCS_BACKEND_RECV_FN(gcs_vs_recv)
{
    long ret = 0;
    long cpy = 0;
    conn_t *conn = backend->conn;
    if (conn == 0)
	return -EBADFD;
    
retry:
    std::pair<vs_ev, bool> wr(conn->vs_ctx.wait_event(buf, len));
    if (wr.second == false)
	return -ENOTCONN;
    vs_ev& ev(wr.first);

    if (!(ev.msg || ev.view))
	return -ENOTCONN;

    assert((ev.msg && (ev.rb || ev.msg_size))|| ev.view);

    if (ev.msg) {
	*msg_type = static_cast<gcs_msg_type_t>(ev.msg->get_user_type());
	std::map<Address, long>::iterator i = conn->comp_map.find(ev.msg->get_source());
	assert(i != conn->comp_map.end());
	*sender_id = i->second;
	if (ev.rb) {
	    ret = ev.rb->get_len();
	    if (ret <= len) {
		memcpy(buf, ev.rb->get_buf(), ret);
		conn->n_copied++;
	    }
	} else {
	    assert(ev.msg_size > 0);
	    ret = ev.msg_size;
	}
    } else {
	// This check should be enough:
	// - Reg view will definitely have more members than previous 
	//   trans view
	// - Check number of members that left *ungracefully* 
	//   (see fixme in CLOSE)
	// 
	//
	gcs_comp_msg_t *new_comp = 0;
	if (ev.view->is_trans() && conn->comp_msg && 
	    ev.view->get_addr().size()*2 + ev.view->get_left().size() 
	    < static_cast<size_t>(gcs_comp_msg_num(conn->comp_msg))) {
	    new_comp = gcs_comp_msg_new(false, 0, ev.view->get_addr().size());
	} else if (ev.view->is_trans()) {
	    // Drop transitional views that lead to prim comp
	    conn->vs_ctx.release_event();
	    goto retry;
	} else {
	    new_comp = gcs_comp_msg_new(true, 0, ev.view->get_addr().size());
	}

        if (!new_comp) {
            gu_fatal ("Failed to allocate new component message.");
            abort();
        }

	fill_comp(new_comp, ev.view->is_trans() ? 0 : &conn->comp_map, ev.view->get_addr(), conn->vs_ctx.vs->get_self());
	if (conn->comp_msg) gcs_comp_msg_delete(conn->comp_msg);
	conn->comp_msg = new_comp;
	cpy = std::min(static_cast<size_t>(gcs_comp_msg_size(conn->comp_msg)), len);
	ret = std::max(static_cast<size_t>(gcs_comp_msg_size(conn->comp_msg)), len);
	memcpy(buf, conn->comp_msg, cpy);
	*msg_type = GCS_MSG_COMPONENT;
    }
    if (static_cast<size_t>(ret) <= len) {
	conn->vs_ctx.release_event();
	conn->n_received++;
    }
    
    return ret;
}

static GCS_BACKEND_NAME_FN(gcs_vs_name)
{
    static const char *name = "vs";
    return name;
}

static void *conn_run(void *arg)
{
    conn_t *conn = reinterpret_cast<conn_t*>(arg);
    try {

	while (true) {
	    int err = conn->vs_ctx.po->poll(std::numeric_limits<int>::max());
	    if (err < 0) {
		gu_fatal("unrecoverable error: '%s'", strerror(err));
		abort();
	    }
	}
    } catch (Exception e) {
	gu_error("poll error: '%s', thread exiting", e.what());
	conn->vs_ctx.handle_up(-1, 0, 0, 0);
    }
    return 0;
}

static GCS_BACKEND_OPEN_FN(gcs_vs_open)
{
    conn_t *conn = backend->conn;

    if (!conn) return -EBADFD;

    try {
	conn->vs_ctx.vs->join(0, &conn->vs_ctx);
	int err = gu_thread_create(&conn->thr, 0, &conn_run, conn);
	if (err != 0)
	    return -err;
    } catch (Exception e) {
	return -EINVAL;
    }
    
    return 0;
}

static GCS_BACKEND_CLOSE_FN(gcs_vs_close)
{
    conn_t *conn = backend->conn;
    if (conn == 0)
	return -EBADFD;

    conn->vs_ctx.vs->leave(0);
    gu_thread_join(conn->thr, 0);

    return 0;
}

static GCS_BACKEND_DESTROY_FN(gcs_vs_destroy)
{
    conn_t *conn = backend->conn;
    if (conn == 0)
	return -EBADFD;
    backend->conn = 0;
    
    conn->vs_ctx.vs->close();
    delete conn->vs_ctx.vs;
    delete conn->vs_ctx.po;
    if (conn->comp_msg) gcs_comp_msg_delete (conn->comp_msg);

    gu_debug("received %llu copied %llu", conn->n_received, conn->n_copied);

    delete conn;
    
    gu_debug("gcs_vs_close(): return 0");
    return 0;
}

static const char* vs_default_socket = "tcp:127.0.0.1:4567";

GCS_BACKEND_CREATE_FN(gcs_vs_create)
{
    conn_t *conn = 0;
    const char* sock = socket;
    
    if (NULL == sock || strlen(sock) == 0)
        sock = vs_default_socket;

    gu_debug ("Opening connection to '%s'", sock);

    try {
	conn = new gcs_backend_conn;	
    } catch (std::bad_alloc e) {
	return -ENOMEM;
    }
    
    try {
	conn->vs_ctx.po = Poll::create("def");
	conn->vs_ctx.vs = VS::create(sock, conn->vs_ctx.po, 
				     &conn->vs_ctx.monitor);
	conn->vs_ctx.set_down_context(conn->vs_ctx.vs);
	conn->vs_ctx.vs->connect();
    } catch (Exception e) {
	delete conn;
	return -EINVAL;
    }
    conn->comp_msg = 0;
    
    backend->open     = &gcs_vs_open;
    backend->close    = &gcs_vs_close;
    backend->destroy  = &gcs_vs_destroy;
    backend->send     = &gcs_vs_send;
    backend->recv     = &gcs_vs_recv;
    backend->name     = &gcs_vs_name;
    backend->msg_size = &gcs_vs_msg_size;
    backend->conn     = conn;
    
    return 0;
}

