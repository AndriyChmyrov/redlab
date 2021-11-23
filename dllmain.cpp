// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "redlab.h"

/*	Driver mutually exclusive access lock (SEMAPHORE).
The semaphore is granted by the mexEnter() function. The owner of the
semaphore must release it with mexLeave().
*/
static HANDLE driver;


/*	Handle of the DLL module itself.
*/
static HANDLE self;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	UNREFERENCED_PARAMETER(lpReserved);
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		self = hModule;
		driver = CreateSemaphore(NULL, 1, 1, NULL);
		return driver != NULL;
	case DLL_THREAD_ATTACH:
		self = hModule;
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

/*	Request exclusive access.

This function uses a semaphore for granting mutually exclusive access to a
code section between mexEnter()/mexLeave(). If the semaphore is locked, the
function waits up to 10ms before giving up.

The semaphore is created once upon initialization of the library and persists
until the library is unloaded. Code that may raise an exception or lead to a
MEX abort on error must not execute within a mexEnter()/mexLeave section,
otherwise the access may lock permanently.
*/
#ifndef NDEBUG
void mexEnter(const char* file, const int line)
#else
void mexEnter(void)
#endif
{
	switch (WaitForSingleObject(driver, 10))
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:					// access granted
		return;
	default:
#ifndef NDEBUG
		mexPrintf("%s:%d - ", file, line);
#endif
		mexErrMsgTxt("Locked.");
	}
}


/*	Release exclusive access.
*/
void mexLeave(void)
{
	ReleaseSemaphore(driver, 1, NULL);
}


/*	Check for an error and print the message.
This function is save within a mexEnter()/mexLeave section.
*/
#ifndef NDEBUG
void mexMessage(const char* file, const int line, int error)
#else
void mexMessage(ViStatus error)
#endif
{
	if (error > NOERRORS)
	{
		MEXLEAVE;
#ifndef NDEBUG
		mexPrintf("%s:%d ", file, line);
#endif
		int error_;
		char ErrMessage[ERRSTRLEN];
		error_ = cbGetErrMsg(error, ErrMessage);
		mexErrMsgTxt(ErrMessage);
	}
}


/*	Free the powermeter and the driver.
*/
void mexCleanup(void)
{
	if (redlabs >= 0)
	{
		MEXENTER;
		while (redlabs)
		{
			redlab = redlabs;
			// reset all VOUTs to 0
			for (int n = 0; n < redlabStates[redlab - 1].numao; n++)
			{
				MEXMESSAGE(cbVOut(redlabStates[redlab - 1].BoardNum, n, redlabStates[redlab - 1].range[n], 0, DEFAULTOPTION));
			}
			mxFree(redlabStates[redlab - 1].range);
			mxFree(redlabStates[redlab - 1].line_out);
			mxFree(redlabStates[redlab - 1].aomin);
			mxFree(redlabStates[redlab - 1].aomax);
			redlabs--;
		}
		MEXLEAVE;
#ifndef NDEBUG
		mexPrintf("MeilhausElectronics DAQ connection shutted down!\n");
#endif
		if (redlabStates)
		{
			mxFree(redlabStates);
			redlabStates = NULL;
		}
	}
}


