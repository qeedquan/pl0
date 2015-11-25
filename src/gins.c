#include "dat.h"
#include "fns.h"

/* structure to store call
   data so we can fix the addresses
   later when we know the address to return
*/
typedef struct Call Call;

struct Call
{
    Sym *sym;
    int  pos;
};

/* stores all procedure location
   during code gen so it can get fixed later
*/
static Sym *ptab[SYMTAB * (MAX_LEXI_LEVEL+1)];
static int  plen;

/* stores all the call instruction places
   so it can be fixed later when we have more info
*/
static Call ctab[MAX_CODE_LENGTH];
static int  clen;

/* instruction buffer we generate code into */
Ins code[MAX_CODE_LENGTH];
int codepos;

/* push a unresolved call to the table */
void pushcall(Sym *s)
{
    Call *c;

    if (clen >= nelem(ctab))
        die("internal error: call table size exceeded");

    c = &ctab[clen++];
    c->sym = s;
    c->pos = codepos;
}

/* to fix call address when we have enough info */
static void fixcall(Sym *s)
{
    Call *c;
    int i, j;

    j = clen;
    for (i = 0; i < j; )
    {
        c = &ctab[i];
        if (strcmp(c->sym->name, s->name) == 0)
        {
            code[c->pos].m = s->addr;

            *c = ctab[j];
            j--;
            continue;
        }

        i++;
    }

    clen = j;
}

/* push unresolved procedure to table */
void pushproc(Sym *s)
{
    if (plen >= nelem(ptab))
        die("internal error: procedure table size exceeded");

    ptab[plen++] = s;
}

/* fix the unresolved procedure when we have enough info */
void popproc(void)
{
    Sym *s;

    if (plen == 0)
        return;

    s = ptab[--plen];
    s->addr = codepos;
    fixcall(s);
}

/* emit an instruction to the code buffer */
void emit(int op, int l, int m)
{
    Ins *i;

    if (codepos >= MAX_CODE_LENGTH)
        die("internal error: exceeded max code buffer size");

    i = &code[codepos++];
    i->op = op;
    i->l = l;
    i->m = m;
}

/* linking stage */
static void link(void)
{
    loadinsbuf(code, codepos);
}

/* compiles a source file and runs it, if any
   previous stage fails, it exits
*/
void compile(char *f)
{
    if (verbose)
    {
        printfile(f);
        printf("\nBegin lexing stage:\n");
    }

    if (tokenize(f) < 0)
        die("Encountered errors in the lexing stage, aborting");

    if (verbose)
        printf("Lexical analysis complete.\n\n");
    
    if (lexonly)
        exit(0);

    if (verbose)
        printf("Begin parsing stage:\n\n");

    plen = 0;
    clen = 0;
    memset(ptab, 0, sizeof(ptab));
    memset(ctab, 0, sizeof(ctab));

    if (parse() < 0)
        die("Encountered error(s) in the parsing stage, aborting");

    if (verbose)
    {
        printf("No errors, program is syntactically correct\n\n");
        printf("Executing code\n\n");
    }
    link();
}
