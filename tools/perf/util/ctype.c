#include "util.h"

enum {
	S = GIT_SPACE,
	A = GIT_ALPHA,
	D = GIT_DIGIT,
	G = GIT_GLOB_SPECIAL,	
	R = GIT_REGEX_SPECIAL,	
	P = GIT_PRINT_EXTRA,	

	PS = GIT_SPACE | GIT_PRINT_EXTRA,
};

unsigned char sane_ctype[256] = {

	0, 0, 0, 0, 0, 0, 0, 0, 0, S, S, 0, 0, S, 0, 0,		
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		
	PS,P, P, P, R, P, P, P, R, R, G, R, P, P, R, P,		
	D, D, D, D, D, D, D, D, D, D, P, P, P, P, P, G,		
	P, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A,		
	A, A, A, A, A, A, A, A, A, A, A, G, G, P, R, P,		
	P, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A,		
	A, A, A, A, A, A, A, A, A, A, A, R, R, P, P, 0,		
	
};

const char *graph_line =
	"_____________________________________________________________________"
	"_____________________________________________________________________";
const char *graph_dotted_line =
	"---------------------------------------------------------------------"
	"---------------------------------------------------------------------"
	"---------------------------------------------------------------------";
