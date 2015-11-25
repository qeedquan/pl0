#include "dat.h"
#include "fns.h"

static void expression(void);

/* all the syntax errors, we use variadic arguments
   so there are string formats in here, so we can generate
   better error messages */
static char *syntaxerrors[] =
{
    [1]  = "use = instead of :=",
    [2]  = "= must be followed by a number",
    [3]  = "identifier must be followed by =",
    [4]  = "const, int, procedure must be followed by the identifier",
    [5]  = "semicolon or comma missing",
    [6]  = "incorrect symbol after procedure declaration",
    [7]  = "statement expected",
    [8]  = "incorrect symbol after statement part in block",
    [9]  = "period expected",
    [10] = "semicolon between statements missing",
    [11] = "undeclared identifier '%s'",
    [12] = "assignment to constant or procedure '%s' is not allowed",
    [13] = "assignment operator expected",
    [14] = "call must be followed by an identifier",
    [15] = "call of a constant or variable is meaningless",
    [16] = "then expected",
    [17] = "semicolon expected",
    [18] = "do expected",
    [19] = "incorrect symbol following statement",
    [20] = "relational operator expected",
    [21] = "expression must not contain a procedure identifier",
    [22] = "right parenthesis missing",
    [23] = "preceding factor cannot begin with this symbol",
    [24] = "an expression cannot begin with this symbol",
    [25] = "this number is too large",
    [26] = "equal sign expected in const declaration",
    [27] = "expected number in const declaration",
    [28] = "unexpected identifier '%s' after read operator",
    [29] = "undeclared identifier '%s' used in read operator",
    [30] = "'%s' redeclared at lexi level %d",
    [31] = "encountered too much nested procedures, went over max lexical level (current lexi level is %d)",
    [32] = "parser somehow made it below base level, current lexi level is %d",
    [33] = "call to an undeclared procedure '%s'",
    [34] = "end expected at end of begin block",
    [35] = "adding symbol '%s' failed in lexi level %d because it exceeded the max lexi levels supported",
    [36] = "expression using procedure '%s' as a variable/constant",
    [37] = "use := instead of =",
    [38] = "unknown type declaration in procedure %s",
    [39] = "procedure arguments not an identifier",
    [40] = "invalid procedure declaration",
    [41] = "expected %s",
    [42] = "calling procedure '%s' with mismatched number of arguments, expected %d, got %d",
    [43] = "calling to a non-procedure '%s'",
    [44] = "procedure %s cannot end with a ,"
};

/* for parsing */
static Sym    S;
static int    nerr;

static Symtab stab[MAX_LEXI_LEVEL+1];
static int    npargs[MAX_LEXI_LEVEL+1];
static int    lexi;

/* prints out a parser error, if it is greater
   than max parser error, quit */
static void parerror(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "parser: %s:%ld:%ld: ", ion, line, pos);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (++nerr >= MAX_PARSER_ERROR)
        die("parser: encountered errors, aborting");
}

/* lookup a type of a token, see if it is a valid
   type (ie: int, proc)
*/
static int lookuptype(int t)
{
    static struct
    {
        int kind;
        int sym;
    } types[] =
    {
        {intsym, symint}
    };

    int i;

    for (i = 0; i < nelem(types); i++)
    {
        if (types[i].kind == t)
            return types[i].sym;
    }
    return -1;
}

/* prints error message using status code */
static void error(int err, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, err);
    vsnprintf(buf, sizeof(buf), syntaxerrors[err], ap);
    parerror("error %d: %s", err, buf);
    va_end(ap);
}

/* helper to expect to see if the token we just got is what we wanted */
static void expect(int type, int err, ...)
{
    va_list ap;
    char buf[256];

    if (tok == type)
        return;

    va_start(ap, err);
    vsnprintf(buf, sizeof(buf), syntaxerrors[err], ap);
    parerror("error %d: %s", err, buf);
    va_end(ap);
}

/* add a symbol to the table where it is 
   added is based on the lexi level we are in
*/
static Sym *addsym(Sym *s)
{
    Symtab *t;
    Sym *sl;
    int i;

    if (lexi < 0 || lexi > MAX_LEXI_LEVEL)
    {
        error(35, s->name, lexi);
        return NULL;
    }

    t = &stab[lexi];
    if (t->len >= SYMTAB)
        die("internal error: too many symbols declared");

    for (i = 0; i < t->len; i++)
    {
        if (strcmp(t->sym[i].name, s->name) == 0)
        {
            error(30, s->name, lexi);
            return NULL;
        }
    }

    sl = &t->sym[t->len++];
    *sl = *s;

    return sl;
}

/* lookup a symbol from the table */
static Sym *lookupsym(char *s)
{
    Symtab *t;
    Sym *sym;
    int i, j;

    for (i = lexi; i >= 0; i--)
    {
        t = &stab[i];
        for (j = 0; j < t->len; j++)
        {
            sym = &t->sym[j];
            if (strcmp(sym->name, s) == 0)
                return sym;
        }
    }
    return NULL;
}

