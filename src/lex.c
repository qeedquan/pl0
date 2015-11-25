#include "dat.h"
#include "fns.h"

typedef struct Word Word;
typedef struct List List;

/* for declaring symbols to match 
   and the tokens for them
 */
struct Word
{
    char *name;
    int   token;
};

/* a pool list for storing tokens */
struct List
{
    Token tokens[256];
    int   len;
    int   cap;
    List *next;
};

/* this is used for parsing later */
long pos;
long line;
int  tok;
int  lval;
char symb[WBUF];

/* filename we are reading as source code input */
char *ion;

/* the current token, for parsing */
Token *curtok;

/* stores all the tokens
   since lexing is a separate pass 
*/
static List  *lp;
static List  *list;

/* this is a buffer that allows us to inject tokens on
   the fly while we are parsing, to allow us to lookaheads
   for parsing hacks, etc. if this is empty, it reads from
   the list above for tokens
*/
static Token *tstk[4];
static int    tlen;

/* current file we are reading */
static FILE *iof;

/* line information to get better diagnostics */
static long lfcol;
static long fcol;
static long lnline;
static long nline;

/* work buffer for lexing */
static int  ch;
static char wbuf[WBUF];
static int  wpos;

static char *symbolic[] =
{
    "eofsym", "nulsym", "identsym", "numbersym", "plussym", "minussym",
    "multsym", "slashsym", "oddsym", "eqsym", "neqsym", "lessym", "leqsym",
    "gtrsym", "geqsym", "lparentsym", "rparentsym", "commasym", "semicolonsym",
    "periodsym", "becomessym", "beginsym", "endsym", "ifsym", "thensym",
    "whilesym", "dosym", "callsym", "constsym", "intsym", "procsym", "writesym",
    "readsym", "elsesym", "errorsym"
};

/* one character symbols from grammar */
static Word sym1[] =
{
    {"+",  plussym},
    {"-",  minussym},
    {"*",  multsym},
    {"(",  lparentsym},
    {")",  rparentsym},
    {"=",  eqsym},
    {",",  commasym},
    {".",  periodsym},
    {"<",  lessym},
    {">",  gtrsym},
    {";",  semicolonsym}
};

/* two character symbols from grammar */
static Word sym2[] =
{
    {"<=", leqsym},
    {">=", geqsym},
    {"<>", neqsym},
    {":=", becomessym}
};

/* the grammar keywords */
static Word keywords[] =
{
    {"const",     constsym},
    {"int",       intsym},
    {"procedure", procsym},
    {"call",      callsym},
    {"begin",     beginsym},
    {"end",       endsym},
    {"if",        ifsym},
    {"then",      thensym},
    {"else",      elsesym},
    {"while",     whilesym},
    {"do",        dosym},
    {"odd",       oddsym},
    {"read",      readsym},
    {"write",     writesym}
};

/* prints lexing errors */
static void lexerror(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "lex: %s:%ld:%ld: ", ion, lnline, lfcol);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* lookup from a table to see if a keyword exist */
static int lookup(char *s, Word *tab, unsigned len)
{
    unsigned i;

    for (i = 0; i < len; i++)
    {
        if (strcmp(s, tab[i].name) == 0)
            return tab[i].token;
    }
    return 0;
}

/* get a character */
static void getch(void)
{
    ch = fgetc(iof);
    if (errno)
    {
        lexerror("%s\n", strerror(errno));
        exit(1);
    }

    fcol++;
    if (ch == '\n')
    {
        fcol = 1;
        nline++;
    }
}

/* put current character into work buffer */
static void putch(void)
{
    if (wpos >= WBUF)
        die("internal error: wbuf overflowed");

    wbuf[wpos++] = ch;
}

/* reads in a new file for lexing */
void newfile(char *f)
{
    if (iof)
        fclose(iof);

    ion = f;
    iof = fopen(f, "rb");
    if (!iof)
        die("%s: %s", ion, strerror(errno));

    tlen = 0;
    lfcol = fcol = 1;
    lnline = nline = 1;

    getch();
}

/* skip whitespaces and non-printable characters */
static void skipch(void)
{
    while ((isspace(ch) || !isprint(ch)) && ch != EOF)
        getch();
}

/* skip comments */
static int skipcom(void)
{
    for (;;)
    {
        getch();
        if (ch == EOF)
        {
            lexerror("unterminated comment\n");
            return -1;
        }
        if (ch == '*')
        {
            getch();
            if (ch == '/')
            {
                getch();
                break;
            }
        }
    }
    return 0;
}

/* gets an identifier */
static int getword(void)
{
    do
    {
        if (wpos >= MAX_IDENT)
        {
            wbuf[wpos] = '\0';
            lexerror("max ident is: %d, ident too long: '%s", MAX_IDENT, wbuf);
            do
            {
                fputc(ch, stderr);
                getch();
            } while (isalpha(ch) || isdigit(ch));
            fprintf(stderr, "'\n");

            return -1;
        }

        putch();
        getch();
    } while(isalpha(ch) || isdigit(ch));

    wbuf[wpos] = '\0';

    return 0;
}

/* helper to print identifier with numbers beginning of them error */
static void numidenterror(void)
{
    lexerror("identifier should not start with a digit: '%s", wbuf);
    while (isalpha(ch) || isdigit(ch))
    {
        fputc(ch, stderr);
        getch();
    }
    fprintf(stderr, "'\n");
}

