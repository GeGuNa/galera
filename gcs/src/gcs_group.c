/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#include <errno.h>

#include "gcs_group.h"

const char* gcs_group_state_str[GCS_GROUP_STATE_MAX] =
{
    "NON_PRIMARY",
    "WAIT_STATE_UUID",
    "WAIT_STATE_MSG",
    "PRIMARY"
};

long
gcs_group_init (gcs_group_t* group, const char* node_name, const char* inc_addr)
{
    // here we also create default node instance.
    group->act_id       = GCS_SEQNO_ILL;
    group->conf_id      = GCS_SEQNO_ILL;
    group->state_uuid   = GU_UUID_NIL;
    group->group_uuid   = GU_UUID_NIL;
    group->proto        = -1;
    group->num          = 1;
    group->my_idx       = 0;
    group->my_name      = strdup(node_name ? node_name : NODE_NO_NAME);
    group->my_address   = strdup(inc_addr  ? inc_addr  : NODE_NO_ADDR);
    group->state        = GCS_GROUP_NON_PRIMARY;
    group->last_applied = GCS_SEQNO_ILL; // mark for recalculation
    group->last_node    = -1;
    group->frag_reset   = true; // just in case
    group->nodes        = GU_CALLOC(group->num, gcs_node_t);

    if (!group->nodes) return -ENOMEM;

    gcs_node_init (&group->nodes[group->my_idx], NODE_NO_ID,
                   group->my_name, group->my_address);

    group->prim_uuid  = GU_UUID_NIL;
    group->prim_seqno = GCS_SEQNO_ILL;
    group->prim_num   = 0;
    group->prim_state = GCS_NODE_STATE_NON_PRIM;

    return 0;
}

long
gcs_group_init_history (gcs_group_t*     group,
                        gcs_seqno_t      seqno,
                        const gu_uuid_t* uuid)
{
    bool negative_seqno = (seqno < 0);
    bool nil_uuid = !gu_uuid_compare (uuid, &GU_UUID_NIL);

    if (negative_seqno && !nil_uuid) {
        gu_error ("Non-nil history UUID with negative seqno (%lld) makes "
                  "no sense.", (long long) seqno);
        return -EINVAL;
    }
    else if (!negative_seqno && nil_uuid) {
        gu_error ("Non-negative state seqno requires non-nil history UUID.");
        return -EINVAL;
    }
    group->act_id     = seqno;
    group->group_uuid = *uuid;
    return 0;
}

/* Initialize nodes array from component message */
static inline gcs_node_t*
group_nodes_init (const gcs_group_t* group, const gcs_comp_msg_t* comp)
{
    const long my_idx     = gcs_comp_msg_self (comp);
    const long nodes_num  = gcs_comp_msg_num  (comp);
    gcs_node_t* ret = GU_CALLOC (nodes_num, gcs_node_t);
    long i;

    if (ret) {
        for (i = 0; i < nodes_num; i++) {
            if (my_idx != i) {
                gcs_node_init (&ret[i], gcs_comp_msg_id (comp, i), NULL, NULL);
            }
            else { // this node
                gcs_node_init (&ret[i], gcs_comp_msg_id (comp, i),
                               group->nodes[group->my_idx].name,
                               group->nodes[group->my_idx].inc_addr);
            }
        }
    }
    else {
        gu_error ("Could not allocate %ld x %z bytes", nodes_num,
                  sizeof(gcs_node_t));
    }
    return ret;
}

/* Free nodes array */
static inline void
group_nodes_free (gcs_group_t* group)
{
    register long i;

    /* cleanup after disappeared members */
    for (i = 0; i < group->num; i++) {
        gcs_node_free (&group->nodes[i]);
    }

    if (group->nodes) gu_free (group->nodes);
}

void
gcs_group_free (gcs_group_t* group)
{
    if (group->my_name)    free ((char*)group->my_name);
    if (group->my_address) free ((char*)group->my_address);
    group_nodes_free (group);
}

/* Reset nodes array without breaking the statistics */
static inline void
group_nodes_reset (gcs_group_t* group)
{
    register long i;
    /* reset recv_acts at the nodes */
    for (i = 0; i < group->num; i++) {
        if (i != group->my_idx) {
            gcs_node_reset (&group->nodes[i]);
        }
        else {
            gcs_node_reset_local (&group->nodes[i]);
        }
    }

    group->frag_reset = true;
}

