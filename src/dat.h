/* all the data declarations */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

/* max sizes for things, a more advance compiler
   would be able to generate arbitarily
   long code but this works for a toy language 
   must be to the power of 2
*/
#define MAX_STACK_HEIGHT (256*1024)
#define MAX_CODE_LENGTH  (1024*1024)
#define MAX_LEXI_LEVEL   5

#define MAX_IDENT 11
#define MAX_DIGIT 5

/* it used to be we allow the parser
   to continue upon first error like lexing,
   but all the state for code gen gets messy on fail,
   I kept the code such that it should work
   if this is >1, but it the next error messages would
   be generally not apply because the failures waterfalled
   from the first one
*/
#define MAX_PARSER_ERROR 1

#define SYMTAB MAX_STACK_HEIGHT
#define WBUF   16

/* the frame registers */
enum
{
    RA    = 0,
    FRAME = 4
};

typedef struct Ins Ins;
typedef struct Token Token;
typedef struct Sym Sym;
typedef struct Symtab Symtab;

/* instruction format */
struct Ins
{
    int op, l, m;
};

/* a token */
struct Token
{
    int  type;
    int  lval;
    long pos;
    long line;
    char symb[WBUF];
};

/* a symbol for looking up things during parsing */
struct Sym
{
    char name[WBUF];
    int  type;
    int  narg;
    int  lval;
    int  level;
    int  addr;
};

/* symbol table */
struct Symtab
{
    Sym sym[SYMTAB];
    int len;
};

/* all the keywords for the grammars, there are some special 
   ones like error and eof, which is used to stop or error out 
*/
enum
{
    eofsym, nulsym, identsym, numbersym, plussym, minussym, 
    multsym, slashsym, oddsym, eqsym, neqsym, lessym, 
    leqsym, gtrsym, geqsym, lparentsym, rparentsym, commasym, 
    semicolonsym, periodsym, becomessym, beginsym, endsym, ifsym, 
    thensym, whilesym, dosym, callsym, constsym, intsym, procsym, 
    writesym, readsym, elsesym, errorsym
};

/* all the different types the language support */
enum
{
    symconst, symint, symproc
};

/* all the instructions for the VM */
enum
{
    OLIT = 1, OOPR, OLOD, OSTO, OCAL, OINC, OJMP, OJPC, OSIO1, OSIO2, OLDS
};

/* the OPR M field */
enum
{
    ORET, ONEG, OADD, OSUB, OMUL, ODIV, OODD, 
    OMOD, OEQL, ONEQ, OLSS, OLEQ, OGTR, OGEQ
};

/* shared globals for the program */
extern int lexonly;
extern int verbose;

extern long pos;
extern long line;
extern int  tok;
extern int  lval;
extern char symb[WBUF];

extern char *ion;

extern Token *curtok;

extern Ins  code[MAX_CODE_LENGTH];
extern int  codepos;
