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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_xhprof.h"
#include "zend_extensions.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <unistd.h>

#if HAVE_PCRE
#include "ext/pcre/php_pcre.h"
#endif

#if __APPLE__
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#endif

ZEND_DECLARE_MODULE_GLOBALS(xhprof)

/**
 * ****************************
 * STATIC FUNCTION DECLARATIONS
 * ****************************
 */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_xhprof_enable, 0, 0, 0)
  ZEND_ARG_INFO(0, flags)
  ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_xhprof_disable, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_xhprof_sample_enable, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_xhprof_sample_disable, 0)
ZEND_END_ARG_INFO()
/* }}} */

/**
 * *********************
 * PHP EXTENSION GLOBALS
 * *********************
 */
/* List of functions implemented/exposed by xhprof */
zend_function_entry xhprof_functions[] = {
  PHP_FE(xhprof_enable, arginfo_xhprof_enable)
  PHP_FE(xhprof_disable, arginfo_xhprof_disable)
  PHP_FE(xhprof_sample_enable, arginfo_xhprof_sample_enable)
  PHP_FE(xhprof_sample_disable, arginfo_xhprof_sample_disable)
  {NULL, NULL, NULL}
};

/* Callback functions for the xhprof extension */
zend_module_entry xhprof_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
        STANDARD_MODULE_HEADER,
#endif
        "xhprof",                        /* Name of the extension */
        xhprof_functions,                /* List of functions exposed */
        PHP_MINIT(xhprof),               /* Module init callback */
        PHP_MSHUTDOWN(xhprof),           /* Module shutdown callback */
        PHP_RINIT(xhprof),               /* Request init callback */
        PHP_RSHUTDOWN(xhprof),           /* Request shutdown callback */
        PHP_MINFO(xhprof),               /* Module info callback */
#if ZEND_MODULE_API_NO >= 20010901
        XHPROF_VERSION,
#endif
        STANDARD_MODULE_PROPERTIES
};

PHP_INI_BEGIN()

/* output directory:
 * Currently this is not used by the extension itself.
 * But some implementations of iXHProfRuns interface might
 * choose to save/restore XHProf profiler runs in the
 * directory specified by this ini setting.
 */
PHP_INI_ENTRY("xhprof.output_dir", "", PHP_INI_ALL, NULL)

/*
 * collect_additional_info
 * Collect mysql_query, curl_exec internal info. The default is 0.
 */
PHP_INI_ENTRY("xhprof.collect_additional_info", "0", PHP_INI_ALL, NULL)

/* sampling_interval:
 * Sampling interval to be used by the sampling profiler, in microseconds.
 */
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

STD_PHP_INI_ENTRY("xhprof.sampling_interval", STRINGIFY(XHPROF_DEFAULT_SAMPLING_INTERVAL), PHP_INI_ALL, OnUpdateLong, sampling_interval, zend_xhprof_globals, xhprof_globals)

/* sampling_depth:
 * Depth to trace call-chain by the sampling profiler
 */
STD_PHP_INI_ENTRY("xhprof.sampling_depth", STRINGIFY(INT_MAX), PHP_INI_ALL, OnUpdateLong, sampling_depth, zend_xhprof_globals, xhprof_globals)
PHP_INI_END()

/* Init module */
#ifdef COMPILE_DL_XHPROF
    ZEND_GET_MODULE(xhprof)
#endif

#ifdef ZTS
    ZEND_TSRMLS_CACHE_DEFINE();
#endif

/**
 * **********************************
 * PHP EXTENSION FUNCTION DEFINITIONS
 * **********************************
 */

/**
 * Start XHProf profiling in hierarchical mode.
 *
 * @param  long $flags  flags for hierarchical mode
 * @return void
 * @author kannan
 */
PHP_FUNCTION(xhprof_enable)
{
    long  xhprof_flags = 0;              /* XHProf flags */
    zval *optional_array = NULL;         /* optional array arg: for future use */

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|lz", &xhprof_flags, &optional_array) == FAILURE) {
        return;
    }

    hp_get_ignored_functions_from_arg(optional_array);

    hp_begin(XHPROF_MODE_HIERARCHICAL, xhprof_flags);
}

/**
 * Stops XHProf from profiling in hierarchical mode anymore and returns the
 * profile info.
 *
 * @param  void
 * @return array  hash-array of XHProf's profile info
 * @author kannan, hzhao
 */
PHP_FUNCTION(xhprof_disable)
{
    if (XHPROF_G(enabled)) {
        hp_stop();
        RETURN_ZVAL(&XHPROF_G(stats_count), 1, 0);
    }
    /* else null is returned */
}

/**
 * Start XHProf profiling in sampling mode.
 *
 * @return void
 * @author cjiang
 */
PHP_FUNCTION(xhprof_sample_enable)
{
    long xhprof_flags = 0;    /* XHProf flags */
    hp_get_ignored_functions_from_arg(NULL);
    hp_begin(XHPROF_MODE_SAMPLED, xhprof_flags);
}

/**
 * Stops XHProf from profiling in sampling mode anymore and returns the profile
 * info.
 *
 * @param  void
 * @return array  hash-array of XHProf's profile info
 * @author cjiang
 */
PHP_FUNCTION(xhprof_sample_disable)
{
    if (XHPROF_G(enabled)) {
        hp_stop();
        RETURN_ZVAL(&XHPROF_G(stats_count), 1, 0);
    }
  /* else null is returned */
}