/* Find node with the smallest last_applied */
static inline void
group_redo_last_applied (gcs_group_t* group)
{
    long n;

    group->last_node    = 0;
    group->last_applied = gcs_node_get_last_applied (&group->nodes[0]);
//    gu_debug (" last_applied[0]: %lld", group->last_applied);

    for (n = 1; n < group->num; n++) {
        gcs_seqno_t seqno = gcs_node_get_last_applied (&group->nodes[n]);
//        gu_debug ("last_applied[%ld]: %lld", n, seqno);
        if (seqno < group->last_applied) {
            group->last_applied = seqno;
            group->last_node    = n;
        }
    }
}

static void
group_go_non_primary (gcs_group_t* group)
{
    if (GCS_GROUP_PRIMARY == group->state) {
        group->nodes[group->my_idx].status = GCS_NODE_STATE_NON_PRIM;
        //@todo: Perhaps the same has to be applied to the rest of the nodes[]?
    }

    group->state   = GCS_GROUP_NON_PRIMARY;
    group->conf_id = GCS_SEQNO_ILL;
    // what else? Do we want to change anything about the node here?
}

/*! Processes state messages and sets group parameters accordingly */
static void
group_post_state_exchange (gcs_group_t* group)
{
    const gcs_state_msg_t* states[group->num];
    gcs_state_quorum_t quorum;
    bool new_exchange = gu_uuid_compare (&group->state_uuid, &GU_UUID_NIL);
    long i;

    /* Collect state messages from nodes. */
    /* Looping here every time is suboptimal, but simply counting state messages
     * is not straightforward too: nodes may disappear, so the final count may
     * include messages from the disappeared nodes.
     * Let's put it this way: looping here is reliable and not that expensive.*/
    for (i = 0; i < group->num; i++) {
        states[i] = group->nodes[i].state_msg;
        if (NULL == states[i] ||
            (new_exchange &&
             gu_uuid_compare (&group->state_uuid,
                              gcs_state_msg_uuid(states[i]))))
            return; // not all states from THIS state exch. received, wait
    }
    gu_debug ("STATE EXCHANGE: "GU_UUID_FORMAT" complete.",
              GU_UUID_ARGS(&group->state_uuid));

    gcs_state_msg_get_quorum (states, group->num, &quorum);

    if (quorum.primary) {
        // primary configuration
        group->proto = quorum.proto;
        if (new_exchange) {
            // new state exchange happened
            group->state      = GCS_GROUP_PRIMARY;
            group->act_id     = quorum.act_id;
            group->conf_id    = quorum.conf_id + 1;
            group->group_uuid = quorum.group_uuid;

            // Update each node state based on quorum outcome:
            // is it up to date, does it need SST and stuff
            for (i = 0; i < group->num; i++) {
                gcs_node_update_status (&group->nodes[i], &quorum);
            }

            group->prim_uuid  = group->state_uuid;
            group->state_uuid = GU_UUID_NIL;
        }
        else {
            // no state exchange happend, processing old state messages
            assert (GCS_GROUP_PRIMARY == group->state);
            group->conf_id++;
        }

        group->prim_seqno = group->conf_id;
        group->prim_num   = group->num;
    }
    else {
        // non-primary configuration
        group_go_non_primary (group);
    }

    gu_info ("Quorum results:"
             "\n\t%s,"
             "\n\tact_id     = %lld,"
             "\n\tconf_id    = %lld,"
             "\n\tlast_appl. = %lld,"
             "\n\tprotocol   = %hd,"
             "\n\tgroup UUID = "GU_UUID_FORMAT,
             quorum.primary ? "PRIMARY" : "NON-PRIMARY",
             quorum.act_id, quorum.conf_id, group->last_applied, quorum.proto,
             GU_UUID_ARGS(&quorum.group_uuid));
}

