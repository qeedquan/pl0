#include "dat.h"
#include "fns.h"

/* the vm data */
static Ins ir;
static int inslen;
static Ins ins[MAX_CODE_LENGTH];
static int stk[MAX_STACK_HEIGHT];

static int sp;
static int bp;
static int pc;
static int oldpc;

static int ar[MAX_STACK_HEIGHT];
static int lastar;

static int halt;

/* reset the vm fields, not the stack or code data,
   we want this to be able to run multiple runs if needed */
static void reset(void)
{
    sp = 0;
    bp = 1;
    pc = 0;
    oldpc = 0;

    memset(&ir, 0, sizeof(ir));

    memset(ar, 0, sizeof(ar));
    lastar = 0;

    halt = 0;
}

/* load instruction from a file, fails if the
   instruction file exceeds the instruction buffer */
void loadinsfile(char *file)
{
    FILE *fp;
    Ins *p;
    int i, j;

    fp = fopen(file, "r");
    if (!fp)
        die("%s: %s", file, strerror(errno));

    for (i = 0; i < MAX_CODE_LENGTH; i++)
    {
        p = &ins[i];
        if (fscanf(fp, "%d %d %d", &p->op, &p->l, &p->m) != 3)
            break;

        if (p->op <= 0 || p->l < 0)
            die("%s: invalid op: %d %d %d", p->op, p->l, p->m);
    }

    inslen = i;
    if (i == MAX_CODE_LENGTH)
    {
        if (fscanf(fp, "%d %d %d", &j, &j, &j) == 3)
            die("%s: max code length exceeded", file);

        inslen = i - 1;
    }

    reset();
    fclose(fp);
}

/* loads instruction from a buffer, this happens when
   we parse the source file */
void loadinsbuf(Ins *p, int len)
{
    if (len >= MAX_CODE_LENGTH)
        die("internal error: max code length exceeded");

    memset(ins, 0, sizeof(ins));
    memmove(ins, p, sizeof(ins[0]) * len);
    inslen = len;

    reset();
}

/* write instructions we generate, or read in to a file */
void writeinsfile(char *f)
{
    FILE *fp;
    int i;

    fp = fopen(f, "w");
    if (!fp)
    {
        fprintf(stderr, "%s: %s\n", f, strerror(errno));
        return;
    }

    for (i = 0; i < inslen; i++)
        fprintf(fp, "%d %d %d\n", ins[i].op, ins[i].l, ins[i].m);

    fclose(fp);
}

/* print instructions for running the vm */
static void printins(int which)
{
    static char *ops[] =
    {
        "",
        "lit", "opr", "lod", "sto", "cal",
        "inc", "jmp", "jpc", "sio", "sio",
        "lds"
    };

    Ins *p;
    int i, j;

    if (halt || !verbose)
        return;

    if (which == 0)
    {
        printf("Instruction listing\n\n");
        printf("Line\tOP\tL\tM\n");
        for (i = 0; i < inslen; i++)
        {
            p = &ins[i];
            if (p->op <= 0 || p->op >= nelem(ops))
                fprintf(stderr, "invalid opcode: %d %s %d %d\n", i, ops[p->op], p->l, p->m);
            else
                printf("%d\t%s\t%d\t%d\n", i, ops[p->op], p->l, p->m);
        }
        printf("\n\n");
        printf("\t\t\t\tpc\tbp\tsp\tstack\n");
        printf("Initial values\t\t\t%d\t%d\t%d\t%d\n", pc, bp, sp, 0);
    }
    else if (which == 1)
    {
        p = &ir;
        if (p->op <= 0 || p->op >= nelem(ops))
            fprintf(stderr, "invalid opcode: %d %d %d", p->op, p->l, p->m);
        else
            printf("%d\t%s   %d   %d      ", oldpc, ops[p->op], p->l, p->m);

        printf("\t%d\t%d\t%d\t", pc, bp, sp);
        if (sp == 0)
            j = 5;
        else
            j = max(lastar, sp);

        if (halt)
            j = 0;

        for (i = 1; i <= j; i++)
        {
            printf("%d ", stk[i]);
            if (ar[i] && (i != j))
                printf("| ");
        }
        printf("\n");
    }
}

static int sw(int v)
{
    return v & (MAX_STACK_HEIGHT-1);
}

static int pw(int v)
{
    return v & (MAX_CODE_LENGTH-1);
}

/* calculates the base */
static int base(int l, int b)
{
    int b1;

    b1 = b;
    while (l > 0)
    {
        b1 = sw(stk[sw(b1 + 1)]);
        l--;
    }
    return b1;
}

static int pop(void)
{
    int v;

    v = stk[sp];
    sp = sw(sp - 1);
    return v;
}