static void php_xhprof_init_globals(zend_xhprof_globals *xhprof_globals)
{
    xhprof_globals->enabled = 0;
    xhprof_globals->ever_enabled = 0;
    xhprof_globals->xhprof_flags = 0;
    xhprof_globals->entries = NULL;
    xhprof_globals->root = NULL;
    xhprof_globals->trace_callbacks = NULL;
    xhprof_globals->ignored_functions = NULL;
    xhprof_globals->sampling_interval = XHPROF_DEFAULT_SAMPLING_INTERVAL;
    xhprof_globals->sampling_depth = INT_MAX;

    ZVAL_UNDEF(&xhprof_globals->stats_count);

    /* no free hp_entry_t structures to start with */
    xhprof_globals->entry_free_list = NULL;

    int i;

    for (i = 0; i < 256; i++) {
        xhprof_globals->func_hash_counters[i] = 0;
    }

    if (xhprof_globals->sampling_interval < XHPROF_MINIMAL_SAMPLING_INTERVAL) {
        xhprof_globals->sampling_interval = XHPROF_MINIMAL_SAMPLING_INTERVAL;
    }
}

/**
 * Module init callback.
 *
 * @author cjiang
 */
PHP_MINIT_FUNCTION(xhprof)
{
    ZEND_INIT_MODULE_GLOBALS(xhprof, php_xhprof_init_globals, NULL);

    REGISTER_INI_ENTRIES();

    hp_register_constants(INIT_FUNC_ARGS_PASSTHRU);

    /* Replace zend_compile with our proxy */
    _zend_compile_file = zend_compile_file;
    zend_compile_file  = hp_compile_file;

    /* Replace zend_compile_string with our proxy */
    _zend_compile_string = zend_compile_string;
    zend_compile_string = hp_compile_string;

    /* Replace zend_execute with our proxy */
    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex  = hp_execute_ex;

    /* Replace zend_execute_internal with our proxy */
    _zend_execute_internal = zend_execute_internal;
    zend_execute_internal = hp_execute_internal;

#if defined(DEBUG)
    /* To make it random number generator repeatable to ease testing. */
    srand(0);
#endif
    return SUCCESS;
}

/**
 * Module shutdown callback.
 */
PHP_MSHUTDOWN_FUNCTION(xhprof)
{
    /* free any remaining items in the free list */
    hp_free_the_free_list();

    /* Remove proxies, restore the originals */
    zend_execute_ex       = _zend_execute_ex;
    zend_execute_internal = _zend_execute_internal;
    zend_compile_file     = _zend_compile_file;
    zend_compile_string   = _zend_compile_string;

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

/**
 * Request init callback. Nothing to do yet!
 */
PHP_RINIT_FUNCTION(xhprof)
{
#if defined(ZTS) && defined(COMPILE_DL_XHPROF)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    XHPROF_G(timebase_conversion) = get_timebase_conversion();

    return SUCCESS;
}

/**
 * Request shutdown callback. Stop profiling and return.
 */
PHP_RSHUTDOWN_FUNCTION(xhprof)
{
    hp_end();
    return SUCCESS;
}

/**
 * Module info callback. Returns the xhprof version.
 */
PHP_MINFO_FUNCTION(xhprof)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "xhprof support", "enabled");
    php_info_print_table_row(2, "Version", XHPROF_VERSION);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}


/**
 * ***************************************************
 * COMMON HELPER FUNCTION DEFINITIONS AND LOCAL MACROS
 * ***************************************************
 */

