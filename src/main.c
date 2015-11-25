#include "dat.h"
#include "fns.h"

/* input and output filenames to be during runtime */
static char *input;
static char *codeoutput = "output.txt";

/* flags */
static int vmfile;
static int dumpcode;

int lexonly;
int verbose;

static void usage(void)
{
    fprintf(stderr, "usage: [-dhlp] input [output]\n");
    fprintf(stderr, "\t-d: dump the generated code to the [output] file, default file used is %s\n", codeoutput);
    fprintf(stderr, "\t-h: print this usage\n");
    fprintf(stderr, "\t-l: only lex, don't parse or execute code\n");
    fprintf(stderr, "\t-p: execute input as if it was a instruction file and not pl0 source\n");
    fprintf(stderr, "\t-v: be verbose (output every stage of the compilation while running the program)\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    int i;

    /* parse arguments */
    while (argc > 2)
    {
        if (argv[1][0] != '-')
            break;

        for (i = 1; argv[1][i] != '\0'; i++)
        {
            switch (argv[1][i])
            {
                case 'p':
                    vmfile = 1;
                    break;

                case 'l':
                    lexonly = 1;
                    verbose = 1;
                    break;

                case 'd':
                    dumpcode = 1;
                    break;
                
                case 'v':
                    verbose = 1;
                    break;

                case 'h':
                default:
                    usage();
                    break;
            }
        }

        argc--;
        argv++;
    }

    if (argc < 2)
        usage();

    input = argv[1];

    /* load instruction file or source file */
    if (vmfile)
        loadinsfile(input);
    else
        compile(input);

    /* write code to output if needed */
    if (argc >= 3)
        codeoutput = argv[2];

    if (dumpcode)
        writeinsfile(codeoutput);

    /* execute the vm */
    execute();

    return 0;
}