/*	Initialize driver
*/
int	 mexStartup(void)
{
	mexAtExit(mexCleanup);

	int UDStat = 0;

	float redlab_rev_level = (float)CURRENTREVNUM;
	MEXMESSAGE(cbDeclareRevision(&redlab_rev_level));
	MEXMESSAGE(cbErrHandling(DONTPRINT, DONTSTOP));

	int ret;
	int type;
	ret = cbGetConfig(BOARDINFO, 0, 0, BIBOARDTYPE, &type);
	if (ret == 126)
	{
		char s_err[ERRSTRLEN];
		cbGetErrMsg(ret, s_err);
		char err_long[2 * ERRSTRLEN];
		sprintf_s(err_long, 2 * ERRSTRLEN, "%s\nTry (re-)installing RedLab drivers and run InstaCal.", s_err);
		mexErrMsgTxt(err_long);
	}
	
	int numberOfDevices = MAXNUMDEVS;
	DaqDeviceDescriptor inventory[MAXNUMDEVS];
	UDStat = cbGetDaqDeviceInventory(ANY_IFC, inventory, &numberOfDevices);
#ifndef NDEBUG
	if (numberOfDevices > 0)
		mexPrintf("%d MeilhausElectronics DAQ%s found:\n", numberOfDevices, (numberOfDevices == 1) ? "" : "s");
	else
		mexPrintf("No MeilhausElectronics DAQ found!\n");
#endif

	redlabs = numberOfDevices;

	if (redlabs > 0)
	{
		int n = 0;
		if (redlabStates)
		{
			mxFree(redlabStates);
			redlabStates = NULL;
		}
		mexMakeMemoryPersistent(redlabStates = static_cast<redlabState*>(mxCalloc(static_cast<size_t>(redlabs), sizeof(redlabState))));
		while (n < redlabs)
		{
			int BoardNum;
			BoardNum = n;
			DaqDeviceDescriptor DeviceDescriptor;
			DeviceDescriptor = inventory[BoardNum];

			redlab = n + 1;

#ifndef NDEBUG
			mexPrintf("Device Name: %s\n", DeviceDescriptor.ProductName);
			mexPrintf("Device Identifier: %s\n", DeviceDescriptor.UniqueID);
			mexPrintf("Assigned Board Number: %d\n", BoardNum);
#endif

			/* Creates a device object within the Universal Library and
			assign a board number to the specified DAQ device with cbCreateDaqDevice()

			Parameters:
			BoardNum			: board number to be assigned to the specified DAQ device
			DeviceDescriptor	: device descriptor of the DAQ device */

			UDStat = cbCreateDaqDevice(BoardNum, DeviceDescriptor);

			/* Flash the LED of the selected device */
			UDStat = cbFlashLED(BoardNum);

			int board_type;
			int numports;
			int numao;
			int numai;
			int numdio;
			int range;
			MEXMESSAGE(cbGetConfig(BOARDINFO, BoardNum, 0, BIBOARDTYPE, &board_type));
			MEXMESSAGE(cbGetConfig(BOARDINFO, BoardNum, 0, BIDINUMDEVS, &numports));
			MEXMESSAGE(cbGetConfig(BOARDINFO, BoardNum, 0, BINUMADCHANS, &numai));
			MEXMESSAGE(cbGetConfig(BOARDINFO, BoardNum, 0, BINUMDACHANS, &numao));

			// set BIDACRANGE (output voltage range) to +-10V
			// MEXMESSAGE(cbSetConfig(BOARDINFO, BoardNum, 0, BIDACRANGE, &range));

#ifndef NDEBUG
			mexPrintf("Board type: %d\n", board_type);
			mexPrintf("Number of digital ports: %d\n", numports);
			mexPrintf("Number of analog inputs: %d\n", numai);
			mexPrintf("Number of analog outputs: %d\n", numao);
#endif
			redlabStates[redlab - 1].range = static_cast<int*>(mxCalloc(numao, sizeof(int)));
			redlabStates[redlab - 1].aomin = static_cast<float*>(mxCalloc(numao, sizeof(float)));
			redlabStates[redlab - 1].aomax = static_cast<float*>(mxCalloc(numao, sizeof(float)));
			mexMakeMemoryPersistent(redlabStates[redlab - 1].range);
			mexMakeMemoryPersistent(redlabStates[redlab - 1].aomin);
			mexMakeMemoryPersistent(redlabStates[redlab - 1].aomax);

			for (int ka = 0; ka < numao; ka++)
			{
				MEXMESSAGE(cbGetConfig(BOARDINFO, BoardNum, ka, BIDACRANGE, &range));
				redlabStates[redlab - 1].range[ka] = range;
				switch (range)
				{
				case 1:
					redlabStates[redlab - 1].aomin[ka] = -10;
					redlabStates[redlab - 1].aomax[ka] = 10;
					break;
				case 100:
					redlabStates[redlab - 1].aomin[ka] = 0;
					redlabStates[redlab - 1].aomax[ka] = 10;
				}
#ifndef NDEBUG
				mexPrintf("Analog output range for VOUT%d = [%.1fV; %.1fV]\n", ka, redlabStates[redlab - 1].aomin[ka], redlabStates[redlab - 1].aomax[ka]);
#endif
			}
			redlabStates[redlab - 1].BoardNum = BoardNum;
			redlabStates[redlab - 1].numao = numao;

			if (numports < 1)
			{
				numdio = 0;
			}
			else
			{
				// get information about digital IO ports
				int out_mask;
				int in_mask;
				MEXMESSAGE(cbGetConfig(DIGITALINFO, BoardNum, 0, DIOUTMASK, &out_mask));
				MEXMESSAGE(cbGetConfig(DIGITALINFO, BoardNum, 0, DIINMASK, &in_mask));

				// We deal only with configurable ports
				if (in_mask & out_mask)
				{
					numdio = 0;
				}

				bool b_in;
				bool b_out;

				numdio = 0;
				for (int i = 0; i < 32; i++)
				{
					b_in = (in_mask & (1 << i)) != 0;
					b_out = (out_mask & (1 << i)) != 0;

					if (b_in || b_out)
					{
						numdio++;
					}
				}

				redlabStates[redlab - 1].numdio = numdio;
				redlabStates[redlab - 1].line_out = static_cast<bool*>(mxCalloc(numdio, sizeof(bool)));
				mexMakeMemoryPersistent(redlabStates[redlab - 1].line_out);

				// configure line 0 for input by default
				MEXMESSAGE(cbDConfigBit(BoardNum, AUXPORT, 0, DIGITALIN));
				redlabStates[redlab - 1].line_out[0] = false;

				// configure lines 1..end for output by default
				for (int i = 1; i < numdio; i++)
				{
					MEXMESSAGE(cbDConfigBit(BoardNum, AUXPORT, i, DIGITALOUT));
					redlabStates[redlab - 1].line_out[i] = true;
				}
			}
#ifndef NDEBUG
			mexPrintf("Number of digital IO lines : %d\n", numdio);
#endif

			// check the state of the pellicle
			int BitNum = 0;
			unsigned short BitValue;
			Sleep(500);
			cbDBitIn(BoardNum, AUXPORT, BitNum, &BitValue);
			bool val = BitValue == 0 ? false : true;
			if (!val)
			{
				// if pellicle is in - make a pulse and switch it
				BitNum = 1;
				cbDBitOut(BoardNum, AUXPORT, BitNum, 0);
				cbDBitOut(BoardNum, AUXPORT, BitNum, 1);
				cbDBitOut(BoardNum, AUXPORT, BitNum, 0);
			}

			n++;
		}
		// sets current redlab number
		redlab = 1;
	}

#ifndef NDEBUG
	if (redlabs > 0)
		mexPrintf("%d MeilhausElectronics DAQ device%s available for use.\n", static_cast<int>(redlabs), (redlabs == 1) ? "" : "s");
	else
		mexPrintf("No MeilhausElectronics DAQ available for use!\n");

#endif

	return static_cast<int>(redlabs);
}

