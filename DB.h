#ifndef __DB_HPP__
#define __DB_HPP__


#include <iostream>
#include <windows.h>
#include <winerror.h>
#include <time.h>
#include "trace.h"


#import "c:\Program Files\Common Files\System\ADO\msado15.dll" no_namespace rename("EOF", "EndOfFile")

#endif /* __DB_HPP__ */



class  CDB
{
public:
	std::string mstrConnectString;
	int miConnectTimeout;
	int miCommandTimeout;
	_ConnectionPtr mpConn;

	unsigned __int32 mLastOperation;

public:
	CDB();
	virtual ~CDB();
	bool Init(
		std::string strConnectString,
		int iConnectTimeout,
		int iCommandTimeout);
	bool Connect();
	bool Disconnect();
	//CTBCAFString GetConnectString();

protected:
	bool CreateConnection(_ConnectionPtr &pConn, const char *strConnectString, int timeout = 3);
	void CreateCommand(_CommandPtr &pCommand, _ConnectionPtr pConn, const char *sp_name, int timeout = 3);
	void CreateRecordset(_RecordsetPtr &pRecordset);

	void Set_CTBCAFString_Value(_CommandPtr pCommand, const char *name, std::string& value);
	void Set_SYSTEMTIME_Value(_CommandPtr pCommand, const char *name, SYSTEMTIME& value);
	void Set_int_Value(_CommandPtr pCommand, const char *name, int& value);
	void Set_double_Value(_CommandPtr pCommand, const char * name, double& value);
	void Set__variant_t_Value(_CommandPtr pCommand, const char *name, _variant_t& value);

	void Get_CTBCAFString_Value(_CommandPtr pCommand, const char *name, std::string* pOutData);
	void Get_SYSTEMTIME_Value(_CommandPtr pCommand, const char *name, SYSTEMTIME* pOutData);
	void Get_int_Value(_CommandPtr pCommand, const char *name, int* pOutData);
	void Get_double_Value(_CommandPtr pCommand, const char *name, double* pOutData);
	void Get__variant_t_Value(_CommandPtr pCommand, const char *name, _variant_t* pOutData);

	std::string ToString_CTBCAFString(const char *value);
	std::string ToString_SYSTEMTIME(SYSTEMTIME &value);
	std::string ToString_int(int value);	
	std::string ToString_double(double value);
	std::string ToString__variant_t(_variant_t value);
};