// does basic sanity check of the component message (in response to #145)
static void
group_check_comp_msg (bool prim, long my_idx, long members)
{
    if (my_idx >= 0) {
        if (my_idx < members) return;
    }
    else {
        if (!prim && (0 == members)) return;
    }

    gu_fatal ("Malformed component message from backend: "
              "%s, idx = %ld, members = %ld",
              prim ? "PRIMARY" : "NON-PRIMARY", my_idx, members);

    assert (0);
    abort ();
}

gcs_group_state_t
gcs_group_handle_comp_msg (gcs_group_t* group, const gcs_comp_msg_t* comp)
{
    long        new_idx, old_idx;
    gcs_node_t* new_nodes = NULL;
    ulong       new_memb  = 0;

    const bool prim_comp     = gcs_comp_msg_primary(comp);
    const long new_my_idx    = gcs_comp_msg_self   (comp);
    const long new_nodes_num = gcs_comp_msg_num    (comp);

    group_check_comp_msg (prim_comp, new_my_idx, new_nodes_num);

    if (new_my_idx >= 0) {
        gu_info ("New COMPONENT: primary = %s, my_idx = %ld, memb_num = %ld",
                 prim_comp ? "yes" : "no", new_my_idx, new_nodes_num);

        new_nodes = group_nodes_init (group, comp);

        if (!new_nodes) {
            gu_fatal ("Could not allocate memory for %ld-node component.",
                      gcs_comp_msg_num (comp));
            assert(0);
            return -ENOMEM;
        }

        if (GCS_GROUP_PRIMARY == group->state) {
            gu_debug ("#281: Saving %s over %s",
                      gcs_node_state_to_str(group->nodes[group->my_idx].status),
                      gcs_node_state_to_str(group->prim_state));
            group->prim_state = group->nodes[group->my_idx].status;
        }
    }
    else {
        // Self-leave message
        gu_info ("Received self-leave message.");
        assert (0 == new_nodes_num);
        assert (!prim_comp);
    }

    if (prim_comp) {
        /* Got PRIMARY COMPONENT - Hooray! */
        assert (new_my_idx >= 0);
        if (group->state == GCS_GROUP_PRIMARY) {
            /* we come from previous primary configuration, relax */
        }
        else {
            const bool first_component =
#ifndef GCS_CORE_TESTING
            (1 == group->num) && !strcmp (NODE_NO_ID, group->nodes[0].id);
#else
            (1 == group->num);
#endif

            if (1 == new_nodes_num && first_component) {
                /* bootstrap new configuration */
                assert (GCS_GROUP_NON_PRIMARY == group->state);
                assert (1 == group->num);
                assert (0 == group->my_idx);

                // This bootstraps initial primary component for state exchange
                gu_uuid_generate (&group->prim_uuid, NULL, 0);
                group->prim_seqno = 0;
                group->prim_num   = 1;
                group->state      = GCS_GROUP_PRIMARY;

                if (group->act_id < 0) {
                    // no history provided: start a new one
                    group->act_id  = GCS_SEQNO_NIL;
                    gu_uuid_generate (&group->group_uuid, NULL, 0);
                    gu_info ("Starting new group from scratch: "GU_UUID_FORMAT,
                             GU_UUID_ARGS(&group->group_uuid));
                }

                group->nodes[0].status = GCS_NODE_STATE_JOINED;
                /* initialize node ID to the one given by the backend - this way
                 * we'll be recognized as coming from prev. conf. in node array
                 * remap below */
                strncpy ((char*)group->nodes[0].id, new_nodes[0].id,
                         sizeof (new_nodes[0].id) - 1);
            }
        }
    }
    else {
        group_go_non_primary (group);
    }

    /* Remap old node array to new one to preserve action continuity */
    assert (group->nodes);
    for (new_idx = 0; new_idx < new_nodes_num; new_idx++) {
        /* find member index in old component by unique member id */
        for (old_idx = 0; old_idx < group->num; old_idx++) {
            // just scan through old group
            if (!strcmp(group->nodes[old_idx].id, new_nodes[new_idx].id)) {
                /* the node was in previous configuration with us */
                /* move node context to new node array */
                gcs_node_move (&new_nodes[new_idx], &group->nodes[old_idx]);
                break;
            }
        }
        /* if wasn't found in new configuration, new member -
         * need to do state exchange */
        new_memb |= (old_idx == group->num);
    }

    /* free old nodes array */
    group_nodes_free (group);

    group->my_idx = new_my_idx;
    group->num    = new_nodes_num;
    group->nodes  = new_nodes;

    if (gcs_comp_msg_primary(comp)) {
        /* TODO: for now pretend that we always have new nodes and perform
         * state exchange because old states can carry outdated node status.
         * However this means aborting ongoing actions. Find a way to avoid
         * this extra state exchange. Generate new state messages on behalf
         * of other nodes? see #238 */
        new_memb = true;
        /* if new nodes joined, reset ongoing actions and state messages */
        if (new_memb) {
            group_nodes_reset (group);
            group->state      = GCS_GROUP_WAIT_STATE_UUID;
            group->state_uuid = GU_UUID_NIL; // prepare for state exchange
        }
        else {
            if (GCS_GROUP_PRIMARY == group->state) {
                /* since we don't have any new nodes since last PRIMARY,
                   we skip state exchange */
                group_post_state_exchange (group);
            }
        }
        group_redo_last_applied (group);
    }

    return group->state;
}

