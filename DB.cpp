#include "DB.h"

#import "c:\Program Files\Common Files\System\ADO\msado15.dll" no_namespace rename("EOF", "EndOfFile")


char* rtrim(char *str)
{
	//CTBCAFString s;
	int n = strlen(str);
	if(n == 0)
		return "";
	while((n > 0) && (str[n - 1] == ' '))
	{
		str[n - 1] = '\0';
		n--;
	}
	return str;
}


CDB::CDB()
{
	//::CoInitialize(NULL);
}

CDB::~CDB()
{
	Disconnect();
	//::CoUninitialize();
}

bool CDB::Init(
	std::string strConnectString,
	int iConnectTimeout,
	int iCommandTimeout)
{
	mstrConnectString = strConnectString;
	miConnectTimeout = iConnectTimeout;
	miCommandTimeout = iCommandTimeout;
	mpConn = NULL;
	mLastOperation = (unsigned __int32)timeGetTime();
	return true;
}

bool CDB::Connect()
{
	try
	{
		if (mpConn != NULL)
		{
			TRACE("disconnect database before re-connect\n");
			Disconnect();
		}

		TRACE("connect database\n");
		if (mpConn == NULL) 
			TRACE("DEBUG1: mpConn is NULL\n");
		else
			TRACE("DEBUG1: mpConn is not  NULL");

		if (mpConn == NULL)
			::CoInitialize(NULL);

		mpConn.CreateInstance(__uuidof(Connection));
		mpConn->ConnectionTimeout = miConnectTimeout;

		if (FAILED(mpConn->Open((_bstr_t)mstrConnectString.c_str(), "", "", adConnectUnspecified)))
		{
			TRACE("connect database failed. connection=%s", mstrConnectString.c_str());
			return false;
		}
	}
	catch (_com_error &e)
	{
		//if(mpTGCall)
		//	mpTGCall->LogTrace(TBCAF_TRACE_LEVEL_ERROR, FWARNING"connect database failed. error='%s'\n", (char*) e.ErrorMessage());
		//else

			TRACE("DB-->connect()  error ");
			TRACE("conncetString is %s",mstrConnectString.c_str());
			if (mpConn == NULL) 
				TRACE("mpConn is NULL");
			else
				TRACE("mpConn is not  NULL");


			TRACE("connect database failed. connection=%s error='%s'\n", mstrConnectString.c_str(), (char*) e.ErrorMessage() );
		return false;
	}
	TRACE("  connect DB success");
	return true;
}

bool CDB::Disconnect()
{
	if (mpConn != NULL)
	{
		try
		{
			TRACE("disconnect database");
			mpConn->Close();
			mpConn.Release();
			mpConn = NULL;
		}
		catch (_com_error &e)
		{
			//if(mpTGCall)
			//	mpTGCall->LogTrace(TBCAF_TRACE_LEVEL_ERROR, FWARNING"disconnect database failed. error='%s'\n", (char*) e.ErrorMessage());
			//else
				TRACE("disconnect database failed. error='%s'\n", (char*) e.ErrorMessage());
			//mpConn.Release();
			mpConn = NULL;
			return false;
		}
	}
	return true;
}

//CTBCAFString CDB::GetConnectString()
//{
//	return mstrConnectString;
//}

bool CDB::CreateConnection(_ConnectionPtr &pConn, const char *strConnectString, int timeout)
{
	pConn.CreateInstance(__uuidof(Connection));
	pConn->ConnectionTimeout = timeout;
	if (FAILED(pConn->Open((_bstr_t)strConnectString, "", "", adConnectUnspecified)))
		return false;
	else
		return true;
}

void CDB::CreateCommand(_CommandPtr &pCommand, _ConnectionPtr pConn, const char *sp_name, int timeout)
{
	pCommand.CreateInstance(__uuidof(Command));
	pCommand->ActiveConnection = pConn;
	pCommand->CommandText = (_bstr_t)sp_name;
	pCommand->CommandType = adCmdStoredProc;
	pCommand->CommandTimeout = timeout;
}

