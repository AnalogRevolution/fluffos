/*
 * vm.cc
 *
 *  Created on: Nov 16, 2014
 *      Author: sunyc
 */

#include "base/std.h"

#include <cstdlib>

#include "vm/internal/eval.h"
#include "vm/internal/master.h"
#include "vm/internal/simul_efun.h"
#include "vm/internal/posix_timers.h"
#include "vm/internal/otable.h"             // for init_otable
#include "vm/internal/base/machine.h"       // for reset_machine
#include "vm/internal/compiler/lex.h"       // for add_predefines, fixme!
#include "vm/internal/compiler/compiler.h"  // for init_locals, fixme!

#ifdef PACKAGE_MUDLIB_STATS
#include "packages/mudlib_stats/mudlib_stats.h"
#endif

void preload_objects();

void vm_init() {
  init_strings();     /* in stralloc.c */
  init_identifiers(); /* in lex.c */
  init_locals();      /* in compiler.c */
  init_otable();      /* in otable.c */

  max_cost = CONFIG_INT(__MAX_EVAL_COST__);
  set_inc_list(CONFIG_STR(__INCLUDE_DIRS__));

  add_predefines();
  reset_machine(1);

  error_context_t econ;
  save_context(&econ);
  try {
    init_simul_efun(CONFIG_STR(__SIMUL_EFUN_FILE__));
    init_master(CONFIG_STR(__MASTER_FILE__));
  } catch (const char *) {
    debug_message("The simul_efun (%s) and master (%s) objects must be loadable.\n",
                  CONFIG_STR(__SIMUL_EFUN_FILE__), CONFIG_STR(__MASTER_FILE__));
    exit(-1);
  }
  pop_context(&econ);

  // init posix timers
  init_posix_timers();

#ifdef PACKAGE_MUDLIB_STATS
  restore_stat_files();
#endif

  preload_objects();
}

/* The epilog() in master.c is supposed to return an array of files to load.
 * The preload() in master object called to do the actual loading.
 */
void preload_objects() {
  // Legacy: epilog() has a int param to make it to avoid load anything.
  // I'm not sure who would use that.
  push_number(0);
  auto ret = safe_apply_master_ob(APPLY_EPILOG, 1);

  if ((ret == 0) || (ret == (svalue_t *)-1) || (ret->type != T_ARRAY)) {
    return;
  }

  auto prefiles = ret->u.arr;
  if ((prefiles == nullptr) || (prefiles->size < 1)) {
    return;
  }

  // prefiles (the global apply return value) would have been freed on next apply call.
  // so we have to increase ref here to make sure it is around.
  prefiles->ref++;

  debug_message("\nLoading preload files ...\n");

  for (int i = 0; i < prefiles->size; i++) {
    if (prefiles->item[i].type != T_STRING) {
      continue;
    }
    push_svalue(&prefiles->item[i]);
    debug_message("%s...\n", prefiles->item[i].u.string);
    set_eval(max_cost);
    safe_apply_master_ob(APPLY_PRELOAD, 1);
  }
  free_array(prefiles);
} /* preload_objects() */