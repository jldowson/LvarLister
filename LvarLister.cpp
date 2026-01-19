// LvarLister.cpp

#include <MSFS/MSFS_WindowsTypes.h>
#include <MSFS/MSFS.h>
#include <MSFS/Legacy/gauges.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <SimConnect.h>
#include "LvarLister.h"

#define MY_DATA_DEFINITION 0xFF
#define DATA_REQUEST_ID 1

enum DATA_NAMES
{
	DATA_TITLE,
	DATA_CAMERASTATE
};
enum STATES
{
	EV_NULL,
	EV_SIM_START,
	EV_1SEC_UPDATE,
};

struct StructOneDatum
{
	int		id;
	union
	{
		char title[256];
		int cameraState;
	};
};
// maxReturnedItems is 2 in this case, as we only request
// TITLE and CAMERA STATE
#define maxReturnedItems	2

// Define Scan period, the time between scand for lvars (in seconds)
#define lvarScanPeriod	30

// A structure that can be used to receive Tagged data
struct StructDatum
{
	StructOneDatum  datum[maxReturnedItems];
};

HANDLE  hSimConnect = 0, hLog = 0;
BOOL cameraState = FALSE;
std::string logFileName = "\\work\\LvarLister.log";
std::ofstream logFile;

#define LogBufferSize 512
char szLogBuffer[LogBufferSize];

std::string getCurrentTime()
{
	std::string currTime;
	//Current date/time based on current time
	time_t now = time(0);
	// Convert current time to string
	currTime.assign(ctime(&now));

	// Last charactor of currentTime is "\n", so remove it
	std::string currentTime = currTime.substr(0, currTime.size() - 1);

	return currentTime;
}

