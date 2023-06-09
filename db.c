#include "./db.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./comm.h"

#define MAXLEN 256

// The root node of the binary tree, unlike all
// other nodes in the tree, this one is never
// freed (it's allocated in the data region).
node_t head = {"", "", 0, 0};

node_t *node_constructor(char *arg_name, char *arg_value, node_t *arg_left,
                         node_t *arg_right) {
    size_t name_len = strlen(arg_name);
    size_t val_len = strlen(arg_value);

    if (name_len > MAXLEN || val_len > MAXLEN) return 0;

    node_t *new_node = (node_t *)malloc(sizeof(node_t));

    if (new_node == 0) return 0;

    if ((new_node->name = (char *)malloc(name_len + 1)) == 0) {
        free(new_node);
        return 0;
    }

    if ((new_node->value = (char *)malloc(val_len + 1)) == 0) {
        free(new_node->name);
        free(new_node);
        return 0;
    }

    if ((snprintf(new_node->name, MAXLEN, "%s", arg_name)) < 0) {
        free(new_node->value);
        free(new_node->name);
        free(new_node);
        return 0;
    } else if ((snprintf(new_node->value, MAXLEN, "%s", arg_value)) < 0) {
        free(new_node->value);
        free(new_node->name);
        free(new_node);
        return 0;
    }

    new_node->lchild = arg_left;
    new_node->rchild = arg_right;
    return new_node;
}

void node_destructor(node_t *node) {
    if (node->name != 0) free(node->name);
    if (node->value != 0) free(node->value);
    free(node);
}

void db_query(char *name, char *result, int len) {
    // TODO: Make this thread-safe!
    node_t *target;
    int rlerr;
    if ((rlerr = pthread_rwlock_rdlock(&head.lock)) != 0) {
        handle_error_en(rlerr, "pthread_rwlock_rdlock");
    }
    target = search(name, &head, 0, l_read);

    if (target == 0) {
        snprintf(result, len, "not found");
        return;
    } else {
        int ulock;
        if ((ulock = pthread_rwlock_unlock(&target->lock)) != 0) {
            handle_error_en(ulock, "pthread_rwlock_unlock");
        }
        snprintf(result, len, "%s", target->value);
        return;
    }
}

int db_add(char *name, char *value) {
    // TODO: Make this thread-safe!
    node_t *parent;
    node_t *target;
    node_t *newnode;
    int wrerr;
    if ((wrerr = pthread_rwlock_wrlock(&head.lock)) != 0) {
        handle_error_en(wrerr, "pthread_rwlock_wrlock");
    }

    if ((target = search(name, &head, &parent, l_write)) != 0) {
        int tar_ulock;
        if ((tar_ulock = pthread_rwlock_unlock(&target->lock)) != 0) {
            handle_error_en(tar_ulock, "pthread_rwlock_unlock");
        }
        int par_ulock;
        if ((par_ulock = pthread_rwlock_unlock(&parent->lock)) != 0) {
            handle_error_en(par_ulock, "pthread_rwlock_unlock");
        }
        return (0);
    }

    newnode = node_constructor(name, value, 0, 0);
    int init_err;
    if ((init_err = pthread_rwlock_init(&newnode->lock, 0)) != 0) {
        handle_error_en(init_err, "pthread_rwlock_init");
    }

    if (strcmp(name, parent->name) < 0)
        parent->lchild = newnode;
    else
        parent->rchild = newnode;

    int uulock;
    if ((uulock = pthread_rwlock_unlock(&parent->lock)) != 0) {
        handle_error_en(uulock, "pthread_rwlock_unlock");
    }

    return (1);
}

