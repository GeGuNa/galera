/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#include <check.h>
#include <unistd.h>
#include <string.h>

#include "gcs_proto_test.h"
#include "../gcs_act_proto.h"

static long
frgcmp (gcs_act_frag_t* f1, gcs_act_frag_t* f2)
{
    if (
	(f1->act_id   == f2->act_id)   &&
	(f1->act_size == f2->act_size) &&
	(f1->act_type == f2->act_type) &&
	(f1->frag_len == f2->frag_len) && // expect to point
	(f1->frag     == f2->frag)        // at the same buffer here
	) return 0;
    else
	return -1;
}

START_TEST (gcs_proto_test)
{
    const char   act_send[]   = "Test action smuction";
    const char*  act_send_ptr = act_send;
    char         act_recv[]   = "owoeijrvfokpvfcsdnfvkmk;l";
    char*        act_recv_ptr = act_recv;
    const size_t buf_len      = 32;
    char         buf[buf_len];
    gcs_act_frag_t frg_send, frg_recv;
    long         ret;

    frg_send.act_id    = getpid();
    frg_send.act_size  = strlen (act_send);
    frg_send.frag      = NULL;
    frg_send.frag_len  = 0;
    frg_send.frag_no   = 0;
    frg_send.act_type  = 0;
    frg_send.proto_ver = 0;

    // set up action header
    ret = gcs_act_proto_write (&frg_send, buf, buf_len);
    fail_if (ret, "error code: %d", ret);
    fail_if (frg_send.frag     == NULL);
    fail_if (frg_send.frag_len == 0);
    fail_if (strlen(act_send) < frg_send.frag_len,
	     "Expected fragmentation, but action seems to fit in buffer"
	     " - increase send action length");

    // write action to the buffer, it should not fit
    strncpy ((char*)frg_send.frag, act_send_ptr, frg_send.frag_len);
    act_send_ptr += frg_send.frag_len;

    // message was sent and received, now parse the header
    ret = gcs_act_proto_read (&frg_recv, buf, buf_len);
    fail_if (ret, "error code: %d", ret);
    fail_if (frg_recv.frag     == NULL);
    fail_if (frg_recv.frag_len == 0);
    fail_if (frgcmp (&frg_send, &frg_recv),
	     "Sent and recvd headers are not identical");
    fail_if (frg_send.frag_no != frg_recv.frag_no,
	     "Fragment numbers are not identical: %d %d",
	     frg_send.frag_no, frg_recv.frag_no);

    // read the fragment into receiving action buffer
    // FIXME: this works by sheer luck - only because strncpy() pads
    // the remaining buffer space with 0
    strncpy (act_recv_ptr, frg_recv.frag, frg_recv.frag_len);
    act_recv_ptr += frg_recv.frag_len;

    // send the second fragment. Increment the fragment counter
    gcs_act_proto_inc (buf); // should be 1 now

    // write action to the buffer, it should fit now
    strncpy ((char*)frg_send.frag, act_send_ptr, frg_send.frag_len);
    //    act_send_ptr += frg_send.frag_len;

    // message was sent and received, now parse the header
    ret = gcs_act_proto_read (&frg_recv, buf, buf_len);
    fail_if (ret, "error code: %d", ret);
    fail_if (frgcmp (&frg_send, &frg_recv),
	     "Sent and recvd headers are not identical");
    fail_if (frg_send.frag_no + 1 != frg_recv.frag_no,
	     "Fragment numbers are not sequential: %d %d",
	     frg_send.frag_no, frg_recv.frag_no);

    // read the fragment into receiving action buffer
    // FIXME: this works by sheer luck - only because strncpy() pads
    // the remaining buffer space with 0
    strncpy (act_recv_ptr, frg_recv.frag, frg_recv.frag_len);
    fail_if (strlen(act_recv_ptr) >= frg_send.frag_len,
	     "Fragment does not seem to fit in buffer: '%s'(%d)",
	     strlen(act_recv_ptr), act_recv_ptr);

    // check that actions are identical
    fail_if (strcmp(act_send, act_recv), "Actions don't match: '%s' -- '%s'",
	     act_send, act_recv);
}
END_TEST

Suite *gcs_proto_suite(void)
{
  Suite *suite = suite_create("GCS Core protocol");
  TCase *tcase = tcase_create("gcs_proto");

  suite_add_tcase (suite, tcase);
  tcase_add_test  (tcase, gcs_proto_test);
  return suite;
}