static void hp_register_constants(INIT_FUNC_ARGS)
{
    REGISTER_LONG_CONSTANT("XHPROF_FLAGS_NO_BUILTINS",
                         XHPROF_FLAGS_NO_BUILTINS,
                         CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("XHPROF_FLAGS_CPU",
                         XHPROF_FLAGS_CPU,
                         CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("XHPROF_FLAGS_MEMORY",
                         XHPROF_FLAGS_MEMORY,
                         CONST_CS | CONST_PERSISTENT);
}

/**
 * A hash function to calculate a 8-bit hash code for a function name.
 * This is based on a small modification to 'zend_inline_hash_func' by summing
 * up all bytes of the ulong returned by 'zend_inline_hash_func'.
 *
 * @param str, char *, string to be calculated hash code for.
 *
 * @author cjiang
 */
static inline uint8 hp_inline_hash(char * str)
{
    ulong h = 5381;
    uint i = 0;
    uint8 res = 0;

    while (*str) {
        h += (h << 5);
        h ^= (ulong) *str++;
    }

    for (i = 0; i < sizeof(ulong); i++) {
        res += ((uint8 *)&h)[i];
    }

    return res;
}

/**
 * Parse the list of ignored functions from the zval argument.
 *
 * @author mpal
 */
void hp_get_ignored_functions_from_arg(zval *args)
{
    if (args == NULL) {
        return;
    }

    zval *pzval;
    pzval = hp_zval_at_key("ignored_functions", args);
    XHPROF_G(ignored_functions) = hp_ignored_functions_init(hp_strings_in_zval(pzval));
}

void hp_ignored_functions_clear(hp_ignored_functions *functions)
{
    if (functions == NULL) {
        return;
    }

    hp_array_del(functions->names);
    functions->names = NULL;

    memset(functions->filter, 0, XHPROF_IGNORED_FUNCTION_FILTER_SIZE);
    efree(functions);
}

double get_timebase_conversion()
{
#if defined(__APPLE__)
    mach_timebase_info_data_t info;
    (void) mach_timebase_info(&info);

    return (info.numer / info.denom) * 1000 * 1000;
#endif

    return 1.0;
}

hp_ignored_functions *hp_ignored_functions_init(char **names)
{
    /* Delete the array storing ignored function names */
    hp_ignored_functions_clear(XHPROF_G(ignored_functions));

    if (names == NULL) {
        return NULL;
    }

    hp_ignored_functions *functions;

    functions = emalloc(sizeof(hp_ignored_functions));
    functions->names = names;

    memset(functions->filter, 0, XHPROF_IGNORED_FUNCTION_FILTER_SIZE);

    int i = 0;
    for(; names[i] != NULL; i++) {
        char *str  = names[i];
        uint8 hash = hp_inline_hash(str);
        int   idx  = INDEX_2_BYTE(hash);
        functions->filter[idx] |= INDEX_2_BIT(hash);
    }

    return functions;
}

/**
 * Check if function collides in filter of functions to be ignored.
 *
 * @author mpal
 */
int hp_ignored_functions_filter_collision(hp_ignored_functions *functions, uint8 hash)
{
    uint8 mask = INDEX_2_BIT(hash);
    return functions->filter[INDEX_2_BYTE(hash)] & mask;
}

/**
 * Initialize profiler state
 *
 * @author kannan, veeve
 */
void hp_init_profiler_state(int level)
{
    /* Setup globals */
    if (!XHPROF_G(ever_enabled)) {
        XHPROF_G(ever_enabled) = 1;
        XHPROF_G(entries) = NULL;
    }

    XHPROF_G(profiler_level) = (int)level;

    /* Init stats_count */
    if (Z_TYPE(XHPROF_G(stats_count)) != IS_UNDEF) {
        zval_ptr_dtor(&XHPROF_G(stats_count));
    }

    array_init(&XHPROF_G(stats_count));

    hp_init_trace_callbacks();

    /* Call current mode's init cb */
    XHPROF_G(mode_cb).init_cb();
}

/**
 * Cleanup profiler state
 *
 * @author kannan, veeve
 */
void hp_clean_profiler_state()
{
    /* Call current mode's exit cb */
    XHPROF_G(mode_cb).exit_cb();

    /* Clear globals */
    if (Z_TYPE(XHPROF_G(stats_count)) != IS_UNDEF) {
        zval_ptr_dtor(&XHPROF_G(stats_count));
    }

    ZVAL_UNDEF(&XHPROF_G(stats_count));

    XHPROF_G(entries) = NULL;
    XHPROF_G(profiler_level) = 1;
    XHPROF_G(ever_enabled) = 0;

    if (XHPROF_G(trace_callbacks)) {
        zend_hash_destroy(XHPROF_G(trace_callbacks));
        FREE_HASHTABLE(XHPROF_G(trace_callbacks));
        XHPROF_G(trace_callbacks) = NULL;
    }

    /* Delete the array storing ignored function names */
    hp_ignored_functions_clear(XHPROF_G(ignored_functions));
    XHPROF_G(ignored_functions) = NULL;
}

/**
 * Returns formatted function name
 *
 * @param  entry        hp_entry
 * @author veeve
 */
size_t hp_get_entry_name(hp_entry_t *entry, char *result_buf, size_t result_len)
{
    size_t len;

    /* Add '@recurse_level' if required */
    /* NOTE:  Dont use snprintf's return val as it is compiler dependent */
    if (entry->rlvl_hprof) {
        len = snprintf(result_buf, result_len, "%s@%d", entry->name_hprof, entry->rlvl_hprof);
    } else {
        len = snprintf(result_buf, result_len, "%s", entry->name_hprof);
    }

    return len;
}

/**
 * Check if this entry should be ignored, first with a conservative Bloomish
 * filter then with an exact check against the function names.
 *
 * @author mpal
 */
int hp_ignore_entry_work(uint8 hash_code, char *curr_func)
{
    if (XHPROF_G(ignored_functions) == NULL) {
        return 0;
    }

    hp_ignored_functions *functions = XHPROF_G(ignored_functions);

    if (hp_ignored_functions_filter_collision(functions, hash_code)) {
        int i = 0;
        for (; functions->names[i] != NULL; i++) {
            char *name = functions->names[i];
            if (strcmp(curr_func, name) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Build a caller qualified name for a callee.
 *
 * For example, if A() is caller for B(), then it returns "A==>B".
 * Recursive invokations are denoted with @<n> where n is the recursion
 * depth.
 *
 * For example, "foo==>foo@1", and "foo@2==>foo@3" are examples of direct
 * recursion. And  "bar==>foo@1" is an example of an indirect recursive
 * call to foo (implying the foo() is on the call stack some levels
 * above).
 *
 * @author kannan, veeve
 */
size_t hp_get_function_stack(hp_entry_t *entry, int level, char *result_buf, size_t result_len)
{
    size_t len = 0;

    /* End recursion if we dont need deeper levels or we dont have any deeper
    * levels */
    if (!entry->prev_hprof || (level <= 1)) {
        return hp_get_entry_name(entry, result_buf, result_len);
    }

    /* Take care of all ancestors first */
    len = hp_get_function_stack(entry->prev_hprof, level - 1, result_buf, result_len);

    /* Append the delimiter */
# define    HP_STACK_DELIM        "==>"
# define    HP_STACK_DELIM_LEN    (sizeof(HP_STACK_DELIM) - 1)

    if (result_len < (len + HP_STACK_DELIM_LEN)) {
        /* Insufficient result_buf. Bail out! */
        return len;
    }

    /* Add delimiter only if entry had ancestors */
    if (len) {
        strncat(result_buf + len, HP_STACK_DELIM, result_len - len);
        len += HP_STACK_DELIM_LEN;
    }

# undef     HP_STACK_DELIM_LEN
# undef     HP_STACK_DELIM

    /* Append the current function name */
    return len + hp_get_entry_name(entry, result_buf + len, result_len - len);
}

/**
 * Takes an input of the form /a/b/c/d/foo.php and returns
 * a pointer to one-level directory and basefile name
 * (d/foo.php) in the same string.
 */
static const char *hp_get_base_filename(const char *filename)
{
    const char *ptr;
    int   found = 0;

    if (!filename)
        return "";

    /* reverse search for "/" and return a ptr to the next char */
    for (ptr = filename + strlen(filename) - 1; ptr >= filename; ptr--) {
        if (*ptr == '/') {
            found++;
        }

        if (found == 2) {
            return ptr + 1;
        }
    }

    /* no "/" char found, so return the whole string */
    return filename;
}

/**
 * Get the name of the current function. The name is qualified with
 * the class name if the function is in a class.
 *
 * @author kannan, hzhao
 */
static char *hp_get_function_name(zend_execute_data *execute_data)
{
    const char        *cls = NULL;
    char              *ret;
    zend_function     *curr_func;
    zend_string       *func = NULL;

    if (!execute_data) {
        return NULL;
    }

    /* shared meta data for function on the call stack */
    curr_func = execute_data->func;
    /* extract function name from the meta info */
    func = curr_func->common.function_name;

    if (!func) {
        return NULL;
    }

    if (curr_func->common.scope != NULL) {
        char *sep = "::";
        cls = curr_func->common.scope->name->val;
        spprintf(&ret, 0, "%s%s%s", cls, sep, func->val);
    } else {
        spprintf(&ret, 0, "%s", ZSTR_VAL(func));
    }

    return ret;
}

/**
 * Free any items in the free list.
 */
static void hp_free_the_free_list()
{
    hp_entry_t *p = XHPROF_G(entry_free_list);
    hp_entry_t *cur;

    while (p) {
        cur = p;
        p = p->prev_hprof;
        free(cur);
    }
}



/**
 * Fast allocate a hp_entry_t structure. Picks one from the
 * free list if available, else does an actual allocate.
 *
 * Doesn't bother initializing allocated memory.
 *
 * @author kannan
 */
static hp_entry_t *hp_fast_alloc_hprof_entry()
{
    hp_entry_t *p;

    p = XHPROF_G(entry_free_list);

    if (p) {
        XHPROF_G(entry_free_list) = p->prev_hprof;
        return p;
    } else {
        return (hp_entry_t *) malloc(sizeof(hp_entry_t));
    }
}

/**
 * Fast free a hp_entry_t structure. Simply returns back
 * the hp_entry_t to a free list and doesn't actually
 * perform the free.
 *
 * @author kannan
 */
static void hp_fast_free_hprof_entry(hp_entry_t *p)
{
    /* we use/overload the prev_hprof field in the structure to link entries in
     * the free list.
     * */
    p->prev_hprof = XHPROF_G(entry_free_list);
    XHPROF_G(entry_free_list) = p;
}

/**
 * Increment the count of the given stat with the given count
 * If the stat was not set before, inits the stat to the given count
 *
 * @param  zval *counts   Zend hash table pointer
 * @param  char *name     Name of the stat
 * @param  long  count    Value of the stat to incr by
 * @return void
 * @author kannan
 */
void hp_inc_count(zval *counts, char *name, long count)
{
    HashTable *ht;
    zval *data, val;

    if (!counts) {
        return;
    }

    ht = HASH_OF(counts);

    if (!ht) {
        return;
    }

    data = zend_hash_str_find(ht, name, strlen(name));

    if (data) {
        ZVAL_LONG(data, Z_LVAL_P(data) + count);
    } else {
        ZVAL_LONG(&val, count);
        zend_hash_str_update(ht, name, strlen(name), &val);
    }

}

/**
 * Truncates the given timeval to the nearest slot begin, where
 * the slot size is determined by intr
 *
 * @param  tv       Input timeval to be truncated in place
 * @param  intr     Time interval in microsecs - slot width
 * @return void
 * @author veeve
 */
void hp_trunc_time(struct timeval *tv, uint64 intr)
{
    uint64 time_in_micro;

    /* Convert to microsecs and trunc that first */
    time_in_micro = (tv->tv_sec * 1000000) + tv->tv_usec;
    time_in_micro /= intr;
    time_in_micro *= intr;

    /* Update tv */
    tv->tv_sec  = (time_in_micro / 1000000);
    tv->tv_usec = (time_in_micro % 1000000);
}

/**
 * Sample the stack. Add it to the stats_count global.
 *
 * @param  tv            current time
 * @param  entries       func stack as linked list of hp_entry_t
 * @return void
 * @author veeve
 */
void hp_sample_stack(hp_entry_t  **entries)
{
    char key[SCRATCH_BUF_LEN];
    char symbol[SCRATCH_BUF_LEN * 1000];

    /* Build key */
    snprintf(key, sizeof(key), "%d.%06d", XHPROF_G(last_sample_time).tv_sec, XHPROF_G(last_sample_time).tv_usec);

    /* Init stats in the global stats_count hashtable */
    hp_get_function_stack(*entries, XHPROF_G(sampling_depth), symbol, sizeof(symbol));

    add_assoc_string(&XHPROF_G(stats_count), key, symbol);
}

/**
 * Checks to see if it is time to sample the stack.
 * Calls hp_sample_stack() if its time.
 *
 * @param  entries        func stack as linked list of hp_entry_t
 * @param  last_sample    time the last sample was taken
 * @param  sampling_intr  sampling interval in microsecs
 * @return void
 * @author veeve
 */
void hp_sample_check(hp_entry_t **entries)
{
    /* Validate input */
    if (!entries || !(*entries)) {
        return;
    }

    /* See if its time to sample.  While loop is to handle a single function
    * taking a long time and passing several sampling intervals. */
    while ((cycle_timer() - XHPROF_G(last_sample_tsc)) > XHPROF_G(sampling_interval_tsc)) {
        /* bump last_sample_tsc */
        XHPROF_G(last_sample_tsc) += XHPROF_G(sampling_interval_tsc);

        /* bump last_sample_time - HAS TO BE UPDATED BEFORE calling hp_sample_stack */
        incr_us_interval(&XHPROF_G(last_sample_time), XHPROF_G(sampling_interval));

        /* sample the stack */
        hp_sample_stack(entries);
    }
}


/**
 * ***********************
 * High precision timer related functions.
 * ***********************
 */

static inline uint64 cycle_timer()
{
#if defined(__APPLE__) && defined(__MACH__)
    return mach_absolute_time() / XHPROF_G(timebase_conversion);
#else
    struct timespec s;
    clock_gettime(CLOCK_MONOTONIC, &s);

    return s.tv_sec * 1000 * 1000 + s.tv_nsec / 1000;
#endif
}

/**
 * Get the current real CPU clock timer
 */
static uint64 cpu_timer()
{
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec s;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s);

    return s.tv_sec * 1000 * 1000 + s.tv_nsec / 1000;
#else
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);

    return (ru.ru_utime.tv_sec  + ru.ru_stime.tv_sec ) * 1000 * 1000 + (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec);
#endif
}

/**
 * Incr time with the given microseconds.
 */
static void incr_us_interval(struct timeval *start, uint64 incr)
{
    incr += (start->tv_sec * 1000000 + start->tv_usec);
    start->tv_sec  = incr / 1000000;
    start->tv_usec = incr % 1000000;
}

/**
 * ***************************
 * XHPROF DUMMY CALLBACKS
 * ***************************
 */
void hp_mode_dummy_init_cb()
{

}

void hp_mode_dummy_exit_cb()
{

}

void hp_mode_dummy_beginfn_cb(hp_entry_t **entries, hp_entry_t *current)
{

}

void hp_mode_dummy_endfn_cb(hp_entry_t **entries)
{

}


/**
 * ****************************
 * XHPROF COMMON CALLBACKS
 * ****************************
 */
/**
 * XHPROF universal begin function.
 * This function is called for all modes before the
 * mode's specific begin_function callback is called.
 *
 * @param  hp_entry_t **entries  linked list (stack)
 *                                  of hprof entries
 * @param  hp_entry_t  *current  hprof entry for the current fn
 * @return void
 * @author kannan, veeve
 */
void hp_mode_common_beginfn(hp_entry_t **entries, hp_entry_t *current)
{
    hp_entry_t *p;

    /* This symbol's recursive level */
    int recurse_level = 0;

    if (XHPROF_G(func_hash_counters[current->hash_code]) > 0) {
        /* Find this symbols recurse level */
        for (p = (*entries); p; p = p->prev_hprof) {
            if (!strcmp(current->name_hprof, p->name_hprof)) {
                recurse_level = (p->rlvl_hprof) + 1;
                break;
            }
        }
    }

    XHPROF_G(func_hash_counters[current->hash_code])++;

    /* Init current function's recurse level */
    current->rlvl_hprof = recurse_level;
}

/**
 * *********************************
 * XHPROF INIT MODULE CALLBACKS
 * *********************************
 */
/**
 * XHPROF_MODE_SAMPLED's init callback
 *
 * @author veeve
 */
void hp_mode_sampled_init_cb()
{
    /* Init the last_sample in tsc */
    XHPROF_G(last_sample_tsc) = cycle_timer();

    /* Find the microseconds that need to be truncated */
    gettimeofday(&XHPROF_G(last_sample_time), 0);
    hp_trunc_time(&XHPROF_G(last_sample_time), XHPROF_G(sampling_interval));

    /* Convert sampling interval to ticks */
    XHPROF_G(sampling_interval_tsc) = XHPROF_G(sampling_interval);
}


/**
 * ************************************
 * XHPROF BEGIN FUNCTION CALLBACKS
 * ************************************
 */

/**
 * XHPROF_MODE_HIERARCHICAL's begin function callback
 *
 * @author kannan
 */
void hp_mode_hier_beginfn_cb(hp_entry_t **entries, hp_entry_t  *current)
{
    /* Get start tsc counter */
    current->tsc_start = cycle_timer();

    /* Get CPU usage */
    if (XHPROF_G(xhprof_flags) & XHPROF_FLAGS_CPU) {
        current->cpu_start = cpu_timer();
    }

    /* Get memory usage */
    if (XHPROF_G(xhprof_flags) & XHPROF_FLAGS_MEMORY) {
        current->mu_start_hprof  = zend_memory_usage(0);
        current->pmu_start_hprof = zend_memory_peak_usage(0);
    }
}


/**
 * XHPROF_MODE_SAMPLED's begin function callback
 *
 * @author veeve
 */
void hp_mode_sampled_beginfn_cb(hp_entry_t **entries, hp_entry_t *current)
{
    /* See if its time to take a sample */
    hp_sample_check(entries);
}


/**
 * **********************************
 * XHPROF END FUNCTION CALLBACKS
 * **********************************
 */

/**
 * XHPROF_MODE_HIERARCHICAL's end function callback
 *
 * @author kannan
 */
void hp_mode_hier_endfn_cb(hp_entry_t **entries)
{
    hp_entry_t      *top = (*entries);
    zval            *counts;
    char            symbol[SCRATCH_BUF_LEN];
    long int        mu_end;
    long int        pmu_end;
    double          wt, cpu;

    /* Get end tsc counter */
    wt = cycle_timer() - top->tsc_start;

    /* Get the stat array */
    hp_get_function_stack(top, 2, symbol, sizeof(symbol));

    counts = zend_hash_str_find(Z_ARRVAL(XHPROF_G(stats_count)), symbol, strlen(symbol));

    if (counts == NULL) {
        zval count_val;
        array_init(&count_val);
        counts = zend_hash_str_update(Z_ARRVAL(XHPROF_G(stats_count)), symbol, strlen(symbol), &count_val);
    }

    /* Bump stats in the counts hashtable */
    hp_inc_count(counts, "ct", 1);
    hp_inc_count(counts, "wt", wt);

    if (XHPROF_G(xhprof_flags) & XHPROF_FLAGS_CPU) {
        cpu = cpu_timer() - top->cpu_start;

        /* Bump CPU stats in the counts hashtable */
        hp_inc_count(counts, "cpu", cpu);
    }

    if (XHPROF_G(xhprof_flags) & XHPROF_FLAGS_MEMORY) {
        /* Get Memory usage */
        mu_end  = zend_memory_usage(0);
        pmu_end = zend_memory_peak_usage(0);

        /* Bump Memory stats in the counts hashtable */
        hp_inc_count(counts, "mu",  mu_end - top->mu_start_hprof);
        hp_inc_count(counts, "pmu", pmu_end - top->pmu_start_hprof);
    }

    XHPROF_G(func_hash_counters[top->hash_code])--;
}

/**
 * XHPROF_MODE_SAMPLED's end function callback
 *
 * @author veeve
 */
void hp_mode_sampled_endfn_cb(hp_entry_t **entries)
{
    /* See if its time to take a sample */
    hp_sample_check(entries);
}


/**
 * ***************************
 * PHP EXECUTE/COMPILE PROXIES
 * ***************************
 */

/**
 * XHProf enable replaced the zend_execute function with this
 * new execute function. We can do whatever profiling we need to
 * before and after calling the actual zend_execute().
 *
 * @author hzhao, kannan
 */

ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data)
{
    if (!XHPROF_G(enabled)) {
        _zend_execute_ex(execute_data);
        return;
    }

    char *func = NULL;
    int hp_profile_flag = 1;

    func = hp_get_function_name(execute_data);

    if (!func) {
        _zend_execute_ex(execute_data);
        return;
    }

    zend_execute_data *real_execute_data = execute_data->prev_execute_data;

    BEGIN_PROFILING(&XHPROF_G(entries), func, hp_profile_flag, real_execute_data);

    _zend_execute_ex(execute_data);

    if (XHPROF_G(entries)) {
        END_PROFILING(&XHPROF_G(entries), hp_profile_flag);
    }

    efree(func);
}

/**
 * Very similar to hp_execute. Proxy for zend_execute_internal().
 * Applies to zend builtin functions.
 *
 * @author hzhao, kannan
 */

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, zval *return_value)
{
    if (!XHPROF_G(enabled) || (XHPROF_G(xhprof_flags) & XHPROF_FLAGS_NO_BUILTINS)) {
        execute_internal(execute_data, return_value);
        return;
    }

    char *func = NULL;
    int hp_profile_flag = 1;

    func = hp_get_function_name(execute_data);

    if (func) {
        BEGIN_PROFILING(&XHPROF_G(entries), func, hp_profile_flag, execute_data);
    }

    if (!_zend_execute_internal) {
        /* no old override to begin with. so invoke the builtin's implementation  */
        execute_internal(execute_data, return_value);
    } else {
        /* call the old override */
        _zend_execute_internal(execute_data, return_value);
    }

    if (func) {
        if (XHPROF_G(entries)) {
            END_PROFILING(&XHPROF_G(entries), hp_profile_flag);
        }
        efree(func);
    }

}

/**
 * Proxy for zend_compile_file(). Used to profile PHP compilation time.
 *
 * @author kannan, hzhao
 */
ZEND_DLEXPORT zend_op_array* hp_compile_file(zend_file_handle *file_handle, int type)
{
    if (!XHPROF_G(enabled)) {
        return _zend_compile_file(file_handle, type);
    }

    const char     *filename;
    char           *func;
    int            len;
    zend_op_array  *ret;
    int            hp_profile_flag = 1;

    filename = hp_get_base_filename(file_handle->filename);
    len      = strlen("load") + strlen(filename) + 3;
    func     = (char *)emalloc(len);
    snprintf(func, len, "load::%s", filename);

    BEGIN_PROFILING(&XHPROF_G(entries), func, hp_profile_flag, NULL);
    ret = _zend_compile_file(file_handle, type);

    if (XHPROF_G(entries)) {
        END_PROFILING(&XHPROF_G(entries), hp_profile_flag);
    }

    efree(func);
    return ret;
}

/**
 * Proxy for zend_compile_string(). Used to profile PHP eval compilation time.
 */
ZEND_DLEXPORT zend_op_array* hp_compile_string(zval *source_string, char *filename)
{
    if (!XHPROF_G(enabled)) {
        return _zend_compile_string(source_string, filename);
    }

    char          *func;
    int           len;
    zend_op_array *ret;
    int           hp_profile_flag = 1;

    len  = strlen("eval") + strlen(filename) + 3;
    func = (char *)emalloc(len);
    snprintf(func, len, "eval::%s", filename);

    BEGIN_PROFILING(&XHPROF_G(entries), func, hp_profile_flag, NULL);
    ret = _zend_compile_string(source_string, filename);

    if (XHPROF_G(entries)) {
        END_PROFILING(&XHPROF_G(entries), hp_profile_flag);
    }

    efree(func);
    return ret;
}

/**
 * **************************
 * MAIN XHPROF CALLBACKS
 * **************************
 */

/**
 * This function gets called once when xhprof gets enabled.
 * It replaces all the functions like zend_execute, zend_execute_internal,
 * etc that needs to be instrumented with their corresponding proxies.
 */
static void hp_begin(long level, long xhprof_flags)
{
    if (!XHPROF_G(enabled)) {
        int hp_profile_flag = 1;

        XHPROF_G(enabled)      = 1;
        XHPROF_G(xhprof_flags) = (uint32)xhprof_flags;

        /* Initialize with the dummy mode first Having these dummy callbacks saves
         * us from checking if any of the callbacks are NULL everywhere. */
        XHPROF_G(mode_cb).init_cb     = hp_mode_dummy_init_cb;
        XHPROF_G(mode_cb).exit_cb     = hp_mode_dummy_exit_cb;
        XHPROF_G(mode_cb).begin_fn_cb = hp_mode_dummy_beginfn_cb;
        XHPROF_G(mode_cb).end_fn_cb   = hp_mode_dummy_endfn_cb;

        /* Register the appropriate callback functions Override just a subset of
        * all the callbacks is OK. */
        switch (level) {
            case XHPROF_MODE_HIERARCHICAL:
                XHPROF_G(mode_cb).begin_fn_cb = hp_mode_hier_beginfn_cb;
                XHPROF_G(mode_cb).end_fn_cb   = hp_mode_hier_endfn_cb;
                break;
            case XHPROF_MODE_SAMPLED:
                XHPROF_G(mode_cb).init_cb     = hp_mode_sampled_init_cb;
                XHPROF_G(mode_cb).begin_fn_cb = hp_mode_sampled_beginfn_cb;
                XHPROF_G(mode_cb).end_fn_cb   = hp_mode_sampled_endfn_cb;
                break;
        }

        /* one time initializations */
        hp_init_profiler_state(level);

        /* start profiling from fictitious main() */
        XHPROF_G(root) = estrdup(ROOT_SYMBOL);

        /* start profiling from fictitious main() */
        BEGIN_PROFILING(&XHPROF_G(entries), XHPROF_G(root), hp_profile_flag, NULL);
    }
}

/**
 * Called at request shutdown time. Cleans the profiler's global state.
 */
static void hp_end()
{
    /* Bail if not ever enabled */
    if (!XHPROF_G(ever_enabled)) {
        return;
    }

    /* Stop profiler if enabled */
    if (XHPROF_G(enabled)) {
        hp_stop();
    }

    /* Clean up state */
    hp_clean_profiler_state();
}

/**
 * Called from xhprof_disable(). Removes all the proxies setup by
 * hp_begin() and restores the original values.
 */
static void hp_stop()
{
    int   hp_profile_flag = 1;

    /* End any unfinished calls */
    while (XHPROF_G(entries)) {
        END_PROFILING(&XHPROF_G(entries), hp_profile_flag);
    }

    if (XHPROF_G(root)) {
        efree(XHPROF_G(root));
        XHPROF_G(root) = NULL;
    }

    /* Stop profiling */
    XHPROF_G(enabled) = 0;
}


/**
 * *****************************
 * XHPROF ZVAL UTILITY FUNCTIONS
 * *****************************
 */

/** Look in the PHP assoc array to find a key and return the zval associated
 *  with it.
 *
 *  @author mpal
 **/
static zval *hp_zval_at_key(char  *key, zval *values)
{
    zval *result = NULL;

    if (Z_TYPE_P(values) == IS_ARRAY) {
        HashTable *ht;
        uint len = strlen(key);

        result = zend_hash_str_find(Z_ARRVAL_P(values), key, len);
    }

    return result;
}

/** Convert the PHP array of strings to an emalloced array of strings. Note,
 *  this method duplicates the string data in the PHP array.
 *
 *  @author mpal
 **/
static char **hp_strings_in_zval(zval  *values)
{
    char   **result;
    size_t   count;
    size_t   ix = 0;

    if (!values) {
        return NULL;
    }

    if (Z_TYPE_P(values) == IS_ARRAY) {

        HashTable *ht;
        zend_ulong num_key;
        zend_string *key;
        zval *val;

        ht    = Z_ARRVAL_P(values);
        count = zend_hash_num_elements(ht);

        if((result = (char**) emalloc(sizeof(char*) * (count + 1))) == NULL) {
            return result;
        }

        ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, key, val) {
            if (!key) {
                if (Z_TYPE_P(val) == IS_STRING && strcmp(Z_STRVAL_P(val), ROOT_SYMBOL) != 0) {
                    /* do not ignore "main" */
                    result[ix] = estrdup(Z_STRVAL_P(val));
                    ix++;
                }
            }
        } ZEND_HASH_FOREACH_END();

    } else if (Z_TYPE_P(values) == IS_STRING) {
        if ((result = (char**) emalloc(sizeof(char*) * 2)) == NULL) {
            return result;
        }
        result[0] = estrdup(Z_STRVAL_P(values));
        ix = 1;
    } else {
        result = NULL;
    }

    /* NULL terminate the array */
    if (result != NULL) {
        result[ix] = NULL;
    }

    return result;
}

/* Free this memory at the end of profiling */
static inline void hp_array_del(char **name_array)
{
    if (name_array != NULL) {
        int i = 0;
        for(; name_array[i] != NULL && i < XHPROF_MAX_IGNORED_FUNCTIONS; i++) {
            efree(name_array[i]);
        }

        efree(name_array);
    }
}

zend_string *hp_pcre_match(char *pattern, int len, zval *data, zend_ulong idx)
{
    zval matches, *match;
    zval rsubparts, *subparts;
    pcre_cache_entry *pce_regexp;
    zend_string *pattern_str, *result = NULL;

    pattern_str = zend_string_init(pattern, len, 0);
    if ((pce_regexp = pcre_get_compiled_regex_cache(pattern_str)) == NULL) {
        zend_string_release(pattern_str);
        return NULL;
    }

    ZVAL_NULL(&rsubparts);
    subparts = &rsubparts;

    php_pcre_match_impl(pce_regexp, Z_STRVAL_P(data), Z_STRLEN_P(data), &matches, subparts /* subpats */,
                        0/* global */, 0/* ZEND_NUM_ARGS() >= 4 */, 0/*flags PREG_OFFSET_CAPTURE*/, 0/* start_offset */);

    if (zend_hash_num_elements(Z_ARRVAL_P(subparts))) {
        match = zend_hash_index_find(Z_ARRVAL_P(subparts), idx);

        if (match != NULL) {
            result = zend_string_init(Z_STRVAL_P(match), Z_STRLEN_P(match), 0);
        }
    }

    zend_string_release(pattern_str);
    zval_ptr_dtor(&matches);
    zval_ptr_dtor(subparts);

    return result;
}

zend_string *hp_pcre_replace(char *pattern, int len, zval *repl, zval *data, int limit)
{
    pcre_cache_entry *pce_regexp;
    zend_string *pattern_str, *replace;

    pattern_str = zend_string_init(pattern, len, 0);

    if ((pce_regexp = pcre_get_compiled_regex_cache(pattern_str)) == NULL) {
        return NULL;
    }

    zend_string_release(pattern_str);

#if PHP_VERSION_ID < 70200
    if (Z_TYPE_P(data) != IS_STRING) {
        convert_to_string(data);
    }

    replace = php_pcre_replace_impl(pce_regexp, NULL, Z_STRVAL_P(repl), Z_STRLEN_P(repl), data, 0, limit, 0);
#elif PHP_VERSION_ID >= 70200
    zend_string *z_str = zval_get_string(data);

    replace = php_pcre_replace_impl(pce_regexp, NULL, Z_STRVAL_P(repl), Z_STRLEN_P(repl), z_str, limit, 0);

    zend_string_release(z_str);
#endif

    return replace;
}

char* hp_trace_callback_sql_query(char *symbol, zend_execute_data *data)
{
    char *result;

    if (strcmp(symbol, "mysqli_query") == 0) {
        zval *arg = ZEND_CALL_ARG(data, 2);
        spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL_P(arg));
    } else {
        zval *arg = ZEND_CALL_ARG(data, 1);
        spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL_P(arg));
    }

    return result;
}

