#ifndef __DB_Redious__HPP__
#define __DB_Redious__HPP__

#include "DB.h"


//------------------------------------------------------------------------------
//									DB_Redious
//------------------------------------------------------------------------------
class  CDB_Redious : public CDB
{
public:
	CDB_Redious();
	virtual ~CDB_Redious();

public:

	bool sp_Test_alwayRetTrue
	(
		int I_iCallID_A,
		int *O_iErrCode,
		std::string *O_sMsg
	);

};

#endif /* __DB_CCloud__HPP__ */