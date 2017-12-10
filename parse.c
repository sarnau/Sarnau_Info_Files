/***
 *	a very simple top-down-parser for terms
 *
 *	(c)1995-2006 Sigma-Soft, Markus Fritze
 ***/

#include "parse.h"
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

typedef double	(*funcPtr)(...);

typedef struct {
	char	*name;
	short	pcount;		// number of parameters for that function (0 = variable)
	funcPtr	func;
} FuncStruct;

static int	SearchFunc(const void *name, const void *f)
{
	return strcmp((const char *)name, ((const FuncStruct *)f)->name);
}

// our own functions
static double	get_pi(...) { return M_PI; }
static double	get_e(...) { return exp(1); }
double			var[3];
static double	get_1(...) { return var[0]; }
static double	get_2(...) { return var[1]; }
static double	get_3(...) { return var[2]; }

FuncStruct	FuncTable[] = {	// has to be sorted, because we use a binary search
	{"ACOS", 1, (funcPtr)acos},
	{"ARCCOS", 1, (funcPtr)acos},
	{"ARCSIN", 1, (funcPtr)asin},
	{"ARCTAN", 1, (funcPtr)atan},
	{"ARCTAN2", 2, (funcPtr)atan2},
	{"ASIN", 1, (funcPtr)asin},
	{"ATAN", 1, (funcPtr)atan},
	{"ATAN2", 2, (funcPtr)atan2},
	{"CEIL", 1, (funcPtr)ceil},
	{"COS", 1, (funcPtr)cos},
	{"COSH", 1, (funcPtr)cosh},
	{"E", 0, (funcPtr)get_e},
	{"EXP", 1, (funcPtr)exp},
	{"FLOOR", 1, (funcPtr)floor},
	{"FMOD", 2, (funcPtr)fmod},
	{"LOG", 1, (funcPtr)log},
	{"LOG10", 1, (funcPtr)log10},
	{"PI", 0, (funcPtr)get_pi},
	{"POW", 2, (funcPtr)pow},
	{"SIN", 1, (funcPtr)sin},
	{"SINH", 1, (funcPtr)sinh},
	{"SQRT", 1, (funcPtr)sqrt},
	{"TAN", 1, (funcPtr)tan},
	{"TANH", 1, (funcPtr)tanh},
	{"X", 0, (funcPtr)get_1},
	{"Y", 0, (funcPtr)get_2},
	{"Z", 0, (funcPtr)get_3}
};

// skip a token in the input stream
#if 1
#define	SkipToken(p)	(*(p))++
#else
static void	SkipToken(char **p)
{
	(*p)++;
}
#endif

// get the next token, ignore spaces
static char	GetAktToken(char **p)
{
char	c = **p;

	/* Leerzeichen, etc. ignorieren */
	if(isspace(c)) {
		while(isspace(c)) {
			SkipToken(p);
			c = **p;
		}
	}
	return c;
}

// parse a floating point number
static double	GetNumTokenSub(char **p)
{
	/* numerische Konstante */
	double		a = 0.0;
	while(isdigit(GetAktToken(p))) {
		a *= 10;
		a += GetAktToken(p) - '0';
		SkipToken(p);
	}
	if(GetAktToken(p) == '.') {
		double		b = 0.0;
		double		i = 10.0;
		SkipToken(p);
		while(isdigit(GetAktToken(p))) {
			b += (double)(GetAktToken(p) - '0') / i;
			i *= 10;
			SkipToken(p);
		}
		a += b;
	}
	return a;
}

// parse a floating point number with an optional exponent
static double	GetNumToken(char **p)
{
	// numeric constant
	double		a = GetNumTokenSub(p);
	if(toupper(GetAktToken(p)) == 'E') {
		double		b;
		short	signum = 1;
		SkipToken(p);
		if(GetAktToken(p) == '-') {	// neg. sign in front of the exponent
			SkipToken(p);
			signum = -1;
		} else if(GetAktToken(p) == '+')
			SkipToken(p);			// ignore a positive sign
		b = GetNumTokenSub(p);
		a *= pow(10,b * signum);
	}
	return a;
}

#if TERM_COMPILER
#define MAX_STACK_SIZE		200