void LOG(std::string& data)
{
	logFile << getCurrentTime() << "  " << data << std::endl;
}
void LOG(const char* data)
{
//	fprintf(stderr, data);
	std::string s(data, strlen(data));
	LOG(s);
}

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	static BOOL dataRequested = FALSE;
	static int count = 0, noScans = 0;
	HRESULT hr;
	switch (pData->dwID)
	{
		case SIMCONNECT_RECV_ID_EVENT:
		{
			SIMCONNECT_RECV_EVENT* evt = (SIMCONNECT_RECV_EVENT*)pData;

			switch (evt->uEventID)
			{
				case EV_1SEC_UPDATE:
				{
					if (cameraState && count++ % lvarScanPeriod == 0)
					{
						noScans++;
						LOG("Scanning for Lvars....");
						PCSTRINGZ lvarname;
//						std::vector<PCSTRINGZ> lvarList; 
						bool isValidLVAR = true;
						int noLvars = 0;
						do {
							lvarname = get_name_of_named_variable(noLvars);
							if (lvarname)
							{
//								lvarList.push_back(lvarname);
//								snprintf(szLogBuffer, LogBufferSize-1, "    ID %d='%s'", noLvars, lvarname);
//								szLogBuffer[LogBufferSize - 1] = 0;
//								LOG(szLogBuffer);
								noLvars++;
							}
							else
								isValidLVAR = false;
						} while (isValidLVAR);

						sprintf(szLogBuffer, "Scan %d found %d lvars: ", noScans, noLvars);
						LOG(szLogBuffer);
/****
						for (int i=0; i< (int)lvarList.size(); i++)
						{
							snprintf(szLogBuffer, LogBufferSize-1, "    ID %d='%s'", i, lvarList[i]);
							szLogBuffer[LogBufferSize-1] = 0;
							LOG(szLogBuffer);
						}
						lvarList.clear();
****/
						count = 1;
					}
					break;
				}
				case EV_SIM_START:
				{
					if (!dataRequested)
					{
						// Now the sim is running, request information on the user aircraft
						SimConnect_AddToDataDefinition(hSimConnect, MY_DATA_DEFINITION, "TITLE", nullptr, SIMCONNECT_DATATYPE_STRING256, 0, DATA_TITLE);
						SimConnect_AddToDataDefinition(hSimConnect, MY_DATA_DEFINITION, "CAMERA STATE", nullptr, SIMCONNECT_DATATYPE_INT32, 0, DATA_CAMERASTATE);
						hr = SimConnect_RequestDataOnSimObject(hSimConnect, DATA_REQUEST_ID, MY_DATA_DEFINITION,
							SIMCONNECT_SIMOBJECT_TYPE_USER, SIMCONNECT_PERIOD_VISUAL_FRAME,
							SIMCONNECT_DATA_REQUEST_FLAG_CHANGED | SIMCONNECT_DATA_REQUEST_FLAG_TAGGED);
						if (hr == S_OK)
						{
							LOG("Requested TITLE & CAMERA STATE data on User Aircraft");
							dataRequested = TRUE;
						}
						else
							LOG("**** Requested TITLE & CAMERA STATE data on User Aircraft failed");
						SimConnect_SubscribeToSystemEvent(hSimConnect, EV_1SEC_UPDATE, "1sec");
					}
					break;
				}
			}
			break;
		}
		
		case SIMCONNECT_RECV_ID_OPEN:
		{
			SIMCONNECT_RECV_OPEN* pOpen = (SIMCONNECT_RECV_OPEN*)pData;

			if (pOpen->dwApplicationVersionMajor == 12)
			{
				LOG("Running in MSFS2024");
			}
			else
				LOG("Running in MSFS2020");
			break;
		}

		case SIMCONNECT_RECV_ID_QUIT:
		{
			LOG("Quit message received");
			break;
		}

		case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
		{
			SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

			switch (pObjData->dwRequestID)
			{
				case DATA_REQUEST_ID:
				{
					int	count = 0;
					StructDatum* pS = (StructDatum*)&pObjData->dwData;
					while (count < (int)pObjData->dwDefineCount)
					{
						switch (pS->datum[count].id)
						{
							case DATA_TITLE:
							{
								sprintf(szLogBuffer, "Sim data received: aircraft='%s'", pS->datum[count].title);
								LOG(szLogBuffer);
								break;
							}

							case DATA_CAMERASTATE:
							{
								sprintf(szLogBuffer, "Sim data received: cameraState=%d", pS->datum[count].cameraState);
								LOG(szLogBuffer);
								if ((((pS->datum[count].cameraState < 11 && pS->datum[count].cameraState > 1) || pS->datum[count].cameraState == 18) && !cameraState) ||
									(pS->datum[count].cameraState > 10 && pS->datum[count].cameraState < 26 && pS->datum[count].cameraState != 18 && cameraState))
								{
									cameraState = !cameraState;
								}
								break;
							}

							default:
							{
								sprintf(szLogBuffer, "Unknown simdata datum ID received (count=%d, dwDefineCount=%lu): %d", count, pObjData->dwDefineCount, pS->datum[count].id);
								LOG(szLogBuffer);
								break;
							}
						}
						++count;
					}
					break;
				}

				default:
				{
					sprintf(szLogBuffer, "Unknown dwRequestID ID received for SimIbject data: %lu", pObjData->dwRequestID);
					LOG(szLogBuffer);
					break;
				}
			}
			break;
		}

		case SIMCONNECT_RECV_ID_EXCEPTION:
		{
			SIMCONNECT_RECV_EXCEPTION* except = (SIMCONNECT_RECV_EXCEPTION*)pData;
			sprintf(szLogBuffer, "Simconnect Exception received: %lu (dwSendID=%lu)", except->dwException, except->dwSendID);
			LOG(szLogBuffer);
			break;
		}

		default:
		{
			sprintf(szLogBuffer, "Unknown event received: %lu (%lX)", pData->dwID, pData->dwID);
			LOG(szLogBuffer);
			break;
		}
	}
}

bool existsFile(const std::string& name)
{
	if (FILE* file = fopen(name.c_str(), "r"))
	{
		fclose(file);
		return true;
	}
	else
		return false;
}


// This is called when the WASM is loaded into the system.
extern "C" MSFS_CALLBACK void module_init(void)
{
	// Open log file
	if (existsFile(logFileName))
	{
		std::remove(logFileName.c_str());
	}
	logFile.open(logFileName.c_str(), std::ios::out | std::ios::app);
	LOG("<=============================== START OF PROGRAM ===============================>");

	// Get a SimConnect Connection
	if (SUCCEEDED(SimConnect_Open(&hSimConnect, "LVARLISTER.WASM", (HWND)NULL, 0, (HANDLE)NULL, -1)))
	{
		LOG("SimConnect connection opened ok");
		SimConnect_SubscribeToSystemEvent(hSimConnect, EV_SIM_START, "SimStart");

		if (!SUCCEEDED(SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL)))
			LOG("Error calling SimConnect_CallDispatch");
	}
	else
		LOG("SimConnect_Open failed");

}

extern "C" MSFS_CALLBACK void module_deinit(void)
{
	if (hSimConnect)
	{
		// Stop Data Request
		SimConnect_RequestDataOnSimObject(hSimConnect, DATA_REQUEST_ID, MY_DATA_DEFINITION, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER);
		SimConnect_Close(hSimConnect);
		hSimConnect = 0;
	}
	
	LOG("<=============================== END OF PROGRAM ===============================>");
	logFile.close();
}