static void factor(void)
{
    Sym *s;

    if (tok == identsym)
    {
        s = lookupsym(symb);
        if (!s)
        {
            error(11, symb);
            return;
        }

        /* LOD L, M if variable
           LIT 0, M if constant */
        if (s->type == symconst)
            emit(OLIT, 0, s->lval);
        else if (s->type == symint)
            emit(OLOD, lexi - s->level, s->addr);
        else
        {
            error(36, symb);
            return;
        }

        token();
    }
    else if (tok == numbersym)
    {
        /* a number is a LIT instruction */
        emit(OLIT, 0, lval);
        token();
    }
    else if (tok == lparentsym)
    {
        token();
        expression();
        expect(rparentsym, 22);
        token();
    }
    else
        error(23);
}

static void term(void)
{
    int op;

    factor();
    while (tok == multsym || tok == slashsym)
    {
        op = (tok == multsym) ? OMUL : ODIV;
        token();
        factor();

        emit(OOPR, 0, op);
    }
}

static void expression(void)
{
    int op;

    if (tok == plussym || tok == minussym)
    {
        op = tok;
        token();
        term();
        if (op == minussym)
            emit(OOPR, 0, ONEG);
    }
    else
        term();

    while (tok == plussym || tok == minussym)
    {
        op = (tok == plussym) ? OADD : OSUB;
        token();
        term();
        emit(OOPR, 0, op);
    }
}

static void condition(void)
{
    static struct
    {
        int type;
        int op;
    } rel[] =
    {
        {lessym, OLSS},
        {leqsym, OLEQ},
        {gtrsym, OGTR},
        {geqsym, OGEQ},
        {neqsym, ONEQ},
        {eqsym,  OEQL}
    };

    int i;

    if (tok == oddsym)
    {
        token();
        expression();
        emit(OOPR, 0, OODD);
    }
    else
    {
        expression();

        if (tok == becomessym)
        {
            error(1);
            return;
        }

        for (i = 0; i < nelem(rel); i++)
        {
            if (rel[i].type == tok)
                break;
        }
        if (i == nelem(rel))
        {
            error(20);
            return;
        }
        token();
        expression();
        emit(OOPR, 0, rel[i].op);
    }
}

/* helper to generate a call */
static void gcallproc(void)
{
    Sym *s;
    int n;

    token();
    expect(identsym, 14);
    s = lookupsym(symb);
    if (s->type != symproc)
    {
        error(43, symb);
        return;
    }

    if (!s)
    {
        error(33, symb);
        return;
    }

    token();
    expect(lparentsym, 41, "(");


    /* handle expressions in argument passing */
    token();
    n = 0;
    if (tok != rparentsym)
    {
        for (;;)
        {
            expression();
            emit(OLDS, 0, FRAME + n);
            n++;

            if (tok != commasym)
                break;
            token();
        }

    }
    expect(rparentsym, 41, ")");
    
    if (s->narg != n)
    {
        error(42, s->name, s->narg, n);
        return;
    }

    /* add the current code position to a list
    so we can fix it later, when we have more info
    though if the function address is already known
    don't bother */
    if (s->addr < 0)
        pushcall(s);

    emit(OCAL, lexi - s->level, s->addr);

    token();
}

static void statement(void)
{
    Token *t1, *t2;
    Sym *s;
    int a1, a2;

    if (tok == identsym)
    {
        s = lookupsym(symb);
        if (!s)
        {
            error(11, symb);
            return;
        }

        /* check if it is a variable we can actually assign to */
        if (s->type != symint)
        {
            error(12, symb);
            return;
        }

        token();
        if (tok == eqsym)
            error(37);
        else
            expect(becomessym, 13);
        token();
        expression();

        emit(OSTO, lexi - s->level, s->addr);
    }
    else if (tok == beginsym)
    {
        token();
        statement();
        while (tok == semicolonsym)
        {
            token();
            statement();
        }

        expect(endsym, 34);
        token();
    }
    else if (tok == ifsym)
    {
        token();
        condition();
        expect(thensym, 16);

        /* save the pos for code emitter for statements later on */
        a1 = codepos;
        emit(OJPC, 0, 0);

        token();
        statement();

        /* we need to lookahead here, to see if we found
           a semicolon before an else, for consistency
         */
        if (tok == semicolonsym)
        {
            t1 = curtok;
            token();
            if (tok != elsesym)
            {
                t2 = curtok;
                pushtoken(t2);
                pushtoken(t1);
                token();
            }
        }

        /* emit a jmp so if the condition goes through, it will
           skip the else statement */
        if (tok == elsesym)
        {
            a2 = codepos;
            emit(OJMP, 0, 0);
        }

        /* fix the jpc so that it can skip the if code correctly */
        code[a1].m = codepos;

        /* else */
        if (tok == elsesym)
        {
            token();
            statement();

            code[a2].m = codepos;
        }
    }
    else if (tok == callsym)
    {
        gcallproc();
    }
    else if (tok == whilesym)
    {
        a1 = codepos;
        token();
        condition();
        expect(dosym, 18);

        /* same as if statement code gen except add unconditional jmp */
        a2 = codepos;
        emit(OJPC, 0, 0);

        token();
        statement();

        /* jump back to a1 then on next line we have a jpc */
        emit(OJMP, 0, a1);
        code[a2].m = codepos;
    }
    else if (tok == readsym)
    {
        token();
        expect(identsym, 28);

        s = lookupsym(symb);
        if (!s)
        {
            error(29, symb);
            return;
        }

        if (s->type != symint)
        {
            error(28, symb);
            return;
        }

        emit(OSIO2, 0, 2);
        emit(OSTO, s->level, s->addr);

        token();
    }
    else if (tok == writesym)
    {
        token();
        expression();
        emit(OSIO1, 0, 1);
    }
}