char* hp_trace_callback_pdo_statement_execute(char *symbol, zend_execute_data *data)
{
    char *result;
    zend_class_entry *pdo_ce;
    zval *object = (data->This.value.obj) ? &(data->This) : NULL;
    zval *query_string, *arg, copy_query;

    if (object != NULL) {
        query_string = zend_read_property(pdo_ce, object, ZEND_STRL("queryString"), 0, NULL);

        if (query_string == NULL || Z_TYPE_P(query_string) != IS_STRING) {
            spprintf(&result, 0, "%s", symbol);
            return result;
        }

#ifndef HAVE_PCRE
        spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL_P(query_string));
        return result;
#endif

        if (ZEND_CALL_NUM_ARGS(data) < 1) {
            spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL_P(query_string));
            return result;
        }

        arg = ZEND_CALL_ARG(data, 1);
        if (Z_TYPE_P(arg) != IS_ARRAY) {
            spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL_P(query_string));
            return result;
        }

        zend_string *pattern_str = NULL;

        ZVAL_STR(&copy_query, zval_get_string(query_string));

        if (strstr(Z_STRVAL(copy_query), "?") != NULL) {
            pattern_str = zend_string_init("([\?])", sizeof("([\?])") - 1, 0);
        } else if (strstr(Z_STRVAL(copy_query), ":") != NULL) {
            pattern_str = zend_string_init("(:([^\\s]+))", sizeof("(:([^\\s]+))") - 1, 0);
        }

        if (pattern_str) {
            zend_string *match;
            if ((match = hp_pcre_match(ZSTR_VAL(pattern_str), ZSTR_LEN(pattern_str), &copy_query, 0))) {
                zend_ulong num_key;
                zend_string *key;
                zval *val;
                zend_string *replace;

                ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arg), num_key, key, val)
                {
                    replace = hp_pcre_replace(ZSTR_VAL(pattern_str), ZSTR_LEN(pattern_str), &copy_query, val, 1);

                    if (replace != NULL) {
                        zval_ptr_dtor(&copy_query);
                        ZVAL_STR(&copy_query, replace);
                    }

                }ZEND_HASH_FOREACH_END();

                zend_string_release(match);
            }

            zend_string_release(pattern_str);

            spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL(copy_query));

        } else {
            spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL(copy_query));
        }

        zval_ptr_dtor(&copy_query);

    } else {
        spprintf(&result, 0, "%s", symbol);
    }

    return result;
}

