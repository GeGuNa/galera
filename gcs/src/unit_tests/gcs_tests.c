/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#include <stdio.h>  // printf()
#include <string.h> // strcmp()
#include <stdlib.h> // EXIT_SUCCESS | EXIT_FAILURE
#include <check.h>

#include <galerautils.h>
#include "gcs_comp_test.h"
#include "gcs_state_test.h"
#include "gcs_fifo_test.h"
#include "gcs_proto_test.h"
#include "gcs_defrag_test.h"
#include "gcs_node_test.h"
#include "gcs_group_test.h"
#include "gcs_backend_test.h"

typedef Suite *(*suite_creator_t)(void);

static suite_creator_t suites[] =
    {
	gcs_comp_suite,
	gcs_state_suite,
	gcs_fifo_suite,
	gcs_proto_suite,
	gcs_defrag_suite,
	gcs_node_suite,
	gcs_group_suite,
	gcs_backend_suite,
	NULL
    };

int main(int argc, char* argv[])
{
  int no_fork = ((argc > 1) && !strcmp(argv[1], "nofork")) ? 1 : 0;
  int i       = 0;
  int failed  = 0;

  FILE* log_file = NULL;

  log_file = fopen ("gcs_tests.log", "w");
  if (!log_file) return EXIT_FAILURE;
  gu_conf_set_log_file (log_file);
  gu_conf_debug_on();

  while (suites[i]) {
      SRunner* sr = srunner_create(suites[i]());

      gu_info ("###########################################");
      gu_info ("Test %d.", i);
      gu_info ("###########################################");
      if (no_fork) srunner_set_fork_status(sr, CK_NOFORK);
      srunner_run_all (sr, CK_NORMAL);
      failed += srunner_ntests_failed (sr);
      srunner_free (sr);
      i++;
  }

  fclose (log_file);
  printf ("Total test failed: %d\n", failed);
  return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
