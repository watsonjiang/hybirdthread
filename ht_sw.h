#ifndef HT_H
#define HT_H

struct _ht_task_t;

int ht_init(int num_worker);

int ht_bg_exec_begin();

int ht_bg_exec_end();

#endif //HT_T
