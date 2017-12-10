// This allows us to compile a term. The precompiled term
// can be processed _much_ faster and this is nice if the
// same term (with the variables X,Y and Z) has to be calculated
// many times
#define TERM_COMPILER	1
 
// up to 3 variable are supported (X,Y und Z)
#define calcTerm2(p,p1,p2)	calcTerm3((p),(p1),(p2),HUGE_VAL)
#define calcTerm1(p,p1)		calcTerm3((p),(p1),HUGE_VAL,HUGE_VAL)
#define calcTerm(p)			calcTerm3((p),HUGE_VAL,HUGE_VAL,HUGE_VAL)
 
#if TERM_COMPILER
extern void		buildTerm(char *p);
extern double	calcTerm3(char *, double param1, double param2, double param3);
 
#else
#define buildTerm(p)
extern double		calcTerm3(char *p, double param1, double param2, double param3);
#endif