gcs_group_state_t
gcs_group_handle_uuid_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg)
{
    assert (msg->size == sizeof(gu_uuid_t));

    if (GCS_GROUP_WAIT_STATE_UUID == group->state &&
        0 == msg->sender_idx /* check that it is from the representative */) {
        group->state_uuid = *(gu_uuid_t*)msg->buf;
        group->state      = GCS_GROUP_WAIT_STATE_MSG;
    }
    else {
        gu_warn ("Stray state UUID msg: "GU_UUID_FORMAT
                 " from node %ld (%s), current group state %s",
                 GU_UUID_ARGS(&group->state_uuid),
                 msg->sender_idx, group->nodes[msg->sender_idx].name,
                 gcs_group_state_str[group->state]);
    }

    return group->state;
}

static void group_print_state_debug(gcs_state_msg_t* state)
{
    size_t str_len = 1024;
    char state_str[str_len];
    gcs_state_msg_snprintf (state_str, str_len, state);
    gu_debug ("%s", state_str);
}

gcs_group_state_t
gcs_group_handle_state_msg (gcs_group_t* group, const gcs_recv_msg_t* msg)
{
    if (GCS_GROUP_WAIT_STATE_MSG == group->state) {

        gcs_state_msg_t* state = gcs_state_msg_read (msg->buf, msg->size);

        if (state) {

            const gu_uuid_t* state_uuid = gcs_state_msg_uuid (state);

            if (!gu_uuid_compare(&group->state_uuid, state_uuid)) {

                gu_info ("STATE EXCHANGE: got state msg: "GU_UUID_FORMAT
                         " from %ld (%s)", GU_UUID_ARGS(state_uuid),
                         msg->sender_idx, gcs_state_msg_name(state));

                if (gu_log_debug) group_print_state_debug(state);

                gcs_node_record_state (&group->nodes[msg->sender_idx], state);
                group_post_state_exchange (group);
            }
            else {
                gu_debug ("STATE EXCHANGE: stray state msg: "GU_UUID_FORMAT
                          " from node %ld (%s), current state UUID: "
                          GU_UUID_FORMAT,
                          GU_UUID_ARGS(state_uuid),
                          msg->sender_idx, gcs_state_msg_name(state),
                          GU_UUID_ARGS(&group->state_uuid));

                if (gu_log_debug) group_print_state_debug(state);

                gcs_state_msg_destroy (state);
            }
        }
        else {
            gu_warn ("Could not parse state message from node %d",
                     msg->sender_idx, group->nodes[msg->sender_idx].name);
        }
    }

    return group->state;
}

/*! Returns new last applied value if it has changes, 0 otherwise */
gcs_seqno_t
gcs_group_handle_last_msg (gcs_group_t* group, const gcs_recv_msg_t* msg)
{
    gcs_seqno_t seqno;

    assert (GCS_MSG_LAST        == msg->type);
    assert (sizeof(gcs_seqno_t) == msg->size);

    seqno = gcs_seqno_le(*(gcs_seqno_t*)(msg->buf));

    // This assert is too restrictive. It requires application to send
    // last applied messages while holding TO, otherwise there's a race
    // between threads.
    // assert (seqno >= group->last_applied);

    gcs_node_set_last_applied (&group->nodes[msg->sender_idx], seqno);

    if (msg->sender_idx == group->last_node && seqno > group->last_applied) {
        /* node that was responsible for the last value, has changed it.
         * need to recompute it */
        gcs_seqno_t old_val = group->last_applied;
        group_redo_last_applied (group);
        if (old_val != group->last_applied) {
            return group->last_applied;
        }
    }

    return 0;
}

