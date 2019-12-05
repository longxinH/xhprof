/*
 *  Copyright (c) 2009 Facebook
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef PHP_XHPROF_H
#define PHP_XHPROF_H

extern zend_module_entry xhprof_module_entry;
#define phpext_xhprof_ptr &xhprof_module_entry

#ifdef PHP_WIN32
#define PHP_XHPROF_API __declspec(dllexport)
#else
#define PHP_XHPROF_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif


/**
 * **********************
 * GLOBAL MACRO CONSTANTS
 * **********************
 */

/* XHProf version                           */
#define XHPROF_VERSION       "2.1.2-dev"

/* Fictitious function name to represent top of the call tree. The paranthesis
 * in the name is to ensure we don't conflict with user function names.  */
#define ROOT_SYMBOL                "main()"

/* Size of a temp scratch buffer            */
#define SCRATCH_BUF_LEN            512

/* Various XHPROF modes. If you are adding a new mode, register the appropriate
 * callbacks in hp_begin() */
#define XHPROF_MODE_HIERARCHICAL            1
#define XHPROF_MODE_SAMPLED            620002      /* Rockfort's zip code */

/* Hierarchical profiling flags.
 *
 * Note: Function call counts and wall (elapsed) time are always profiled.
 * The following optional flags can be used to control other aspects of
 * profiling.
 */
#define XHPROF_FLAGS_NO_BUILTINS   0x0001         /* do not profile builtins */
#define XHPROF_FLAGS_CPU           0x0002      /* gather CPU times for funcs */
#define XHPROF_FLAGS_MEMORY        0x0004   /* gather memory usage for funcs */

/* Constants for XHPROF_MODE_SAMPLED        */
#define XHPROF_DEFAULT_SAMPLING_INTERVAL       100000      /* In microsecs        */
#define XHPROF_MINIMAL_SAMPLING_INTERVAL          100      /* In microsecs        */

/* Constant for ignoring functions, transparent to hierarchical profile */
#define XHPROF_MAX_IGNORED_FUNCTIONS  256
#define XHPROF_IGNORED_FUNCTION_FILTER_SIZE                           \
               ((XHPROF_MAX_IGNORED_FUNCTIONS + 7)/8)

#if !defined(uint64)
    typedef unsigned long long uint64;
#endif

#if !defined(uint32)
    typedef unsigned int uint32;
#endif

#if !defined(uint8)
    typedef unsigned char uint8;
#endif

/*
 * Start profiling - called just before calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define BEGIN_PROFILING(entries, symbol, profile_curr, execute_data)        \
do {                                                                     \
    /* Use a hash code to filter most of the string comparisons. */     \
    uint8 hash_code  = hp_inline_hash(symbol);                          \
    profile_curr = !hp_ignore_entry_work(hash_code, symbol);                 \
    if (profile_curr) {                                                 \
        if (execute_data != NULL) {                                     \
            symbol = hp_get_trace_callback(symbol, execute_data); \
        }                                                               \
        hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();            \
        (cur_entry)->hash_code = hash_code;                             \
        (cur_entry)->name_hprof = symbol;                               \
        (cur_entry)->prev_hprof = (*(entries));                         \
        /* Call the universal callback */                               \
        hp_mode_common_beginfn((entries), (cur_entry));                 \
        /* Call the mode's beginfn callback */                          \
        XHPROF_G(mode_cb).begin_fn_cb((entries), (cur_entry));         \
        /* Update entries linked list */                                \
        (*(entries)) = (cur_entry);                                     \
    }                                                               \
} while (0)

/*
 * Stop profiling - called just after calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define END_PROFILING(entries, profile_curr)                            \
do {                                                                    \
    if (profile_curr) {                                                 \
        hp_entry_t *cur_entry;                                          \
        /* Call the mode's endfn callback. */                           \
        /* NOTE(cjiang): we want to call this 'end_fn_cb' before */     \
        /* 'hp_mode_common_endfn' to avoid including the time in */     \
        /* 'hp_mode_common_endfn' in the profiling results.      */     \
        XHPROF_G(mode_cb).end_fn_cb((entries));                        \
        cur_entry = (*(entries));                                       \
        /* Free top entry and update entries linked list */             \
        (*(entries)) = (*(entries))->prev_hprof;                        \
        hp_fast_free_hprof_entry(cur_entry);                            \
    }                                                                   \
} while (0)

/* Bloom filter for function names to be ignored */
#define INDEX_2_BYTE(index)  (index >> 3)
#define INDEX_2_BIT(index)   (1 << (index & 0x7));

#define register_trace_callback(function_name, cb) zend_hash_str_update_mem(XHPROF_G(trace_callbacks), function_name, sizeof(function_name) - 1, &cb, sizeof(hp_trace_callback));

/* XHProf maintains a stack of entries being profiled. The memory for the entry
 * is passed by the layer that invokes BEGIN_PROFILING(), e.g. the hp_execute()
 * function. Often, this is just C-stack memory.
 *
 * This structure is a convenient place to track start time of a particular
 * profile operation, recursion depth, and the name of the function being
 * profiled. */
typedef struct hp_entry_t {
    char                   *name_hprof;                       /* function name */
    int                     rlvl_hprof;        /* recursion level for function */
    uint64                  tsc_start;         /* start value for TSC counter  */
    uint64                  cpu_start;
    long int                mu_start_hprof;                    /* memory usage */
    long int                pmu_start_hprof;              /* peak memory usage */
    struct hp_entry_t      *prev_hprof;    /* ptr to prev entry being profiled */
    uint8                   hash_code;     /* hash_code for the function name  */
} hp_entry_t;