extern "C" void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	if (nrhs == 0 && nlhs == 0)
	{
		mexPrintf("\nMeilhausElectronics RedLab DAQ driver.\n\n\tAndriy Chmyrov © 07.07.2016\n\n");
		return;
	}

	if (driver == NULL) mexErrMsgTxt("Semaphore not initialized.");

	if (redlab == -1)
	{
		if (mexStartup() == 0) return;
		redlab = 1;
	}

	int n = 0;
	while (n < nrhs)
	{
		SHORT index;
		int field;
		switch (mxGetClassID(prhs[n]))
		{
		default:
			mexErrMsgTxt("Parameter name expected as string.");
		case mxCHAR_CLASS:
		{
			char read[StringLength];
			if (mxGetString(prhs[n], read, StringLength)) mexErrMsgTxt("Unknown parameter.");
			if (++n < nrhs)
			{
				if ((_stricmp("getDIO", read) == 0) || (_stricmp("getConfig", read) == 0))
				{
					VALUE = getParameter2(read, prhs[n]);
					return;
				}
				else
					setParameter(read, prhs[n]);
				break;
			}
			if (nlhs > 1) mexErrMsgTxt("Too many output arguments.");
			VALUE = getParameter(read);
			return;
		}
		case mxSTRUCT_CLASS:
			for (index = 0; index < static_cast<int>(mxGetNumberOfElements(prhs[n])); index++)
				for (field = 0; field < mxGetNumberOfFields(prhs[n]); field++)
					setParameter(mxGetFieldNameByNumber(prhs[n], field), mxGetFieldByNumber(prhs[n], index, field));
			;
		}
		n++;
	}
	switch (nlhs)
	{
	default:
		mexErrMsgTxt("Too many output arguments.");
	case 1:
		VALUE = mxCreateDoubleScalar(static_cast<double>(redlabs));
	case 0:;
	}
}