/*! return true if this node is the sender to notify the calling thread of
 * success */
long
gcs_group_handle_join_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg)
{
    long        sender_idx = msg->sender_idx;
    gcs_node_t* sender     = &group->nodes[sender_idx];

    assert (GCS_MSG_JOIN == msg->type);

    // TODO: define an explicit type for the join message, like gcs_join_msg_t
    assert (msg->size == sizeof(gcs_seqno_t));

    if (GCS_NODE_STATE_DONOR  == sender->status ||
        GCS_NODE_STATE_JOINER == sender->status) {
        long j;
        gcs_seqno_t seqno     = gcs_seqno_le(*(gcs_seqno_t*)msg->buf);
        gcs_node_t* peer      = NULL;
        const char* peer_id   = NULL;
        const char* peer_name = "left the group";
        long        peer_idx  = -1;
        bool        from_donor = false;
        const char* st_dir    = NULL; // state transfer direction symbol

        if (GCS_NODE_STATE_DONOR == sender->status) {
            peer_id    = sender->joiner;
            from_donor = true;
            st_dir     = "to";
            sender->status = GCS_NODE_STATE_JOINED;
        }
        else {
            peer_id = sender->donor;
            st_dir  = "from";
            if (seqno >= 0) sender->status = GCS_NODE_STATE_JOINED;
        }

        // Try to find peer.
        for (j = 0; j < group->num; j++) {
            if (j == sender_idx) continue;
            if (!memcmp(peer_id, group->nodes[j].id,
                        sizeof (group->nodes[j].id))) {
                peer_idx  = j;
                peer      = &group->nodes[peer_idx];
                peer_name = peer->name;
                break;
            }
        }

        if (j == group->num) {
            gu_warn ("Could not find peer: %s", peer_id);
        }

        if (seqno < 0) {
            gu_warn ("%ld(%s): State transfer %s %ld(%s) failed: "
                     "%d (%s)",
                     sender_idx, sender->name, st_dir, peer_idx, peer_name,
                     (int)seqno, strerror((int)-seqno));

            if (from_donor && peer_idx == group->my_idx &&
                GCS_NODE_STATE_JOINER == group->nodes[peer_idx].status) {
                // this node will be waiting for SST forever. If it has only
                // one recv thread there is no (generic) way to wake it up.
                gu_fatal ("Will never receive state. Need to abort.");
                return -ENOTRECOVERABLE;
                // abort();
            }
        }
        else {
            gu_info ("%ld(%s): State transfer %s %ld(%s) complete.",
                     sender_idx, sender->name, st_dir, peer_idx, peer_name);
        }
    }
    else {
        if (GCS_NODE_STATE_PRIM == sender->status) {
            gu_warn ("Rejecting JOIN message from %ld (%s): new State Transfer"
                     " required.", sender_idx, sender->name);
        }
        else {
            // should we freak out and throw an error?
            gu_warn ("Protocol violation. JOIN message sender %ld (%s) is not "
                     "in state transfer (%s). Message ignored.", sender_idx,
                     sender->name, gcs_node_state_to_str(sender->status));
        }
        return 0;
    }

    return (sender_idx == group->my_idx);
}

long
gcs_group_handle_sync_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg)
{
    long        sender_idx = msg->sender_idx;
    gcs_node_t* sender     = &group->nodes[sender_idx];

    assert (GCS_MSG_SYNC == msg->type);

    if (GCS_NODE_STATE_JOINED == sender->status) {

        sender->status = GCS_NODE_STATE_SYNCED;

        gu_info ("Member %ld (%s) synced with group.",
                 sender_idx, sender->name);

        return (sender_idx == group->my_idx);
    }
    else {
        if (GCS_NODE_STATE_SYNCED != sender->status) {
            gu_warn ("SYNC message sender from non-joined %ld (%s). Ignored.",
                     msg->sender_idx, sender->name);
        }
        else {
            gu_debug ("Redundant SYNC message from %ld (%s).",
                      msg->sender_idx, sender->name);
        }
        return 0;
    }
}