static void block(void)
{
    Sym s, *sl;
    int l, n, t;

    n = 0;

    /* emit a jmp to skip procedure decl
       during execution since we don't execute
       it unless there is a call, then we fix
       the address generated later
    */
    l = codepos;
    emit(OJMP, 0, 0);

    if (tok == constsym)
    {
        s = S;
        s.type = symconst;
        do
        {
            token();
            expect(identsym, 4);

            snprintf(s.name, WBUF, "%s", symb);

            token();
            expect(eqsym, 26);

            token();
            expect(numbersym, 27);

            /* add constant value name to table */
            s.lval = lval;
            s.level = lexi;
            addsym(&s);

            token();
        } while (tok == commasym);

        expect(semicolonsym, 5);
        token();
    }

    if (tok == intsym)
    {
        s = S;
        s.type = symint;
        do
        {
            token();
            expect(identsym, 4);

            /* add int variable name to table */
            snprintf(s.name, WBUF, "%s", symb);
            s.addr = FRAME + npargs[lexi] + n;
            s.level = lexi;
            addsym(&s);

            n++;
            token();
        } while(tok == commasym);

        expect(semicolonsym, 5);
        token();
    }

    while (tok == procsym)
    {
        token();
        expect(identsym, 4);

        /* add in procedure name to table */
        s = S;
        s.type = symproc;
        snprintf(s.name, WBUF, "%s", symb);
        s.addr = -1;
        s.level = lexi;
        sl = addsym(&s);
        if (!sl)
            return;

        /* push procedure to stack because we need to
           fix the address of it when we have enough info
        */
        pushproc(sl);

        if (++lexi > MAX_LEXI_LEVEL)
        {
            error(31, lexi);
            return;
        }

        /* reset length for the current lexi symbol table */
        stab[lexi].len = 0;

        /* procedure arguments and return */
        token();
        expect(lparentsym, 5);
        token();

        /* procedure arguments */
        npargs[lexi] = 0;
        for (;;)
        {
            if (tok == rparentsym)
                break;

            t = lookuptype(tok);
            if (t < 0)
            {
                error(38, s.name);
                return;
            }

            token();
            expect(identsym, 39);

            s = S;
            s.type = t;
            s.addr = FRAME + npargs[lexi];
            s.level = lexi;
            snprintf(s.name, WBUF, "%s", symb);
            addsym(&s);
            npargs[lexi]++;

            token();
            if (tok == commasym)
            {
                token();

                if (tok == rparentsym)
                {
                    error(44, sl->name);
                    return;
                }
            }

            if (tok != rparentsym && lookuptype(tok) < 0)
            {
                error(40);
                return;
            }
        }
        sl->narg = npargs[lexi];

        token();
        /* if there are any return arguments, we handle them here */
        if (tok == lparentsym)
        {
            token();
            t = lookuptype(tok);
            if (t < 0)
            {
                error(38, s.name);
                return;
            }

            token();
            expect(identsym, 39);

            s = S;
            snprintf(s.name, WBUF, "%s", symb);
            s.type = t;
            s.level = lexi;
            s.addr = RA;
            addsym(&s);

            token();
            expect(rparentsym, 41, ")");
            token();
        }

        expect(semicolonsym, 5);
        token();
        block();

        expect(semicolonsym, 5);
        token();
    }

    /* make the jmp address correct now that we know the location */
    code[l].m = codepos;

    /* give the procedure the right M address now that we have it
       (if there is any procedures)
     */
    popproc();

    /* now make the procedure address in cal opcodes correct */

    /* inc mem for stack, do it outside of variable decl
       because of the procedures it needs to happen first so
       jmp can go to the right place afterwards
    */
    emit(OINC, 0, FRAME + npargs[lexi] + n);
    statement();

    /* all the code in statement is done, so return */
    emit(OOPR, 0, ORET);

    if (lexi-- < 0)
    {
        error(32, lexi + 1);
        return;
    }
}

static void program(void)
{
    token();
    block();
    expect(periodsym, 9);
}

/* starts parsing */
int parse(void)
{
    nerr = 0;
    lexi = 0;

    memset(stab, 0, sizeof(stab));
    memset(npargs, 0, sizeof(npargs));

    program();
    if (nerr)
        return -1;

    return 0;
}
