/* all the function prototypes */

#define nelem(x)  ((int)(sizeof(x)/sizeof((x)[0])))
#define max(a, b) ((a) > (b) ? (a) : (b))

void     *emalloc      (size_t);
void      die          (char*, ...);

void      loadinsfile  (char*);
void      loadinsbuf   (Ins*, int);
void      writeinsfile (char*);
void      execute      (void);

void      newfile      (char*);
int       lex          (void);
void      printfile    (char*);
int       tokenize     (char*);
void      token        (void);
void      pushtoken    (Token*);

int       parse        (void);

void      pushcall     (Sym*);
void      pushproc     (Sym*);
void      popproc      (void);
void      compile      (char*);
void      emit         (int, int, int);