static long
group_find_node_by_name (gcs_group_t* group, long joiner_idx, const char* name)
{
    long idx;
    for (idx = 0; idx < group->num; idx++) {
        gcs_node_t* node = &group->nodes[idx];
        if (!strcmp(node->name, name)) {
            if (joiner_idx == idx) {
                return -EHOSTDOWN;
            }
            else if (node->status >= GCS_NODE_STATE_JOINED) {
                return idx;
            }
            else {
                return -EAGAIN;
            }
        }
    }
    return -EHOSTUNREACH;
}

static long
group_find_node_by_status (gcs_group_t* group, gcs_node_state_t status)
{
    long idx;
    for (idx = 0; idx < group->num; idx++) {
        gcs_node_t* node = &group->nodes[idx];
        if (status == node->status) return idx;
    }
    return -EAGAIN;
}

/*!
 * Selects and returns the index of state transfer donor, if available.
 * Updates donor and joiner status if state transfer is possible
 *
 * @return
 *         donor index or negative error code:
 *         -EHOSTUNREACH if reqiested donor is not available
 *         -EAGAIN       if there were no nodes in the proper state.
 */
static long
group_select_donor (gcs_group_t* group, long joiner_idx, const char* donor_name)
{
    long donor_idx;
    bool required_donor = (strlen(donor_name) > 0);

    if (required_donor) {
        donor_idx = group_find_node_by_name (group, joiner_idx, donor_name);
    }
    else {
        // first, check SYNCED, they can process state request immediately
        donor_idx = group_find_node_by_status (group, GCS_NODE_STATE_SYNCED);
        if (donor_idx < 0) {
            // then check simply JOINED, they have full state
            donor_idx = group_find_node_by_status (group,GCS_NODE_STATE_JOINED);
        }
    }

    if (donor_idx >= 0) {
        gcs_node_t* joiner = &group->nodes[joiner_idx];
        gcs_node_t* donor  = &group->nodes[donor_idx];

        assert(donor_idx != joiner_idx);

        gu_info ("Node %ld (%s) requested State Transfer from '%s'. "
                 "Selected %ld (%s)(%s) as donor.",
                 joiner_idx, joiner->name, required_donor ? donor_name :"*any*",
                 donor_idx, donor->name, gcs_node_state_to_str(donor->status));

        // reserve donor, confirm joiner
        donor->status  = GCS_NODE_STATE_DONOR;
        joiner->status = GCS_NODE_STATE_JOINER;
        memcpy (donor->joiner, joiner->id, GCS_COMP_MEMB_ID_MAX_LEN+1);
        memcpy (joiner->donor, donor->id,  GCS_COMP_MEMB_ID_MAX_LEN+1);
    }
    else {
        gu_warn ("Node %ld (%s) requested State Transfer from '%s', "
                 "but it is impossible to select State Transfer donor: %s",
                 joiner_idx, group->nodes[joiner_idx].name,
                 required_donor ? donor_name : "*any*", strerror (-donor_idx));
    }

    return donor_idx;
}

/* Cleanup ignored state request */
void
gcs_group_ignore_action (struct gcs_act_rcvd* act)
{
    if (act->act.type <= GCS_ACT_STATE_REQ) free ((void*)act->act.buf);

    act->act.buf     = NULL;
    act->act.buf_len = 0;
    act->act.type    = GCS_ACT_ERROR;
    act->sender_idx  = -1;
    assert (GCS_SEQNO_ILL == act->id);
}

/* NOTE: check gcs_request_state_transfer() for sender part. */
/*! Returns 0 if request is ignored, request size if it should be passed up */
long
gcs_group_handle_state_request (gcs_group_t*         group,
                                long                 joiner_idx,
                                struct gcs_act_rcvd* act)
{
    // pass only to sender and to one potential donor
    const char*      donor_name     = act->act.buf;
    size_t           donor_name_len = strlen(donor_name);
    long             donor_idx      = -1;
    const char*      joiner_name    = group->nodes[joiner_idx].name;
    gcs_node_state_t joiner_status  = group->nodes[joiner_idx].status;