/* gets a number */
static int getnum(void)
{
    long p;

    do
    {
        putch();

        /* this is ambiguous, we have to figure out if the error
           is from having a long invalid ident, or a long number */
        if (wpos > MAX_DIGIT)
        {
            wbuf[wpos] = '\0';
            p = ftell(iof);
            for (;;)
            {
                getch();

                if (isdigit(ch))
                    continue;

                fseek(iof, p, SEEK_SET);
                if (isalpha(ch))
                {
                    getch();
                    numidenterror();
                    return -1;
                }

                getch();
                lexerror("max number is %d digits, number too long: '%s", MAX_DIGIT, wbuf);
                while (isdigit(ch))
                {
                    fputc(ch, stderr);
                    getch();
                }
                fprintf(stderr, "'\n");
                return -1;
            }
        }

        getch();
    } while (isdigit(ch));

    wbuf[wpos] = '\0';

    if (isalpha(ch))
    {
        numidenterror();
        return -1;
    }

    return 0;
}

/* it reads in the file and return one token at a time 
   it always lookahead one character
 */
int lex(void)
{
    char buf[3];
    int t;

loop:
    skipch();

    lfcol = fcol;
    lnline = nline;

    if (ch == EOF)
        return eofsym;

    /* check for comment */
    if (ch == '/')
    {
        getch();
        if (ch != '*')
            return slashsym;

        if (skipcom() < 0)
            return errorsym;

        goto loop;
    }

    /* identifier */
    if (isalpha(ch))
    {
        if (getword() < 0)
        {
            wpos = 0;
            return errorsym;
        }

        t = lookup(wbuf, keywords, nelem(keywords));
        wpos = 0;

        if (t)
            return t;

        snprintf(symb, WBUF, "%s", wbuf);
        return identsym;
    }

    /* number */
    if (isdigit(ch))
    {
        if (getnum() < 0)
        {
            wpos = 0;
            return errorsym;
        }

        wpos = 0;
        snprintf(symb, WBUF, "%s", wbuf);
        return numbersym;
    }

    /* symbols */
    buf[0] = ch;
    getch();

    buf[1] = ch;
    buf[2] = '\0';
    t = lookup(buf, sym2, nelem(sym2));
    if (t)
    {
        getch();
        return t;
    }

    buf[1] = '\0';
    t = lookup(buf, sym1, nelem(sym1));
    if (t)
        return t;

    /* unknown symbol */
    lexerror("unknown character: '%c'\n", buf[0]);
    return errorsym;
}

/* add a token read into the list */
static void pushlist(int type, char *symb, long pos, long line)
{
    Token *t;

    if (lp->cap == nelem(lp->tokens))
    {
        lp->next = emalloc(sizeof(List));
        lp = lp->next;
    }

    t = &lp->tokens[lp->cap++];
    t->type = type;
    t->pos = pos;
    t->line = line;
    if (symb)
    {
        snprintf(t->symb, WBUF, "%s", symb);
        if (type == numbersym)
            t->lval = atoi(symb);
    }
}

/* print the lexemes we read from the file */
static int printlexemes(int symbolicrep)
{
    Token *tp;
    int i, rc;

    rc = 0;
    i = 0;
    lp = list;
    for (;;)
    {
        if (i >= lp->cap)
        {
            if (!lp->next)
                break;

            lp = lp->next;
            i = 0;
            continue;
        }

        tp = &lp->tokens[i++];
        if (tp->type == eofsym)
            continue;

        if (tp->type == errorsym)
            rc = -1;

        if (symbolicrep == 0)
            printf("%d ", tp->type);
        else
            printf("%s ", symbolic[tp->type]);

        if (tp->type == identsym || tp->type == numbersym)
            printf("%s ", tp->symb);
    }
    printf("\n\n");

    return rc;
}

/* tokenizes the file, do this before we parse */
int tokenize(char *f)
{
    int rc, t;
    char *s;

    rc = 0;
    newfile(f);
    if (!list)
        list = emalloc(sizeof(List));

    for (lp = list; lp; lp = lp->next)
        lp->cap = 0;
    lp = list;
    
    for (;;)
    {
        t = lex();
        s = NULL;
        if (t == identsym || t == numbersym)
            s = symb;
        pushlist(t, s, lfcol, lnline);

        if (t == eofsym)
            break;
    }

    if (verbose)
    {
        printf("\nLexeme List:\n");
        printlexemes(0);
        printf("Symbolic Representation:\n");
        rc = printlexemes(1);
    }
    lp = list;

    return rc;
}

/* for parsing, returns the next token */
void token(void)
{
    Token *tp;

    if (tlen > 0)
        tp = tstk[--tlen];
    else
    {
        if (!lp)
        {
            curtok = NULL;
            return;
        }

        tp = &lp->tokens[lp->len++];
        if (lp->len >= lp->cap)
            lp = lp->next;
    }

    tok = tp->type;
    pos = tp->pos;
    line = tp->line;
    snprintf(symb, WBUF, "%s", tp->symb);
    if (tok == numbersym)
        lval = atoi(symb);

    curtok = tp;
}

/* helper to allow us to push arbitary tokens for reading 
   when pushed, they get read in a LIFO manner, if it is empty
   the token function reads from the list
 */
void pushtoken(Token *t)
{
    if (tlen >= nelem(tstk))
        die("internal error: token stack size exceeded");

    tstk[tlen++] = t;
}