static void	calcTop(char **p);

typedef struct {
	short	pcount;		// <0 => value, >=0 => function with n parameters
	union {
		double		val;
		double		(*func)(...);
	} p;
} FuncStack,*FuncStackPtr;

FuncStack		FS[MAX_STACK_SIZE];
FuncStackPtr	FSP;

// for the term parser all operations have to be functions!
static double	Pf_neg(double a) { return -a; }
static double	Pf_mult(double a, double b) { return a * b; }
static double	Pf_div(double a, double b) { return a / b; }
static double	Pf_add(double a, double b) { return a + b; }
static double	Pf_sub(double a, double b) { return a - b; }

static void	calcVar(char **p)
{
char	c = GetAktToken(p);

	if(c == '(') {
		SkipToken(p);
		calcTop(p);
		assert(GetAktToken(p) == ')');
		SkipToken(p);
	} else if(isdigit(c)) {
		// numeric constant
		FSP->pcount = -1;
		FSP->p.val = GetNumToken(p);
		FSP++;
	} else if(isalpha(c)) {
		// variable or function call
		char		buf[256];
		char		*sp = buf;
		FuncStruct	*f;
		while(isalnum(GetAktToken(p))) {
			*sp++ = toupper(GetAktToken(p));
			SkipToken(p);
		}
		*sp++ = 0;
		f = (FuncStruct*)bsearch(buf, FuncTable, sizeof(FuncTable)/sizeof(FuncStruct),sizeof(FuncStruct), SearchFunc);
		assert(f);
		switch(f->pcount) {
		case 0:	break;
		case 1:	assert(GetAktToken(p) == '('); SkipToken(p);
				calcTop(p);
				assert(GetAktToken(p) == ')');
				SkipToken(p);
				break;
		case 2:	assert(GetAktToken(p) == '('); SkipToken(p);
				calcTop(p);
				assert(GetAktToken(p) == ','); SkipToken(p);
				calcTop(p);
				assert(GetAktToken(p) == ')');
				SkipToken(p);
				break;
		}
		FSP->pcount = f->pcount;
		FSP->p.func = f->func;
		FSP++;
	}
}

static void	calcVorz(char **p)
{
	switch(GetAktToken(p)) {
	case '+':	SkipToken(p);	// ignore positive sign
	default:	calcVar(p);
				return;
	case '-':	SkipToken(p);
				calcVar(p);
				FSP->pcount = 1;
				FSP->p.func = (funcPtr)Pf_neg;
				FSP++;
				return;
	}
}

static void	calcPot(char **p)
{
	calcVorz(p);
	do {
		switch(GetAktToken(p)) {
		case '^':	SkipToken(p);
					calcVorz(p);
					FSP->pcount = 2;
					FSP->p.func = (funcPtr)pow;
					FSP++;
					break;
		default:	return;
		}
	} while(1);
}

static void	calcMult(char **p)
{
	calcPot(p);
	do {
		switch(GetAktToken(p)) {
		case '*':	SkipToken(p);
					calcPot(p);
					FSP->pcount = 2;
					FSP->p.func = (funcPtr)Pf_mult;
					FSP++;
					break;
		case '/':	SkipToken(p);
					calcPot(p);
					FSP->pcount = 2;
					FSP->p.func = (funcPtr)Pf_div;
					FSP++;
					break;
		default:	return;
		}
	} while(1);
}

static void	calcAdd(char **p)
{
	calcMult(p);
	do {
		switch(GetAktToken(p)) {
		case '+':	SkipToken(p);
					calcMult(p);
					FSP->pcount = 2;
					FSP->p.func = (funcPtr)Pf_add;
					FSP++;
					break;
		case '-':	SkipToken(p);
					calcMult(p);
					FSP->pcount = 2;
					FSP->p.func = (funcPtr)Pf_sub;
					FSP++;
					break;
		default:	return;
		}
	} while(1);
}

static void	calcTop(char **p)
{
	calcAdd(p);
}

void		buildTerm(char *p)
{
	FSP = &FS[0];

	calcTop(&p);

	// parsed the all of the input?
	assert(GetAktToken(&p) == 0);
}

