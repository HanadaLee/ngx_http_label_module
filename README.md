# ngx_http_label_module

# Name
`ngx_http_label_module` allows defining global key-value labels in Nginx configuration. These labels can be used in variables for request processing, logging, or dynamic configuration.

# Table of Content

- [ngx\_http\_label\_module](#ngx_http_label_module)
- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Directives](#directives)
  - [label](#label)
  - [labels\_hash\_max\_size](#labels_hash_max_size)
  - [labels\_hash\_bucket\_size](#labels_hash_bucket_size)
- [Variables](#variables)
  - [$label\_*name*](#label_name)
  - [$labels](#labels)
- [Author](#author)
- [License](#license)

# Status

This Nginx module is currently considered experimental. Issues and PRs are welcome if you encounter any problems.

# Synopsis

```nginx
http {
    label environment production;
    label cluster_id my_cluster_id;
    label server_region us-east-1;
    label server_id my_server_id;
    label ...

    server {
        listen 80;
        server_name example.com;
        location / {
            add_header Server-Id $label_server_id;
            add_header Cluster-Id $label_cluster_id;
            add_header All-Labels $labels;
            return 204;
        }
    }
}
```

# Installation

To use theses modules, configure your nginx branch with `--add-module=/path/to/ngx_http_label_module`.

# Directives

## label

**Syntax:** *label key value;*

**Default:** *none*

**Context:** *http*

Defines a global key-value label that can be accessed via variables.
The label key is only allowed to be letters, numbers, and `_`. The same key cannot be defined repeatedly.
The label value does not allow the use of `&` and `=`.

Example:
```nginx
label environment production;
label region us-east-1;
```

## labels_hash_max_size

**Syntax:** *labels_hash_max_size number;*

**Default:** *labels_hash_max_size 512;*

**Context:** *http*

Sets the maximum size of the hash table for storing labels.

## labels_hash_bucket_size

**Syntax:** *labels_hash_bucket_size number;*

**Default:** *labels_hash_bucket_size 32|64|128;*

**Context:** *http*

Sets the bucket size of the hash table for labels. Default value depends on the processor’s cache line size. The details of setting up hash tables are provided in a separate [document](https://nginx.org/en/docs/hash.html).

# Variables

## $label_*name*

Accesses the value of a specific label by its key.

## $labels

Returns all defined labels in the format `key1=value1&key2=value2`, like $args. All label keys will be printed in lowercase letters.

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
