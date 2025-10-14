#ifndef XHPROF_TRACE_H
#define XHPROF_TRACE_H

static zend_always_inline void hp_mode_common_beginfn(hp_entry_t **entries, hp_entry_t *current)
{
    hp_entry_t *p;

    /* This symbol's recursive level */
    int recurse_level = 0;

    if (XHPROF_G(func_hash_counters[current->hash_code]) > 0) {
        /* Find this symbols recurse level */
        for (p = (*entries); p; p = p->prev_hprof) {
            if (zend_string_equals(current->name_hprof, p->name_hprof)) {
                recurse_level = (p->rlvl_hprof) + 1;
                break;
            }
        }
    }

    XHPROF_G(func_hash_counters[current->hash_code])++;

    /* Init current function's recurse level */
    current->rlvl_hprof = recurse_level;
}

static zend_always_inline int hp_ignored_functions_filter_collision(hp_ignored_functions *functions, zend_ulong hash)
{
    zend_ulong idx = hash % XHPROF_MAX_IGNORED_FUNCTIONS;
    return functions->filter[idx];
}

static zend_always_inline int hp_ignore_entry_work(zend_ulong hash_code, zend_string *curr_func)
{
    if (XHPROF_G(ignored_functions) == NULL) {
        return 0;
    }

    hp_ignored_functions *functions = XHPROF_G(ignored_functions);

    if (hp_ignored_functions_filter_collision(functions, hash_code)) {
        int i = 0;
        for (; functions->names[i] != NULL; i++) {
            zend_string *name = functions->names[i];
            if (zend_string_equals(curr_func, name)) {
                return 1;
            }
        }
    }

    return 0;
}

static zend_always_inline zend_string *hp_get_function_name(zend_execute_data *execute_data)
{
    zend_function *curr_func;
    zend_string *real_function_name;

    if (!execute_data) {
        return NULL;
    }

    curr_func = execute_data->func;

    if (!curr_func->common.function_name) {
        return NULL;
    }

    if (curr_func->common.scope != NULL) {
        real_function_name = strpprintf(0, "%s::%s", curr_func->common.scope->name->val, ZSTR_VAL(curr_func->common.function_name));
    } else {
        real_function_name = zend_string_copy(curr_func->common.function_name);
    }

    return real_function_name;
}

static zend_always_inline zend_string *hp_get_trace_callback(zend_string *function_name, zend_execute_data *data)
{
    zend_string *trace_name;
    hp_trace_callback *callback;

    if (XHPROF_G(trace_callbacks)) {
        callback = (hp_trace_callback*)zend_hash_find_ptr(XHPROF_G(trace_callbacks), function_name);
        if (callback) {
            trace_name = (*callback)(function_name, data);
            zend_string_release(function_name);
            return trace_name;
        }
    }
    
    /* No callback found, return the original function_name */
    return function_name;
}

static zend_always_inline hp_entry_t *hp_fast_alloc_hprof_entry()
{
    hp_entry_t *p;

    p = XHPROF_G(entry_free_list);

    if (p) {
        XHPROF_G(entry_free_list) = p->prev_hprof;
        return p;
    } else {
        return (hp_entry_t *)malloc(sizeof(hp_entry_t));
    }
}

static zend_always_inline void hp_fast_free_hprof_entry(hp_entry_t *p)
{
    if (p->name_hprof != NULL) {
        zend_string_release(p->name_hprof);
        p->name_hprof = NULL;  /* Prevent double-free if entry is reused */
    }

    /* we use/overload the prev_hprof field in the structure to link entries in
     * the free list.
     * */
    p->prev_hprof = XHPROF_G(entry_free_list);
    XHPROF_G(entry_free_list) = p;
}

static zend_always_inline int begin_profiling(zend_string *root_symbol, zend_execute_data *execute_data)
{
    zend_string *function_name;
    hp_entry_t **entries = &XHPROF_G(entries);

    if (root_symbol == NULL) {
        function_name = hp_get_function_name(execute_data);
    } else {
        function_name = zend_string_copy(root_symbol);
    }

    if (function_name == NULL) {
        return 0;
    }

    zend_ulong hash_code = ZSTR_HASH(function_name);
    int profile_curr = !hp_ignore_entry_work(hash_code, function_name);
    if (profile_curr) {
        if (execute_data != NULL) {
            function_name = hp_get_trace_callback(function_name, execute_data);
        }

        hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();
        (cur_entry)->hash_code = hash_code % XHPROF_FUNC_HASH_COUNTERS_SIZE;
        (cur_entry)->name_hprof = function_name;
        (cur_entry)->prev_hprof = (*(entries));
        (cur_entry)->rlvl_hprof = 0;
#if PHP_VERSION_ID >= 80000
        (cur_entry)->is_trace = 1;
#endif
        /* Call the universal callback */
        hp_mode_common_beginfn((entries), (cur_entry));
        /* Call the mode's beginfn callback */
        XHPROF_G(mode_cb).begin_fn_cb((entries), (cur_entry));
        /* Update entries linked list */
        (*(entries)) = (cur_entry);
    } else {
#if PHP_VERSION_ID >= 80000
        hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();
        /* Check if entries is not NULL before dereferencing */
        if (*(entries) != NULL) {
            (cur_entry)->name_hprof = zend_string_copy((*(entries))->name_hprof);
        } else {
            /* If this is the first call and it's ignored, use the function name */
            (cur_entry)->name_hprof = zend_string_copy(function_name);
        }
        (cur_entry)->prev_hprof = (*(entries));
        (cur_entry)->is_trace = 0;
        (cur_entry)->hash_code = (*(entries))->hash_code;
        (cur_entry)->rlvl_hprof = 0;
        (*(entries)) = (cur_entry);
        profile_curr = 1;
#endif
        zend_string_release(function_name);
    }

    return profile_curr;
}

static zend_always_inline void end_profiling()
{
    hp_entry_t *cur_entry;
    hp_entry_t **entries = &XHPROF_G(entries);

    /* Call the mode's endfn callback. */
    /* NOTE(cjiang): we want to call this 'end_fn_cb' before */
    /* 'hp_mode_common_endfn' to avoid including the time in */
    /* 'hp_mode_common_endfn' in the profiling results.      */
    XHPROF_G(mode_cb).end_fn_cb(entries);
    cur_entry = (*(entries));
    /* Free top entry and update entries linked list */
    (*(entries)) = (*(entries))->prev_hprof;
    hp_fast_free_hprof_entry(cur_entry);
}
#endif