    assert (GCS_ACT_STATE_REQ == act->act.type);

    if (joiner_status != GCS_NODE_STATE_PRIM) {

        const char* joiner_status_string = gcs_node_state_to_str(joiner_status);

        if (group->my_idx == joiner_idx) {
            gu_error ("Requesting state transfer while in %s. "
                      "Ignoring.", joiner_status_string);
            act->id = -ECANCELED;
            return act->act.buf_len;
        }
        else {
            gu_error ("Node %ld (%s) requested state transfer, "
                      "but its state is %s. Ignoring.",
                      joiner_idx, joiner_name, joiner_status_string);
            gcs_group_ignore_action (act);
            return 0;
        }
    }

    donor_idx = group_select_donor(group, joiner_idx, donor_name);

    assert (donor_idx != joiner_idx);

    if (group->my_idx != joiner_idx && group->my_idx != donor_idx) {
        // if neither DONOR nor JOINER, ignore request
        gcs_group_ignore_action (act);
        return 0;
    }
    else if (group->my_idx == donor_idx) {
        act->act.buf_len -= donor_name_len + 1;
        memmove (*(void**)&act->act.buf,
                 act->act.buf + donor_name_len+1,
                 act->act.buf_len);
        // now action starts with request, like it was supplied by application,
        // see gcs_request_state_transfer()
    }

    // Return index of donor (or error) in the seqno field to sender.
    // It will be used to detect error conditions (no availabale donor,
    // donor crashed and the like).
    // This may be ugly, well, any ideas?
    act->id = donor_idx;

    return act->act.buf_len;
}

/* Creates new configuration action */
ssize_t
gcs_group_act_conf (gcs_group_t* group, struct gcs_act* act)
{
    ssize_t conf_size = sizeof(gcs_act_conf_t) + group->num*GCS_MEMBER_NAME_MAX;
    gcs_act_conf_t* conf = malloc (conf_size);

    if (conf) {
        long idx;

        conf->seqno       = group->act_id;
        conf->conf_id     = group->conf_id;
        conf->memb_num    = group->num;
        conf->my_idx      = group->my_idx;
        memcpy (conf->group_uuid, &group->group_uuid, sizeof (gu_uuid_t));

        if (group->num) {
            assert (conf->my_idx >= 0);
//            conf->st_required = (group->conf_id >= 0) &&
//                (group->nodes[group->my_idx].status < GCS_NODE_STATE_JOINER);
            conf->my_state = group->nodes[group->my_idx].status;

            for (idx = 0; idx < group->num; idx++)
            {
                char* node_id = &conf->data[idx * GCS_MEMBER_NAME_MAX];
                strncpy (node_id, group->nodes[idx].id, GCS_MEMBER_NAME_MAX);
                node_id[GCS_MEMBER_NAME_MAX - 1] = '\0';
            }
        }
        else {
            // self leave message
            assert (conf->conf_id < 0);
            assert (conf->my_idx  < 0);
            conf->my_state = GCS_NODE_STATE_NON_PRIM;
        }

        act->buf     = conf;
        act->buf_len = conf_size;
        act->type    = GCS_ACT_CONF;

        return conf_size;
    }
    else {
        return -ENOMEM;
    }
}

// for future use in fake state exchange (in unit tests et.al. See #237, #238)
static gcs_state_msg_t*
group_get_node_state (gcs_group_t* group, long node_idx)
{
    const gcs_node_t* node = &group->nodes[node_idx];

    uint8_t flags = 0;

    if (0 == node_idx) flags |= GCS_STATE_FREP;

    return gcs_state_msg_create (
        &group->state_uuid,
        &group->group_uuid,
        &group->prim_uuid,
        group->prim_num,
        group->prim_seqno,
        group->act_id,
        group->prim_state,
        node->status,
        node->name,
        node->inc_addr,
        node->proto_min,
        node->proto_max,
        flags
        );
}

/*! Returns state message object for this node */
gcs_state_msg_t*
gcs_group_get_state (gcs_group_t* group)
{
    return group_get_node_state (group, group->my_idx);
}