void CDB::CreateRecordset(_RecordsetPtr &pRecordset)
{
	pRecordset.CreateInstance(__uuidof(Recordset));
}

void CDB::Set_CTBCAFString_Value(_CommandPtr pCommand, const char *name, std::string& value)
{
	pCommand->Parameters->Item[(_bstr_t)name]->Value=(_bstr_t)value.c_str();
}

void CDB::Set_SYSTEMTIME_Value(_CommandPtr pCommand, const char *name, SYSTEMTIME& value)
{
	double dt;
	SystemTimeToVariantTime(&value, &dt);
	pCommand->Parameters->Item[(_bstr_t)name]->Value=dt;
}

void CDB::Set_int_Value(_CommandPtr pCommand, const char *name, int& value)
{
	pCommand->Parameters->Item[(_bstr_t)name]->Value=(long)value;
}

void CDB::Set_double_Value(_CommandPtr pCommand, const char * name, double& value)
{
	pCommand->Parameters->Item[(_bstr_t)name]->Value=value;
}

void CDB::Set__variant_t_Value(_CommandPtr pCommand, const char *name, _variant_t& value)
{
	pCommand->Parameters->Item[(_bstr_t)name]->Value=value;
}


void CDB::Get_CTBCAFString_Value(_CommandPtr pCommand, const char *name, std::string* pOutData)
{
	_variant_t value;
	if (pOutData)
	{
		value = pCommand->Parameters->Item[(_bstr_t)name]->Value;
		if (value.vt != VT_NULL)
			*pOutData = rtrim((char*)((_bstr_t)value));
		else
			*pOutData = "";
	}
}

void CDB::Get_SYSTEMTIME_Value(_CommandPtr pCommand, const char *name, SYSTEMTIME* pOutData)
{
	_variant_t value;
	SYSTEMTIME st;
	if (pOutData)
	{
		value = pCommand->Parameters->Item[(_bstr_t)name]->Value;
		if (value.vt != VT_NULL)
		{
			VariantTimeToSystemTime((double)value, &st);
			*pOutData = st;
		}
		else
		{
			st.wYear = 1900;
			st.wMonth = 1;
			st.wDayOfWeek = 2;
			st.wDay = 1;
			st.wHour = 0;
			st.wMinute = 0;
			st.wSecond = 0;
			st.wMilliseconds = 0;
			*pOutData = st;
		}
	}
}

void CDB::Get_int_Value(_CommandPtr pCommand, const char *name, int* pOutData)
{
	_variant_t value;
	if (pOutData)
	{
		value = pCommand->Parameters->Item[(_bstr_t)name]->Value;
		if (value.vt != VT_NULL)
			*pOutData = long(value);
		else
			*pOutData =  0;
	}
}

void CDB::Get_double_Value(_CommandPtr pCommand, const char *name, double* pOutData)
{
	_variant_t value;
	if (pOutData)
	{
		value = pCommand->Parameters->Item[(_bstr_t)name]->Value;
		if (value.vt != VT_NULL)
			*pOutData = double(value);
		else
			*pOutData = 0;
	}
}

void CDB::Get__variant_t_Value(_CommandPtr pCommand, const char *name, _variant_t* pOutData)
{
	if (pOutData)
	{
		*pOutData = pCommand->Parameters->Item[(_bstr_t)name]->Value;
	}
}



std::string CDB::ToString_CTBCAFString(const char *value)
{
	return value;
}

std::string CDB::ToString_SYSTEMTIME(SYSTEMTIME &value)
{
	char s[50];
	sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d.%03d\0", value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond, value.wMilliseconds);
	return s;
}

std::string CDB::ToString_int(int value)
{
	char buffer[20];
	itoa(value, buffer, 10);
	return buffer;
}

std::string CDB::ToString_double(double value)
{
	char buffer[20];
	_snprintf(buffer, sizeof(buffer), "%f", value);
	return buffer;
}

std::string CDB::ToString__variant_t(_variant_t value)
{
	return "variant";
}
