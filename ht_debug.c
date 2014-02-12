#include "ht.h"
#include "ht_p.h"

void ht_debug(const char *file, int line, int argc, const char *fmt, ...)
{
    va_list ap;
    static char str[1024];
    size_t n;

    ht_shield {
        va_start(ap, fmt);
        if (file != NULL)
            ht_snprintf(str, sizeof(str), "%d:%s:%04d: ", (int)getpid(), file, line);
        else
            str[0] = NUL;
        n = strlen(str);
        if (argc == 1)
            ht_util_cpystrn(str+n, fmt, sizeof(str)-n);
        else
            ht_vsnprintf(str+n, sizeof(str)-n, fmt, ap);
        va_end(ap);
        n = strlen(str);
        str[n++] = '\n';
        ht_sc(write)(STDERR_FILENO, str, n);
    }
    return;
}

/* dump out a page to stderr summarizing the internal state of Pth */
void ht_dumpstate(FILE *fp)
{
    fprintf(fp, "+----------------------------------------------------------------------\n");
    fprintf(fp, "| Pth Version: %s\n", PTH_VERSION_STR);
    fprintf(fp, "| Load Average: %.2f\n", ht_loadval);
    ht_dumpqueue(fp, "NEW", &ht_NQ);
    ht_dumpqueue(fp, "READY", &ht_RQ);
    fprintf(fp, "| Thread Queue RUNNING:\n");
    fprintf(fp, "|   1. thread 0x%lx (\"%s\")\n",
            (unsigned long)ht_current, ht_current->name);
    ht_dumpqueue(fp, "WAITING", &ht_WQ);
    ht_dumpqueue(fp, "SUSPENDED", &ht_SQ);
    ht_dumpqueue(fp, "DEAD", &ht_DQ);
    fprintf(fp, "+----------------------------------------------------------------------\n");
    return;
}

void ht_dumpqueue(FILE *fp, const char *qn, ht_pqueue_t *q)
{
    ht_t t;
    int n;
    int i;

    fprintf(fp, "| Thread Queue %s:\n", qn);
    n = ht_pqueue_elements(q);
    if (n == 0)
        fprintf(fp, "|   no threads\n");
    i = 1;
    for (t = ht_pqueue_head(q); t != NULL; t = ht_pqueue_walk(q, t, PTH_WALK_NEXT)) {
        fprintf(fp, "|   %d. thread 0x%lx (\"%s\")\n", i++, (unsigned long)t, t->name);
    }
    return;
}

