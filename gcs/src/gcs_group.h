/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

/*
 * This header defines node specific context we need to maintain
 */

#ifndef _gcs_group_h_
#define _gcs_group_h_

#include <galerautils.h>

#include "gcs_node.h"
#include "gcs_recv_msg.h"
#include "gcs_seqno.h"
#include "gcs_state.h"

typedef enum gcs_group_state
{
    GCS_GROUP_NON_PRIMARY,
    GCS_GROUP_WAIT_STATE_UUID,
    GCS_GROUP_WAIT_STATE_MSG,
    GCS_GROUP_PRIMARY,
    GCS_GROUP_STATE_MAX
}
gcs_group_state_t;

typedef struct gcs_group
{
    gcs_seqno_t   act_id;       // current(last) action seqno
    gcs_seqno_t   conf_id;      // current configuration seqno
    gu_uuid_t     state_uuid;   // state exchange id
    gu_uuid_t     group_uuid;   // group UUID
    gcs_proto_t   proto;        // protocol version to use
    long          num;          // number of nodes
    long          my_idx;       // my index in the group
    gcs_group_state_t state;    // group state: PRIMARY | NON_PRIMARY
    volatile
    gcs_seqno_t   last_applied; // last_applied action group-wide
    long          last_node;    // node that reported last_applied
    gcs_node_t*   nodes;        // array of node contexts
}
gcs_group_t;

/*!
 * Initialize group at startup
 */
extern long
gcs_group_init (gcs_group_t* group);

/*!
 * Initialize group action history parameters. See gcs.h
 */
extern long
gcs_group_init_history (gcs_group_t*     group,
                        gcs_seqno_t      seqno,
                        const gu_uuid_t* uuid);

/*!
 * Free group resources
 */
extern void
gcs_group_free (gcs_group_t* group);

/*!
 * Handles component message - installs new membership,
 * cleans old one.
 *
 * @return
 *        group state in case of success or
 *        negative error code.
 */
extern gcs_group_state_t
gcs_group_handle_comp_msg  (gcs_group_t* group, const gcs_comp_msg_t* msg);

extern gcs_group_state_t
gcs_group_handle_uuid_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg);

extern gcs_group_state_t
gcs_group_handle_state_msg (gcs_group_t* group, const gcs_recv_msg_t* msg);

extern gcs_seqno_t
gcs_group_handle_last_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg);

/*! @return 0 for success, 1 for (success && i_am_sender)
 * or negative error code */
extern long
gcs_group_handle_join_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg);

/*! @return 0 for success, 1 for (success && i_am_sender)
 * or negative error code */
extern long
gcs_group_handle_sync_msg  (gcs_group_t* group, const gcs_recv_msg_t* msg);

extern long
gcs_group_handle_state_request (gcs_group_t*    group,
                                long            joiner_idx,
                                gcs_recv_act_t* act);
/*!
 * Handles action message. Is called often - therefore, inlined
 *
 * @return negative - error code, 0 - continue, positive - complete action
 */
static inline ssize_t
gcs_group_handle_act_msg (gcs_group_t*          group,
                          const gcs_act_frag_t* frg,
                          const gcs_recv_msg_t* msg,
                          gcs_recv_act_t*       act)
{
    register long sender_idx = msg->sender_idx;
    register ssize_t ret;

    assert (GCS_MSG_ACTION == msg->type);
    assert (sender_idx < group->num);

    ret = gcs_node_handle_act_frag (&group->nodes[sender_idx],
                                    frg, act, (sender_idx == group->my_idx));
    if (gu_unlikely(ret > 0)) {

        assert (ret == act->buf_len);

        act->type       = frg->act_type;
        act->sender_idx = sender_idx;

        if (gu_likely(GCS_ACT_DATA == act->type &&
                      GCS_GROUP_PRIMARY == group->state)) {
            // increment act_id only for DATA (should it be renamed to TOTAL?)
            // and only in PRIM (skip messages while in state exchange)
            act->id = ++group->act_id;
        } else if (act->type == GCS_ACT_STATE_REQ) {
            ret = gcs_group_handle_state_request (group, sender_idx, act);
        }
    }

    return ret;
}

static inline gcs_group_state_t
gcs_group_state (gcs_group_t* group)
{
    return group->state;
}

static inline bool
gcs_group_is_primary (gcs_group_t* group)
{
    return (GCS_GROUP_PRIMARY == group->state);
}

static inline long
gcs_group_my_idx (gcs_group_t* group)
{
    return group->my_idx;
}

/*! Creates new configuration action */
extern ssize_t
gcs_group_act_conf (gcs_group_t* group, gcs_recv_act_t* act);

/*! Returns state object for state message */
extern gcs_state_t*
gcs_group_get_state (gcs_group_t* group);

#endif /* _gcs_group_h_ */