int db_remove(char *name) {
    // TODO: Make this thread-safe!
    node_t *parent;
    node_t *dnode;
    node_t *next;
    int wrerr;
    if ((wrerr = pthread_rwlock_wrlock(&head.lock)) != 0) {
        handle_error_en(wrerr, "pthread_rwlock_wrlock");
    }

    // first, find the node to be removed
    if ((dnode = search(name, &head, &parent, l_write)) == 0) {
        // it's not there
        int uulock;
        if ((uulock = pthread_rwlock_unlock(&parent->lock)) != 0) {
            handle_error_en(uulock, "pthread_rwlock_unlock");
        }

        return (0);
    }
    // pthread_rwlock_unlock(&dnode->lock);
    // pthread_rwlock_unlock(&parent->lock);

    // We found it, if the node has no
    // right child, then we can merely replace its parent's pointer to
    // it with the node's left child.

    if (dnode->rchild == 0) {
        if (dnode->lchild != 0) {
            int wrerr;
            if ((wrerr = pthread_rwlock_wrlock(&dnode->lchild->lock)) != 0) {
                handle_error_en(wrerr, "pthread_rwlock_wrlock");
            }
        }

        if (strcmp(dnode->name, parent->name) < 0)
            parent->lchild = dnode->lchild;
        else
            parent->rchild = dnode->lchild;

        int dunlock;
        if ((dunlock = pthread_rwlock_unlock(&dnode->lock)) != 0) {
            handle_error_en(dunlock, "pthread_rwlock_unlock");
        }

        if (dnode->lchild != 0) {
            int ul2err;
            if ((ul2err = pthread_rwlock_unlock(&dnode->lchild->lock)) != 0) {
                handle_error_en(ul2err, "pthread_rwlock_unlock");
            }
        }
        int ul3err;
        if ((ul3err = pthread_rwlock_unlock(&parent->lock)) != 0) {
            handle_error_en(ul3err, "pthread_rwlock_unlock");
        }
        // done with dnode
        node_destructor(dnode);
    } else if (dnode->lchild == 0) {
        if (dnode->rchild != 0) {
            int wr2err;
            if ((wr2err = pthread_rwlock_wrlock(&dnode->rchild->lock)) != 0) {
                handle_error_en(wr2err, "pthread_rwlock_wrlock");
            }
        }

        // ditto if the node had no left child
        if (strcmp(dnode->name, parent->name) < 0)
            parent->lchild = dnode->rchild;
        else
            parent->rchild = dnode->rchild;

        int dun2lock;
        if ((dun2lock = pthread_rwlock_unlock(&dnode->lock)) != 0) {
            handle_error_en(dun2lock, "pthread_rwlock_unlock");
        }

        if (dnode->rchild != 0) {
            int ul4err;
            if ((ul4err = pthread_rwlock_unlock(&dnode->rchild->lock)) != 0) {
                handle_error_en(ul4err, "pthread_rwlock_unlock");
            }
        }
        int ul5err;
        if ((ul5err = pthread_rwlock_unlock(&parent->lock)) != 0) {
            handle_error_en(ul5err, "pthread_rwlock_unlock");
        }
        // done with dnode
        node_destructor(dnode);
    } else {
        // Find the lexicographically smallest node in the right subtree and
        // replace the node to be deleted with that node. This new node thus is
        // lexicographically smaller than all nodes in its right subtree, and
        // greater than all nodes in its left subtree
        int ul6err;
        if ((ul6err = pthread_rwlock_unlock(&parent->lock)) != 0) {
            handle_error_en(ul6err, "pthread_rwlock_unlock");
        }

        next = dnode->rchild;
        node_t **pnext = &dnode->rchild;

        int wr3err;
        if ((wr3err = pthread_rwlock_wrlock(&next->lock)) != 0) {
            handle_error_en(wr3err, "pthread_rwlock_wrlock");
        }

        while (next->lchild != 0) {
            // work our way down the lchild chain, finding the smallest node
            // in the subtree.h
            node_t *nextl = next->lchild;
            pnext = &next->lchild;
            int wr4err;
            if ((wr4err = pthread_rwlock_wrlock(&nextl->lock)) != 0) {
                handle_error_en(wr4err, "pthread_rwlock_wrlock");
            }

            int ul7err;
            if ((ul7err = pthread_rwlock_unlock(&next->lock)) != 0) {
                handle_error_en(ul7err, "pthread_rwlock_unlock");
            }
            next = nextl;
        }

        dnode->name = realloc(dnode->name, strlen(next->name) + 1);
        dnode->value = realloc(dnode->value, strlen(next->value) + 1);

        snprintf(dnode->name, MAXLEN, "%s", next->name);
        snprintf(dnode->value, MAXLEN, "%s", next->value);
        *pnext = next->rchild;

        int ul8err;
        if ((ul8err = pthread_rwlock_unlock(&next->lock)) != 0) {
            handle_error_en(ul8err, "pthread_rwlock_unlock");
        }

        int ul9err;
        if ((ul9err = pthread_rwlock_unlock(&dnode->lock)) != 0) {
            handle_error_en(ul9err, "pthread_rwlock_unlock");
        }

        node_destructor(next);
    }

    return (1);
}

node_t *search(char *name, node_t *parent, node_t **parentpp,
               enum locktype lt) {
    // Search the tree, starting at parent, for a node containing
    // name (the "target node").  Return a pointer to the node,
    // if found, otherwise return 0.  If parentpp is not 0, then it points
    // to a location at which the address of the parent of the target node
    // is stored.  If the target node is not found, the location pointed to
    // by parentpp is set to what would be the the address of the parent of
    // the target node, if it were there.
    //
    // TODO: Make this thread-safe!

    node_t *next;
    node_t *result;

    if (strcmp(name, parent->name) < 0) {
        next = parent->lchild;
    } else {
        next = parent->rchild;
    }

    if (next == NULL) {
        result = NULL;
    } else {
        if (lt == l_read) {
            int rd2err;
            if ((rd2err = pthread_rwlock_rdlock(&next->lock)) != 0) {
                handle_error_en(rd2err, "pthread_rwlock_rdlock");
            }
        } else {
            int wrerr;
            if ((wrerr = pthread_rwlock_wrlock(&next->lock)) != 0) {
                handle_error_en(wrerr, "pthread_rwlock_wrlock");
            }
        }
        if (strcmp(name, next->name) == 0) {
            result = next;
        } else {
            int ulerr;
            if ((ulerr = pthread_rwlock_unlock(&parent->lock)) != 0) {
                handle_error_en(ulerr, "pthread_rwlock_unlock");
            }
            return search(name, next, parentpp, lt);
        }
    }

    if (parentpp != NULL) {
        *parentpp = parent;
    } else {
        int ul2err;
        if ((ul2err = pthread_rwlock_unlock(&parent->lock)) != 0) {
            handle_error_en(ul2err, "pthread_rwlock_unlock");
        }
    }

    return result;
}

