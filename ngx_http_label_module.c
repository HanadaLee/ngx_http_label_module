
/*
 * Copyright (C) Hanada
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_array_t              *labels;
    ngx_hash_t                hash;
    ngx_hash_keys_arrays_t   *hash_keys;
    ngx_uint_t                hash_max_size;
    ngx_uint_t                hash_bucket_size;
} ngx_http_label_main_conf_t;


static ngx_int_t ngx_http_label_add_variables(ngx_conf_t *cf);
static void *ngx_http_label_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_label_init_main_conf(ngx_conf_t *cf, void *conf);
static char *ngx_http_label(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_labels_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_label_prefix_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


static ngx_command_t  ngx_http_label_commands[] = {

    { ngx_string("label"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE2,
      ngx_http_label,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("labels_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_label_main_conf_t, hash_max_size),
      NULL },

    { ngx_string("labels_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_label_main_conf_t, hash_bucket_size),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_label_module_ctx = {
    ngx_http_label_add_variables,           /* preconfiguration */
    NULL,                                   /* postconfiguration */

    ngx_http_label_create_main_conf,        /* create main configuration */
    ngx_http_label_init_main_conf,          /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    NULL,                                   /* create location configuration */
    NULL                                    /* merge location configuration */
};


ngx_module_t ngx_http_label_module = {
    NGX_MODULE_V1,
    &ngx_http_label_module_ctx,              /* module context */
    ngx_http_label_commands,                 /* module directives */
    NGX_HTTP_MODULE,                         /* module type */
    NULL,                                    /* init master */
    NULL,                                    /* init module */
    NULL,                                    /* init process */
    NULL,                                    /* init thread */
    NULL,                                    /* exit thread */
    NULL,                                    /* exit process */
    NULL,                                    /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_variable_t  ngx_http_label_vars[] = {

    { ngx_string("labels"), NULL, ngx_http_labels_variable,
        0, 0, 0 },

    { ngx_string("label_"), NULL, ngx_http_label_prefix_variable,
        0, NGX_HTTP_VAR_PREFIX, 0 },

      ngx_http_null_variable
};


static ngx_int_t
ngx_http_label_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_label_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static void *
ngx_http_label_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_label_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_label_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->hash = { NULL };
     *     conf->hash_keys = NULL;
     *     conf->labels = NULL;
     */

    conf->hash_max_size = NGX_CONF_UNSET_UINT;
    conf->hash_bucket_size = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *
ngx_http_label_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_label_main_conf_t *lmcf = conf;

    ngx_hash_init_t   hash;

    ngx_conf_init_uint_value(lmcf->hash_max_size, 512);
    ngx_conf_init_uint_value(lmcf->hash_bucket_size, ngx_cacheline_size);

    hash.key = ngx_hash_key;
    hash.max_size = lmcf->hash_max_size;
    hash.bucket_size = lmcf->hash_bucket_size;
    hash.name = "labels_hash";
    hash.pool = cf->pool;

    if (lmcf->labels->nelts) {
        hash.hash = &lmcf->hash;
        hash.temp_pool = NULL;

        if (ngx_hash_init(&hash, lmcf->hash_keys->keys.elts,
                        lmcf->hash_keys->keys.nelts) != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    lmcf->hash_keys = NULL;

    return NGX_CONF_OK;
}


static char *
ngx_http_label(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_label_main_conf_t *lmcf = conf;

    ngx_str_t      *value;
    u_char         *p;
    ngx_keyval_t   *kv;
    ngx_uint_t      i;

    value = cf->args->elts;

    if (value[1].len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "label key cannot be empty");
        return NGX_CONF_ERROR;
    }

    for (p = value[1].data; p < value[1].data + value[1].len; p++) {
        if (!((*p >= '0' && *p <= '9')
              || (*p >= 'a' && *p <= 'z')
              || (*p >= 'A' && *p <= 'Z')
              || *p == '_'))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "label key only allows "
                "numbers, letters, and underscores, but other characters "
                "are included");
            return NGX_CONF_ERROR;
        }
        *p = ngx_tolower(*p);
    }

    for (p = value[2].data; p < value[2].data + value[2].len; p++) {
        if (*p == '&' || *p == '=') {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "label value contains "
                               "invalid characters");
            return NGX_CONF_ERROR;
        }
    }

    if (lmcf->labels == NULL) {
        lmcf->labels = ngx_array_create(cf->pool, 4, sizeof(ngx_keyval_t));
        if (lmcf->labels == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    for (i = 0; i < lmcf->labels->nelts; i++) {
        kv = lmcf->labels->elts;
        if (value[1].len == kv[i].key.len &&
            ngx_strncmp(value[1].data, kv[i].key.data, value[1].len) == 0)
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate label key \"%s\"",
                               value[1].data);
            return NGX_CONF_ERROR;
        }
    }

    kv = ngx_array_push(lmcf->labels);
    if (kv == NULL) {
        return NGX_CONF_ERROR;
    }

    kv->key = value[1];
    kv->value = value[2];

    if (lmcf->hash_keys == NULL) {
        lmcf->hash_keys = ngx_pcalloc(cf->temp_pool,
            sizeof(ngx_hash_keys_arrays_t));
        if (lmcf->hash_keys == NULL) {
            return NGX_CONF_ERROR;
        }

        lmcf->hash_keys->pool = cf->pool;
        lmcf->hash_keys->temp_pool = cf->pool;

        if (ngx_hash_keys_array_init(lmcf->hash_keys, NGX_HASH_SMALL)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (ngx_hash_add_key(lmcf->hash_keys, &kv->key, kv, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_label_prefix_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t *name = (ngx_str_t *) data;

    ngx_http_label_main_conf_t *lmcf;

    u_char         *label;
    size_t          len;
    ngx_keyval_t   *kv;
    ngx_uint_t      key; 

    len = name->len - (sizeof("label_") - 1);
    label = name->data + sizeof("label_") - 1;

    if (len == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    lmcf = ngx_http_get_module_main_conf(r, ngx_http_label_module);
    if (lmcf == NULL || lmcf->labels == NULL || lmcf->hash.buckets == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    key = ngx_hash_key(label, len);

    kv = ngx_hash_find(&lmcf->hash, key, label, len);
    if (kv == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ngx_pnalloc(r->pool, kv->value.len);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(v->data, kv->value.data, kv->value.len);

    v->len = kv->value.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_labels_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_label_main_conf_t *lmcf;

    ngx_keyval_t   *kv;
    ngx_uint_t      i;
    size_t          len;
    u_char         *p, *buf;

    lmcf = ngx_http_get_module_main_conf(r, ngx_http_label_module);
    if (lmcf == NULL || lmcf->labels->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    kv = lmcf->labels->elts;
    i = lmcf->labels->nelts;

    len = 0;

    for (i = 0; i < lmcf->labels->nelts; i++) {

        len += kv[i].key.len + 1 + kv[i].value.len;  /* key=value */

        if (i != lmcf->labels->nelts - 1) {
            len += 1;  /* & */
        }
    }

    buf = ngx_pnalloc(r->pool, len);
    if (buf == NULL) {
        return NGX_ERROR;
    }

    p = buf;

    for (i = 0; i < lmcf->labels->nelts; i++) {
        p = ngx_copy(p, kv[i].key.data, kv[i].key.len);

        *p++ = '=';

        p = ngx_copy(p, kv[i].value.data, kv[i].value.len);

        if (i != lmcf->labels->nelts - 1) {
            *p++ = '&';
        }
    }

    v->data = buf;
    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}