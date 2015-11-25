#include "dat.h"
#include "fns.h"

void *emalloc(size_t size)
{
    void *p;

    p = calloc(1, size);
    if (!p)
        die("oom trying to allocate %zu bytes", size);

    return p;
}

void die(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void printfile(char *f)
{
    FILE *fp;
    int c;

    fp = fopen(f, "r");
    if (!fp)
        die("%s: %s", f, strerror(errno));

    printf("Reading source file: %s\n\n", f);
    printf("Source Program:\n");

    while ((c = fgetc(fp)) != EOF)
        fputc(c, stdout);

    fclose(fp);
}

