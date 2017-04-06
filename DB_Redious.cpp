#include "DB_Redious.h"


//------------------------------------------------------------------------------
//									CDB_Redious
//------------------------------------------------------------------------------
CDB_Redious::CDB_Redious()
{
}

CDB_Redious::~CDB_Redious()
{
}


bool CDB_Redious::sp_Test_alwayRetTrue
(
		int in_I_CallID,
		int *out_O_ErrCode,
		std::string *out_O_Msg
)
{

	//::CoInitialize(NULL);
	try{
		_CommandPtr pCommand = NULL;

		unsigned __int32 curTick =(unsigned __int32)timeGetTime();
		if (mpConn == NULL)
		{
			TRACE("not yet connect");
			Connect();

		}
		else if (((curTick - mLastOperation) / 1000) > 30)
		{
			TRACE("timeout");
			Disconnect();
			Connect();
		}
		mLastOperation = curTick;
		
		pCommand.CreateInstance(__uuidof(Command));
		pCommand->ActiveConnection = mpConn;
		pCommand->CommandText = (_bstr_t)"sp_Test_alwayRetTrue";
		pCommand->CommandType = adCmdStoredProc;
		pCommand->CommandTimeout = miCommandTimeout;

		pCommand->Parameters->Refresh();
		Set_int_Value(pCommand, "@I_callID_A", in_I_CallID);


		pCommand->Execute(NULL, NULL, adCmdStoredProc);
		//TRACE("reach here mean exec sp success\n");

		Get_int_Value(pCommand, "@O_ErrCode", out_O_ErrCode);
		Get_CTBCAFString_Value(pCommand, "@O_Msg", out_O_Msg);

		//pCommand.Detach();
		pCommand.Release();
		pCommand = NULL;
	}
	catch (_com_error &e)
	{
		printf("exec procedure failed. error='%s'\n", (char*) e.ErrorMessage());
		Disconnect();
		return false;
	}
	//::CoUninitialize();
	return true;
}