typedef struct hp_ignored_functions {
    char **names;
    uint8 filter[XHPROF_MAX_IGNORED_FUNCTIONS];
} hp_ignored_functions;

typedef char* (*hp_trace_callback) (char *symbol, zend_execute_data *data);

/* Various types for XHPROF callbacks       */
typedef void (*hp_init_cb)           ();
typedef void (*hp_exit_cb)           ();
typedef void (*hp_begin_function_cb) (hp_entry_t **entries, hp_entry_t *current);
typedef void (*hp_end_function_cb)   (hp_entry_t **entries);

/**
 * ***********************
 * GLOBAL STATIC VARIABLES
 * ***********************
 */
/* Pointer to the original execute function */
static void (*_zend_execute_ex) (zend_execute_data *execute_data);
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data);

/* Pointer to the origianl execute_internal function */
static void (*_zend_execute_internal) (zend_execute_data *data, zval *return_value);
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, zval *return_value);

/* Pointer to the original compile function */
static zend_op_array * (*_zend_compile_file) (zend_file_handle *file_handle, int type);
ZEND_DLEXPORT zend_op_array* hp_compile_file(zend_file_handle *file_handle, int type);

/* Pointer to the original compile string function (used by eval) */
static zend_op_array * (*_zend_compile_string) (zval *source_string, char *filename);
ZEND_DLEXPORT zend_op_array* hp_compile_string(zval *source_string, char *filename);

/**
 * ****************************
 * STATIC FUNCTION DECLARATIONS
 * ****************************
 */
static void hp_register_constants(INIT_FUNC_ARGS);

static void hp_begin(long level, long xhprof_flags);
static void hp_stop();
static void hp_end();

static inline uint64 cycle_timer();

static void hp_free_the_free_list();
static hp_entry_t *hp_fast_alloc_hprof_entry();
static void hp_fast_free_hprof_entry(hp_entry_t *p);
static inline uint8 hp_inline_hash(char *str);
static void incr_us_interval(struct timeval *start, uint64 incr);

static void hp_get_ignored_functions_from_arg(zval *args);

static inline zval *hp_zval_at_key(char *key, zval *values);
static inline char **hp_strings_in_zval(zval *values);
static inline void hp_array_del(char **name_array);

char *hp_get_trace_callback(char *symbol, zend_execute_data *data);
void hp_init_trace_callbacks();

double get_timebase_conversion();

hp_ignored_functions *hp_ignored_functions_init(char **names);

/* Struct to hold the various callbacks for a single xhprof mode */
typedef struct hp_mode_cb {
    hp_init_cb             init_cb;
    hp_exit_cb             exit_cb;
    hp_begin_function_cb   begin_fn_cb;
    hp_end_function_cb     end_fn_cb;
} hp_mode_cb;

/* Xhprof's global state.
 *
 * This structure is instantiated once.  Initialize defaults for attributes in
 * hp_init_profiler_state() Cleanup/free attributes in
 * hp_clean_profiler_state() */
ZEND_BEGIN_MODULE_GLOBALS(xhprof)

    /*       ----------   Global attributes:  -----------       */

    /* Indicates if xhprof is currently enabled */
    int              enabled;

    /* Indicates if xhprof was ever enabled during this request */
    int              ever_enabled;

    /* Holds all the xhprof statistics */
    zval            stats_count;

    /* Indicates the current xhprof mode or level */
    int              profiler_level;

    /* Top of the profile stack */
    hp_entry_t      *entries;

    /* freelist of hp_entry_t chunks for reuse... */
    hp_entry_t      *entry_free_list;

    /* Callbacks for various xhprof modes */
    hp_mode_cb       mode_cb;

    /*       ----------   Mode specific attributes:  -----------       */

    /* Global to track the time of the last sample in time and ticks */
    struct timeval   last_sample_time;
    uint64           last_sample_tsc;
    /* XHPROF_SAMPLING_INTERVAL in ticks */
    long             sampling_interval;
    uint64           sampling_interval_tsc;
    int              sampling_depth;
    /* XHProf flags */
    uint32 xhprof_flags;

    char *root;

    /* counter table indexed by hash value of function names. */
    uint8  func_hash_counters[256];

    HashTable *trace_callbacks;

    /* Table of ignored function names and their filter */
    hp_ignored_functions *ignored_functions;

    double timebase_conversion;

ZEND_END_MODULE_GLOBALS(xhprof)

PHP_MINIT_FUNCTION(xhprof);
PHP_MSHUTDOWN_FUNCTION(xhprof);
PHP_RINIT_FUNCTION(xhprof);
PHP_RSHUTDOWN_FUNCTION(xhprof);
PHP_MINFO_FUNCTION(xhprof);

PHP_FUNCTION(xhprof_enable);
PHP_FUNCTION(xhprof_disable);
PHP_FUNCTION(xhprof_sample_enable);
PHP_FUNCTION(xhprof_sample_disable);

#ifdef ZTS
#define XHPROF_G(v) TSRMG(xhprof_globals_id, zend_xhprof_globals *, v)
#else
#define XHPROF_G(v) (xhprof_globals.v)
#endif

extern ZEND_DECLARE_MODULE_GLOBALS(xhprof);

#endif /* PHP_XHPROF_H */