/* reads number from a user */
static int readnum(void)
{
    char buf[16], *p;
    int i, j, mul;

loop:
    printf("Enter a value to be placed on top of the stack: ");
    if (!fgets(buf, sizeof(buf), stdin))
    {
        fprintf(stderr, "Invalid input, try again\n");
        goto loop;
    }

    i = 0;
    while ((p = strchr(buf, '\n')) == NULL)
    {
        fgets(buf, sizeof(buf), stdin);
        i = 1;
    }
    if (i)
    {
        fprintf(stderr, "Input too long, try again\n");
        goto loop;
    }

    *p = '\0';
    if (buf[0] == '\0')
    {
        fprintf(stderr, "No input entered, try again\n");
        goto loop;
    }

    mul = 1;
    i = 0;
    while (buf[i] == '+' || buf[i] == '-')
    {
        mul = (buf[i] == '+') ? 1 : -1;
        i++;
    }
    p = &buf[i];

    j = 0;
    for (; buf[i] != '\0'; i++, j++)
    {
        if (j > MAX_DIGIT)
        {
            fprintf(stderr, "Input too long, enter a shorter number\n");
            goto loop;
        }

        if (!(('0' <= buf[i]) && (buf[i] <= '9')))
        {
            fprintf(stderr, "Input contains non-numbered characters, try again\n");
            goto loop;
        }
    }

    return atoi(p) * mul;
}

/* run the virtual machine until halt is reached,
   either when the bp is 0 or less or an invalid
   instruction happens, or any exception, such as dividing by 0
*/
void execute(void)
{
    int v;

    printins(0);
    while (!halt)
    {
        ir = ins[pc];
        oldpc = pc;
        pc = pw(pc + 1);
        switch (ir.op)
        {
            case 1: /* LIT 0, M */
                sp = sw(sp + 1);
                stk[sp] = ir.m;
                break;

            case 2: /* OPR 0, M */
                switch (ir.m)
                {
                    case 0: /* RET */
                        ar[sw(bp - 1)] = 0;

                        sp = sw(bp - 1);
                        pc = stk[sw(sp + 4)];
                        bp = stk[sw(sp + 3)];

                        lastar = sw(bp + FRAME);

                        if (sp <= 0)
                        {
                            lastar = 0;
                            printins(1);
                            halt = 1;
                        }

                        break;

                    case 1: /* NEG */
                        stk[sp] = -stk[sp];
                        break;

                    case 2: /* ADD */
                        v = pop();
                        stk[sp] += v;
                        break;

                    case 3: /* SUB */
                        v = pop();
                        stk[sp] -= v;
                        break;

                    case 4: /* MUL */
                        v = pop();
                        stk[sp] *= v;
                        break;

                    case 5: /* DIV */
                        v = pop();
                        if (v == 0)
                            die("vm: divide by 0");
                        stk[sp] /= v;
                        break;

                    case 6: /* ODD */
                        stk[sp] %= 2;
                        break;

                    case 7: /* MOD */
                        v = pop();
                        if (v == 0)
                            die("vm: mod by 0");
                        stk[sp] %= v;
                        break;

                    case 8: /* EQL */
                        v = pop();
                        stk[sp] = (stk[sp] == v);
                        break;

                    case 9: /* NEQ */
                        v = pop();
                        stk[sp] = (stk[sp] != v);
                        break;

                    case 10: /* LSS */
                        v = pop();
                        stk[sp] = (stk[sp] < v);
                        break;

                    case 11: /* LEQ */
                        v = pop();
                        stk[sp] = (stk[sp] <= v);
                        break;

                    case 12: /* GTR */
                        v = pop();
                        stk[sp] = (stk[sp] > v);
                        break;

                    case 13: /* GEQ */
                        v = pop();
                        stk[sp] = (stk[sp] >= v);
                        break;

                    default:
                        fprintf(stderr, "vm: unknown instruction: OP: %d L: %d M: %d\n", ir.op, ir.l, ir.m);
                        halt = 1;
                        break;
                }
                break;

            case 3: /* LOD L, M */
                sp = sw(sp + 1);
                stk[sp] = stk[sw(base(ir.l, bp) + ir.m)];
                break;

            case 4: /* STO L, M */
                stk[sw(base(ir.l, bp) + ir.m)] = stk[sp];
                sp = sw(sp - 1);
                break;

            case 5: /* CAL L, M */
                stk[sw(sp + 1)] = 0;
                stk[sw(sp + 2)] = base(ir.l, bp);
                stk[sw(sp + 3)] = bp;
                stk[sw(sp + 4)] = pc;
                bp = sw(sp + 1);
                pc = pw(ir.m);

                if (bp <= 0)
                    halt = 1;

                ar[sw(bp - 1)] = 1;
                lastar = sw(sp + FRAME);
                break;

            case 6: /* INC 0, M */
                sp = sw(sp + ir.m);
                break;

            case 7: /* JMP 0, M */
                pc = pw(ir.m);
                break;

            case 8: /* JPC 0, M */
                if (stk[sp] == 0)
                    pc = pw(ir.m);
                sp = sw(sp - 1);

                break;

            case 9: /* SIO 0, 1 */
                printf("Value on top of the stack: %d\n", stk[sp]);
                sp = sw(sp - 1);
                break;

            case 10: /* SIO 0, 2 */
                sp = sw(sp + 1);
                v = readnum();
                stk[sp] = v;
                break;

            case 11: /* LDS 0, M */
                stk[sw(sp + ir.m)] = stk[sp];
                sp = sw(sp - 1);
                break;

            default:
                fprintf(stderr, "vm: unknown instruction: OP: %d L: %d M: %d\n", ir.op, ir.l, ir.m);
                halt = 1;
                break;
        }

        printins(1);
    }
}