char* hp_trace_callback_curl_exec(char *symbol, zend_execute_data *data)
{
    char *result;
    zval func, retval, *option;
    zval *arg = ZEND_CALL_ARG(data, 1);

    if (arg == NULL || Z_TYPE_P(arg) != IS_RESOURCE) {
        return symbol;
    }

    zval params[1];
    ZVAL_COPY(&params[0], arg);
    ZVAL_STRING(&func, "curl_getinfo");

    zend_fcall_info fci = {
            size: sizeof(zend_fcall_info),
#if PHP_VERSION_ID < 70100
            function_table: EG(function_table),
#endif
            function_name: func,
#if PHP_VERSION_ID < 70100
            symbol_table: NULL,
#endif
            retval: &retval,
            params: &params,
            object: NULL,
            no_separation: 1,
            param_count: 1
    };

    if (zend_call_function(&fci, NULL) == FAILURE) {
        spprintf(&result, 0, "%s#%s", symbol, "unknown");
    } else {
        option = zend_hash_str_find(Z_ARRVAL(retval), ZEND_STRL("url"));
        spprintf(&result, 0, "%s#%s", symbol, Z_STRVAL_P(option));
    }

    zval_ptr_dtor(&func);
    zval_ptr_dtor(&retval);

    return result;
}

