#ifndef NDEBUG
#define DEBUG(text) mexPrintf("%s:%d - %s\n",__FILE__,__LINE__,text);	// driver actions

#define MEXMESSAGE(error) mexMessage(__FILE__,__LINE__,error)
#define MEXENTER mexEnter(__FILE__,__LINE__)
#else
#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG(text)

#define MEXMESSAGE(error) mexMessage(error)
#define MEXENTER mexEnter()
#endif
#define MEXLEAVE mexLeave()

const WORD StringLength = 128;
#define MAXNUMDEVS 100

#define VALUE	plhs[0]

typedef struct
{
	int	BoardNum;			// device handle structure
	int numao;
	int numdio;
	int* range;
	float* aomin;
	float* aomax;
	bool* line_out;			// the DIO line is configured for output
} redlabState;

extern int redlab, redlabs;		// device handles
extern redlabState* redlabStates;

#ifndef NDEBUG
void mexEnter(const char* file, const int line);
void mexMessage(const char* file, const int line, int error);
#else
void mexEnter(void);
void mexMessage(ViStatus error);
#endif
void mexLeave(void);
void mexCleanup(void);

double	getScalar(const mxArray* array);
mxArray*	getParameter(const char* name);
mxArray*	getParameter2(const char* name, const mxArray* field);
void	setParameter(const char* name, const mxArray* field);