static inline void print_spaces(int lvl, FILE *out) {
    for (int i = 0; i < lvl; i++) {
        fprintf(out, " ");
    }
}

/* helper function for db_print */
void db_print_recurs(node_t *node, int lvl, FILE *out) {
    // TODO: Make this thread-safe!
    // print spaces to differentiate levels
    print_spaces(lvl, out);

    // print out the current node
    if (node == NULL) {
        fprintf(out, "(null)\n");
        return;
    }
    int rl_err;
    if ((rl_err = pthread_rwlock_rdlock(&node->lock)) != 0) {
        handle_error_en(rl_err, "pthread_rwlock_rdlock");
    }

    if (node == &head) {
        fprintf(out, "(root)\n");
    } else {
        fprintf(out, "%s %s\n", node->name, node->value);
    }

    db_print_recurs(node->lchild, lvl + 1, out);
    db_print_recurs(node->rchild, lvl + 1, out);

    int ul_err;
    if ((ul_err = pthread_rwlock_unlock(&node->lock)) != 0) {
        handle_error_en(ul_err, "pthread_rwlock_unlock");
    }
}

int db_print(char *filename) {
    FILE *out;
    if (filename == NULL) {
        db_print_recurs(&head, 0, stdout);
        return 0;
    }

    // skip over leading whitespace
    while (isspace(*filename)) {
        filename++;
    }

    if (*filename == '\0') {
        db_print_recurs(&head, 0, stdout);
        return 0;
    }

    if ((out = fopen(filename, "w+")) == NULL) {
        return -1;
    }

    db_print_recurs(&head, 0, out);
    fclose(out);

    return 0;
}

/* Recursively destroys node and all its children. */
void db_cleanup_recurs(node_t *node) {
    if (node == NULL) {
        return;
    }

    db_cleanup_recurs(node->lchild);
    db_cleanup_recurs(node->rchild);

    node_destructor(node);
}

void db_cleanup() {
    db_cleanup_recurs(head.lchild);
    db_cleanup_recurs(head.rchild);
}

void interpret_command(char *command, char *response, int len) {
    char value[MAXLEN];
    char ibuf[MAXLEN];
    char name[MAXLEN];
    int sscanf_ret;

    if (strlen(command) <= 1) {
        snprintf(response, len, "ill-formed command");
        return;
    }

    // which command is it?
    switch (command[0]) {
        case 'q':
            // Query
            sscanf_ret = sscanf(&command[1], "%255s", name);
            if (sscanf_ret < 1) {
                snprintf(response, len, "ill-formed command");
                return;
            }
            db_query(name, response, len);
            if (strlen(response) == 0) {
                snprintf(response, len, "not found");
            }

            return;

        case 'a':
            // Add to the database
            sscanf_ret = sscanf(&command[1], "%255s %255s", name, value);
            if (sscanf_ret < 2) {
                snprintf(response, len, "ill-formed command");
                return;
            }
            if (db_add(name, value)) {
                snprintf(response, len, "added");
            } else {
                snprintf(response, len, "already in database");
            }

            return;

        case 'd':
            // Delete from the database
            sscanf_ret = sscanf(&command[1], "%255s", name);
            if (sscanf_ret < 1) {
                snprintf(response, len, "ill-formed command");
                return;
            }
            if (db_remove(name)) {
                snprintf(response, len, "removed");
            } else {
                snprintf(response, len, "not in database");
            }

            return;

        case 'f':
            // process the commands in a file (silently)
            sscanf_ret = sscanf(&command[1], "%255s", name);
            if (sscanf_ret < 1) {
                snprintf(response, len, "ill-formed command");
                return;
            }

            FILE *finput = fopen(name, "r");
            if (!finput) {
                snprintf(response, len, "bad file name");
                return;
            }
            while (fgets(ibuf, sizeof(ibuf), finput) != 0) {
                pthread_testcancel();  // fgets is not a cancellation point
                interpret_command(ibuf, response, len);
            }
            fclose(finput);
            snprintf(response, len, "file processed");
            return;

        default:
            snprintf(response, len, "ill-formed command");
            return;
    }
}