char *hp_get_trace_callback(char* symbol, zend_execute_data *data)
{
    char *result;
    hp_trace_callback *callback;

    if (XHPROF_G(trace_callbacks)) {
        callback = (hp_trace_callback*)zend_hash_str_find_ptr(XHPROF_G(trace_callbacks), symbol, strlen(symbol));
        if (callback) {
            result = (*callback)(symbol, data);
        } else {
            return symbol;
        }
    } else {
        return symbol;
    }

    efree(symbol);

    return result;
}

static inline void hp_free_trace_callbacks(zval *val) {
    efree(Z_PTR_P(val));
}

void hp_init_trace_callbacks()
{
    hp_trace_callback callback;

    if (INI_INT("xhprof.collect_additional_info") == 0) {
        return;
    }

    if (XHPROF_G(trace_callbacks)) {
        return;
    }

    XHPROF_G(trace_callbacks) = NULL;
    ALLOC_HASHTABLE(XHPROF_G(trace_callbacks));

    if (!XHPROF_G(trace_callbacks)) {
        return;
    }

    zend_hash_init(XHPROF_G(trace_callbacks), 8, NULL, hp_free_trace_callbacks, 0);

    callback = hp_trace_callback_sql_query;
    register_trace_callback("PDO::exec", callback);
    register_trace_callback("PDO::query", callback);
    register_trace_callback("mysql_query", callback);
    register_trace_callback("mysqli_query", callback);
    register_trace_callback("mysqli::query", callback);

    callback = hp_trace_callback_pdo_statement_execute;
    register_trace_callback("PDOStatement::execute", callback);

    callback = hp_trace_callback_curl_exec;
    register_trace_callback("curl_exec", callback);
}
