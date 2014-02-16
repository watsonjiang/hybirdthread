#include "ht_p.h"

struct ht_keytab_st {
    int used;
    void (*destructor)(void *);
};

static struct ht_keytab_st ht_keytab[HT_KEY_MAX];

int 
ht_key_create(ht_key_t *key, void (*func)(void *))
{
    if (key == NULL)
        return ht_error(FALSE, EINVAL);
    for ((*key) = 0; (*key) < HT_KEY_MAX; (*key)++) {
        if (ht_keytab[(*key)].used == FALSE) {
            ht_keytab[(*key)].used = TRUE;
            ht_keytab[(*key)].destructor = func;
            return TRUE;
        }
    }
    return ht_error(FALSE, EAGAIN);
}

int 
ht_key_delete(ht_key_t key)
{
    if (key < 0 || key >= HT_KEY_MAX)
        return ht_error(FALSE, EINVAL);
    if (!ht_keytab[key].used)
        return ht_error(FALSE, ENOENT);
    ht_keytab[key].used = FALSE;
    return TRUE;
}

int 
ht_key_setdata(ht_key_t key, const void *value)
{
    if (key < 0 || key >= HT_KEY_MAX)
        return ht_error(FALSE, EINVAL);
    if (!ht_keytab[key].used)
        return ht_error(FALSE, ENOENT);
    if (ht_current->data_value == NULL) {
        ht_current->data_value = (const void **)calloc(1, sizeof(void *)*HT_KEY_MAX);
        if (ht_current->data_value == NULL)
            return ht_error(FALSE, ENOMEM);
    }
    if (ht_current->data_value[key] == NULL) {
        if (value != NULL)
            ht_current->data_count++;
    }
    else {
        if (value == NULL)
            ht_current->data_count--;
    }
    ht_current->data_value[key] = value;
    return TRUE;
}

void *
ht_key_getdata(ht_key_t key)
{
    if (key < 0 || key >= HT_KEY_MAX)
        return ht_error((void *)NULL, EINVAL);
    if (!ht_keytab[key].used)
        return ht_error((void *)NULL, ENOENT);
    if (ht_current->data_value == NULL)
        return (void *)NULL;
    return (void *)ht_current->data_value[key];
}

void 
ht_key_destroydata(ht_t t)
{
    void *data;
    int key;
    int itr;
    void (*destructor)(void *);

    if (t == NULL)
        return;
    if (t->data_value == NULL)
        return;
    /* POSIX thread iteration scheme */
    for (itr = 0; itr < HT_DESTRUCTOR_ITERATIONS; itr++) {
        for (key = 0; key < HT_KEY_MAX; key++) {
            if (t->data_count > 0) {
                destructor = NULL;
                data = NULL;
                if (ht_keytab[key].used) {
                    if (t->data_value[key] != NULL) {
                        data = (void *)t->data_value[key];
                        t->data_value[key] = NULL;
                        t->data_count--;
                        destructor = ht_keytab[key].destructor;
                    }
                }
                if (destructor != NULL)
                    destructor(data);
            }
            if (t->data_count == 0)
                break;
        }
        if (t->data_count == 0)
            break;
    }
    free(t->data_value);
    t->data_value = NULL;
    return;
}

