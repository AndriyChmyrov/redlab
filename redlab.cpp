// redlab.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "redlab.h"


/*	Selected powermeter.
*/
int redlab = -1;
int redlabs = -1;


/*	Powermeter states. Unused handles are set to 0.
*/
redlabState* redlabStates;


/*	Check for and read a scalar value.
*/
double getScalar(const mxArray* array)
{
	if (!mxIsNumeric(array) || mxGetNumberOfElements(array) != 1) mexErrMsgTxt("Not a scalar.");
	return mxGetScalar(array);
}


/*	Check for and read a matrix
*/
double* getArray(const mxArray* array, int size)
{
	if (!mxIsNumeric(array) || mxGetNumberOfElements(array) != size) mexErrMsgTxt("Supplied wrong number of elements in array.");
	return mxGetPr(array);
}


/*	Get a parameter or status value.
*/
mxArray* getParameter(const char* name)
{
	if (_stricmp("numAO", name) == 0)
	{
		return mxCreateDoubleScalar(static_cast<double>(redlabStates[redlab - 1].numao));
	}

	if (_stricmp("reset", name) == 0)
	{
		int BoardNum = redlabStates[redlab - 1].BoardNum;
		int numao = redlabStates[redlab - 1].numao;
		int numdio = redlabStates[redlab - 1].numdio;
		
		MEXENTER;
		for (int n = 0; n < numao; n++)
		{
			MEXMESSAGE(cbVOut(BoardNum, n, redlabStates[redlab - 1].range[n], 0, DEFAULTOPTION));
		}
		
		for (int n = 0; n < numdio; n++)
		{
			if (redlabStates[redlab - 1].line_out[n])
			{
				MEXMESSAGE(cbDBitOut(BoardNum, AUXPORT, n, 0));
			}
		}
		MEXLEAVE;

		return mxCreateDoubleScalar(1);
	}

#ifndef NDEBUG
	mexPrintf("%s:%d - ", __FILE__, __LINE__);
#endif
	mexPrintf("\"%s\" unknown.\n", name);
	return NULL;

}


/*	Get a parameter or status value.
*/
mxArray* getParameter2(const char* name, const mxArray* field)
{
	if (mxGetNumberOfElements(field) < 1) return NULL;

	if (redlab < 1 || redlab > redlabs) mexErrMsgTxt("Invalid powermeter handle.");

	int BoardNum = redlabStates[redlab - 1].BoardNum;
	int maxdio = redlabStates[redlab - 1].numdio - 1;

	if (_stricmp("getConfig", name) == 0)
	{
		if (mxGetN(field) != 3)
			mexErrMsgTxt("You need to specify three parameters - InfoType, DevNum (typically 0) and ConfigItem!");
		double* arguments = getArray(field, static_cast<int>(mxGetN(field)));
		int InfoType = static_cast<int>(arguments[0]);
		int DevNum = static_cast<int>(arguments[1]);
		int ConfigItem = static_cast<int>(arguments[2]);
		
		if ((InfoType < GLOBALINFO) || (InfoType > MEMINFO))
		{
			char errStr[StringLength];
			sprintf_s(errStr, StringLength, "First parameter - InfoType - should be in a range[%d, %d]!", GLOBALINFO, MEMINFO);
			mexErrMsgTxt(errStr);
		}

		int ConfigVal;
		MEXENTER;
		MEXMESSAGE(cbGetConfig(InfoType, BoardNum, DevNum, ConfigItem, &ConfigVal));
		MEXLEAVE;
		return mxCreateDoubleScalar(ConfigVal);
	}

	if (_stricmp("getDIO", name) == 0)
	{
		if (mxGetN(field) != 1)
			mexErrMsgTxt("You need to specify only 1 parameter - BitNum!");
		int BitNum = static_cast<int>(getScalar(field));
		if ((BitNum < 0) || (BitNum > redlabStates[redlab - 1].numdio-1))
		{
			char *errStr;
			errStr = static_cast<char*>(mxCalloc(StringLength, sizeof(char)));
			sprintf_s(errStr, StringLength, "BitNum should be a positive value in a range[0, %d]!", maxdio);
			mexErrMsgTxt(errStr);
		}
		unsigned short BitValue;
		MEXENTER;
		MEXMESSAGE(cbDBitIn(BoardNum, AUXPORT, BitNum, &BitValue));
		MEXLEAVE;
		return mxCreateLogicalScalar(BitValue == 0 ? false : true);
	}

#ifndef NDEBUG
	mexPrintf("%s:%d - ", __FILE__, __LINE__);
#endif
	mexPrintf("\"%s\" unknown.\n", name);
	return NULL;

}


