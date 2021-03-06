/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#include <check.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "gcs_defrag_test.h"
#include "../gcs_defrag.h"

#define TRUE (0 == 0)
#define FALSE (!TRUE)

static void
defrag_check_init (gcs_defrag_t* defrag)
{
    fail_if (defrag->sent_id  != GCS_SEQNO_ILL);
    fail_if (defrag->head     != NULL);
    fail_if (defrag->tail     != NULL);
    fail_if (defrag->size     != 0);
    fail_if (defrag->received != 0);
    fail_if (defrag->frag_no  != 0);
}

START_TEST (gcs_defrag_test)
{
    ssize_t ret;

    // The Action
    const char   act_buf[]  = "Test action smuction";
    size_t       act_len    = sizeof (act_buf);

    // lengths of three fragments of the action
    size_t       frag1_len  = act_len / 3;
    size_t       frag2_len  = frag1_len;
    size_t       frag3_len  = act_len - frag1_len - frag2_len;

    // pointer to the three fragments of the action
    const char*  frag1      = act_buf;
    const char*  frag2      = frag1 + frag1_len;
    const char*  frag3      = frag2 + frag2_len;

    // recv fragments
    gcs_act_frag_t frg1, frg2, frg3, frg4;

    gcs_defrag_t defrag;
    gcs_recv_act_t recv_act;

    void* tail;

    mark_point();

#ifndef NDEBUG
    // debug build breaks this test due to asserts
    return;
#endif

    // Initialize message parameters
    frg1.act_id    = getpid();
    frg1.act_size  = act_len;
    frg1.frag      = frag1;
    frg1.frag_len  = frag1_len;
    frg1.frag_no   = 0;
    frg1.act_type  = GCS_ACT_DATA;
    frg1.proto_ver = 0;

    // normal fragments
    frg2 = frg3 = frg1;

    frg2.frag     = frag2;
    frg2.frag_len = frag2_len;
    frg2.frag_no  = frg1.frag_no + 1;
    frg3.frag     = frag3;
    frg3.frag_len = frag3_len;
    frg3.frag_no  = frg2.frag_no + 1;

    // bad fragmets to be tried instead of frg2
    frg4 = frg2;
    frg4.frag     = "junk";
    frg4.frag_len = strlen("junk");
    frg4.act_id   = frg2.act_id + 1; // wrong action id

    mark_point();

    // ready for the first fragment
    gcs_defrag_init (&defrag);
    defrag_check_init (&defrag);

    mark_point();

    // 1. Try fragment that is not the first
    ret = gcs_defrag_handle_frag (&defrag, &frg3, &recv_act, FALSE);
    fail_if (ret != -EPROTO);
    mark_point();
    defrag_check_init (&defrag); // should be no changes

    // 2. Try first fragment
    ret = gcs_defrag_handle_frag (&defrag, &frg1, &recv_act, FALSE);
    fail_if (ret != 0);
    fail_if (defrag.head == NULL);
    fail_if (defrag.received != frag1_len);
    fail_if (defrag.tail != defrag.head + defrag.received);
    tail = defrag.tail;

#define TRY_WRONG_2ND_FRAGMENT(frag)                                \
    ret = gcs_defrag_handle_frag (&defrag, frag, &recv_act, FALSE); \
    fail_if (ret != -EPROTO);                                       \
    fail_if (defrag.received != frag1_len);                         \
    fail_if (defrag.tail != tail);

    // 3. Try first fragment again
    TRY_WRONG_2ND_FRAGMENT(&frg1);

    // 4. Try third fragment
    TRY_WRONG_2ND_FRAGMENT(&frg3);

    // 5. Try fouth fragment
    TRY_WRONG_2ND_FRAGMENT(&frg4);

    // 6. Try second fragment
    ret = gcs_defrag_handle_frag (&defrag, &frg2, &recv_act, FALSE);
    fail_if (ret != 0);
    fail_if (defrag.received != frag1_len + frag2_len);
    fail_if (defrag.tail != defrag.head + defrag.received);

    // 7. Try third fragment, last one
    ret = gcs_defrag_handle_frag (&defrag, &frg3, &recv_act, FALSE);
    fail_if (ret != act_len);

    // 8. Check the action
    fail_if (recv_act.buf_len != act_len);
    fail_if (strncmp(recv_act.buf, act_buf, act_len),
             "Action received: '%s', expected '%s'", recv_act.buf, act_buf);
    defrag_check_init (&defrag); // should be empty

    // 9. Try the same with local action
    ret = gcs_defrag_handle_frag (&defrag, &frg1, &recv_act, TRUE);
    fail_if (ret != 0);
    fail_if (defrag.head != NULL);
    ret = gcs_defrag_handle_frag (&defrag, &frg2, &recv_act, TRUE);
    fail_if (ret != 0);
    fail_if (defrag.head != NULL);
    ret = gcs_defrag_handle_frag (&defrag, &frg3, &recv_act, TRUE);
    fail_if (ret != act_len);
    fail_if (defrag.head != NULL);

    // 10. Check the action
    fail_if (recv_act.buf_len != act_len);
    fail_if (recv_act.buf != NULL);
    defrag_check_init (&defrag); // should be empty
}
END_TEST

Suite *gcs_defrag_suite(void)
{
  Suite *suite = suite_create("GCS defragmenter");
  TCase *tcase = tcase_create("gcs_defrag");

  suite_add_tcase (suite, tcase);
  tcase_add_test  (tcase, gcs_defrag_test);
  return suite;
}