double			calcTerm3(char *, double param1, double param2, double param3)
{
double			stk[MAX_STACK_SIZE];
short			sp = 0;
FuncStackPtr	FSrd;

	var[0] = param1;
	var[1] = param2;
	var[2] = param3;

	for(FSrd = &FS[0]; FSrd < FSP; ++FSrd) {
		switch(FSrd->pcount) {
		case -1:stk[sp++] = FSrd->p.val;
				break;
		case 0:	stk[sp++] = (FSrd->p.func)();
				break;
		case 1:	assert(sp >= 1);
				stk[sp - 1] = (FSrd->p.func)(stk[sp - 1]);
				break;
		case 2:	assert(sp >= 2);
				stk[sp - 2] = (FSrd->p.func)(stk[sp - 2], stk[sp - 1]);
				sp--;
				break;
		}
	}
	// to avoid a -0.0
	if(stk[0] == 0.0) stk[0] = 0.0;

	// the result has to be the only element on the stack!
	assert(sp == 1);

	return stk[0];
}
#else
static double	calcTop(char **p);

static double	calcVar(char **p)
{
double	a = 0.0;
char	c = GetAktToken(p);

	if(c == '(') {
		SkipToken(p);
		a = calcTop(p);
		assert(GetAktToken(p) == ')');
		SkipToken(p);
	} else if(isdigit(c)) {
		// numeric constant
		a = GetNumToken(p);
	} else if(isalpha(c)) {
		// variable or function call
		char		buf[256];
		char		*sp = buf;
		FuncStruct	*f;
		while(isalnum(GetAktToken(p))) {
			*sp++ = toupper(GetAktToken(p));
			SkipToken(p);
		}
		*sp++ = 0;
		f = (FuncStruct*)bsearch(buf, FuncTable, sizeof(FuncTable)/sizeof(FuncStruct),sizeof(FuncStruct), SearchFunc);
		assert(f);
		switch(f->pcount) {
		case 0:	a = (f->func)();
				break;
		case 1:	assert(GetAktToken(p) == '('); SkipToken(p);
				a = (f->func)(calcTop(p));
				assert(GetAktToken(p) == ')');
				SkipToken(p);
				break;
		case 2:	assert(GetAktToken(p) == '('); SkipToken(p);
				a = calcTop(p);
				assert(GetAktToken(p) == ','); SkipToken(p);
				a = (f->func)(a, calcTop(p));
				assert(GetAktToken(p) == ')');
				SkipToken(p);
				break;
		}
	}
	return a;
}

static double	calcVorz(char **p)
{
	switch(GetAktToken(p)) {
	case '+':	SkipToken(p);	// ignore positive sign
	default:	return calcVar(p);
	case '-':	SkipToken(p);
				return -calcVar(p);
	}
}

static double	calcPot(char **p)
{
double		a = calcVorz(p);

	do {
		switch(GetAktToken(p)) {
		case '^':	SkipToken(p);
					a = pow(a, calcVorz(p));
					break;
		default:	return a;
		}
	} while(1);
}

static double	calcMult(char **p)
{
double		a = calcPot(p);

	do {
		switch(GetAktToken(p)) {
		case '*':	SkipToken(p);
					a *= calcPot(p);
					break;
		case '/':	SkipToken(p);
					a /= calcPot(p);
					break;
		default:	return a;
		}
	} while(1);
}

static double	calcAdd(char **p)
{
double		a = calcMult(p);

	do {
		switch(GetAktToken(p)) {
		case '+':	SkipToken(p);
					a += calcMult(p);
					break;
		case '-':	SkipToken(p);
					a -= calcMult(p);
					break;
		default:	return a;
		}
	} while(1);
}

static double	calcTop(char **p)
{
	return calcAdd(p);
}

double			calcTerm3(char *p, double param1, double param2, double param3)
{
double		a;

	var[0] = param1;
	var[1] = param2;
	var[2] = param3;

	a = calcTop(&p);

	// to avoid a -0.0
	if(a == 0.0) a = 0.0;

	// parsed the all of the input?
	assert(GetAktToken(&p) == 0);

	return a;
}
#endif