/*	Set a measurement parameter.
*/
void setParameter(const char* name, const mxArray* field)
{
	if (mxGetNumberOfElements(field) < 1) return;
	if (_stricmp("redlab", name) == 0) return;
	if (_stricmp("redlabs", name) == 0) return;
	if (redlab < 1 || redlab > redlabs) mexErrMsgTxt("Invalid powermeter handle.");

	int BoardNum = redlabStates[redlab - 1].BoardNum;
	int numao = redlabStates[redlab - 1].numao;
	int maxdio = redlabStates[redlab - 1].numdio - 1;

	if (_stricmp("configureDIO", name) == 0)
	{
		if (mxGetNumberOfElements(field) != 2)
			mexErrMsgTxt("You need to specify 2 parameters - [BitNum, Direction], where Direction = 1 for DigitalOut or 2 for DigitalIn!");
		double* arguments = getArray(field, 2);
		int BitNum = static_cast<int>(arguments[0]);
		if ((BitNum < 0) || (BitNum > maxdio))
		{
			char *errStr;
			errStr = static_cast<char*>(mxCalloc(StringLength, sizeof(char)));
			sprintf_s(errStr, StringLength, "BitNum should be a positive value in a range[0, %d]!", maxdio);
			mexErrMsgTxt(errStr);
		}
		int Dir = static_cast<int>(arguments[1]);
		int Direction = 1;
		if (Dir == 1)
			Direction = DIGITALOUT;
		else if (Dir == 2)
			Direction = DIGITALIN;
		else
			mexErrMsgTxt("Direction parameter has to be 1 for DigitalOut or 2 for DigitalIn!");
		redlabStates[redlab - 1].line_out[BitNum] = (Direction == DIGITALOUT);
		MEXENTER;
		MEXMESSAGE(cbDConfigBit(BoardNum, AUXPORT, BitNum, Direction));
		MEXLEAVE;
		return;
	}

	if (_stricmp("pulseDIO", name) == 0)
	{
		if (mxGetNumberOfElements(field) != 1)
			mexErrMsgTxt("You need to specify 1 parameter - BitNum");
		int BitNum = static_cast<int>(getScalar(field));
		if ((BitNum < 0) || (BitNum > maxdio))
		{
			char *errStr;
			errStr = static_cast<char*>(mxCalloc(StringLength, sizeof(char)));
			sprintf_s(errStr, StringLength, "BitNum should be a positive value in a range[0, %d]!", maxdio);
			mexErrMsgTxt(errStr);
		}
		if (!redlabStates[redlab - 1].line_out[BitNum])
		{
			char errStr[StringLength];
			sprintf_s(errStr, StringLength, "Bit %d is configured for input!", BitNum);
			mexErrMsgTxt(errStr);
		}
		MEXENTER;
		MEXMESSAGE(cbDBitOut(BoardNum, AUXPORT, BitNum, 0));
		MEXMESSAGE(cbDBitOut(BoardNum, AUXPORT, BitNum, 1));
		MEXMESSAGE(cbDBitOut(BoardNum, AUXPORT, BitNum, 0));
		MEXLEAVE;
		return;
	}

	if (_stricmp("setConfig", name) == 0)
	{
		if (mxGetNumberOfElements(field) != 2)
			mexErrMsgTxt("You need to specify 2 parameters - [ConfigItem, ConfigVal]!");
		double* arguments = getArray(field, 2);

		int ConfigItem = static_cast<int>(arguments[0]);
		int ConfigVal = static_cast<int>(arguments[1]);

		MEXENTER;
		MEXMESSAGE(cbSetConfig(BOARDINFO, BoardNum, 0, ConfigItem, ConfigVal));
		MEXLEAVE;
		return;
	}

	if (_stricmp("setDIO", name) == 0)
	{
		if (mxGetNumberOfElements(field) != 2)
			mexErrMsgTxt("You need to specify 2 parameters - [BitNum, BitValue]");
		double* arguments = getArray(field, 2);
		int BitNum = static_cast<int>(arguments[0]);
		if ((BitNum < 0) || (BitNum > maxdio))
		{
			char errStr[StringLength];
			sprintf_s(errStr, StringLength, "BitNum should be a positive value in a range[0, %d]!", maxdio);
			mexErrMsgTxt(errStr);
		}
		if (!redlabStates[redlab - 1].line_out[BitNum])
		{
			char errStr[StringLength];
			sprintf_s(errStr, StringLength, "Bit %d is configured for input!", BitNum);
			mexErrMsgTxt(errStr);
		}
		if ((arguments[1] < 0) || (arguments[1] > 1))
			mexErrMsgTxt("BitValue should be a integer [0, 1]");
		unsigned short BitValue = static_cast<unsigned short>(arguments[1]);
		MEXENTER;
		MEXMESSAGE(cbDBitOut(BoardNum, AUXPORT, BitNum, BitValue));
		MEXLEAVE;
		return;
	}

	if (_stricmp("writeAO", name) == 0)
	{
		double* arguments = getArray(field, 2);
		int Chan;
		Chan = static_cast<int>(arguments[0]);
		float value = static_cast<float>(arguments[1]);
		if ((value < redlabStates[redlab - 1].aomin[Chan]) || (value > redlabStates[redlab - 1].aomax[Chan]))
		{
			char errStr[StringLength];
			sprintf_s(errStr, StringLength, "Requested value %fV is outside of the configured output range for VOUT%d = [%.1fV; %.1fV]!", value, Chan, redlabStates[redlab - 1].aomin[Chan], redlabStates[redlab - 1].aomax[Chan]);
			mexErrMsgTxt(errStr);
		}
		MEXENTER;
		MEXMESSAGE(cbVOut(BoardNum, Chan, redlabStates[redlab - 1].range[Chan], value, DEFAULTOPTION));
		MEXLEAVE;
		return;
	}

	if (_stricmp("writeAOall", name) == 0)
	{
		double* arguments = getArray(field, numao);
		for (int n = 0; n < numao; n++)
		{
			float value = static_cast<float>(arguments[n]);
			MEXENTER;
			MEXMESSAGE(cbVOut(BoardNum, n, redlabStates[redlab - 1].range[n], value, DEFAULTOPTION));
			MEXLEAVE;
		}
		return;
	}


#ifndef NDEBUG
	mexPrintf("%s:%d - ", __FILE__, __LINE__);
#endif
	mexPrintf("\"%s\" unknown.\n", name);
	return;
}