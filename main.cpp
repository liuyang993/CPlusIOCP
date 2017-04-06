#include "IOCPServer.h"

CIOCPServer *m_iocpServer = NULL;



void CALLBACK NotifyProc( ClientContext *pContext, UINT nCode)
{
	try
	{
		CString str;


		//str.Format("S: %.2f kb/s R: %.2f kb/s", (float)m_iocpServer->m_nSendKbps / 1024, (float)m_iocpServer->m_nRecvKbps / 1024);
		//printf("%s\n",str);
		switch (nCode)
		{
		case NC_CLIENT_CONNECT:
			break;
		case NC_CLIENT_DISCONNECT:
			
			break;
		case NC_TRANSMIT:
			break;
		case NC_RECEIVE:
			//ProcessReceive(pContext);
			break;
		case NC_RECEIVE_COMPLETE:
			//ProcessReceiveComplete(pContext);
			break;
		}
	}catch(...){}
}

unsigned __stdcall ThreadPoolFuncSP (LPVOID thisContext)    //线程池函数
{
				int				rtn;
			std::string	msg;

		//CDB_Redious* pThis = reinterpret_cast<CDB_Redious*>(thisContext);
		CDB_Redious*	mpDB_Redious = new CDB_Redious();
	mpDB_Redious->Init("Provider=SQLOLEDB;Integrated Security=SSPI;Initial Catalog=SmalllSmartBilling;Data Source=(local)",30,30);
	mpDB_Redious->Connect();
		//int				rtn;
		//std::string	msg;
		//mpDB_Redious->sp_Test_alwayRetTrue(1,&rtn,&msg);
		//printf("sp return %s\n",msg.c_str());
		if(mpDB_Redious->Connect())
		{
			printf("thread %d connect success\n" , GetCurrentThreadId());
		}
		else
		{
			printf("thread %d connect fail\n" , GetCurrentThreadId());
		}

			mpDB_Redious->sp_Test_alwayRetTrue(1,&rtn,&msg);

		
			time_t timer;
			char buffer[26];
			struct tm* tm_info;

			time(&timer);
			tm_info = localtime(&timer);

			strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);


			printf("sp in thread %d return %s at time %s\n" , GetCurrentThreadId() , msg.c_str(),buffer);

		return 0;
}

void main()
{
	// test CBuffer 
	//CBuffer *cb1 =  new CBuffer();
	//char cSrcArray[] = {"abcxyz"};
	//char cSecondSrcArray[] = {"www"};
	//char *cDstArray = new char[6];
	//cb1->Write((PBYTE)cSrcArray,6);
	//cb1->Read((PBYTE)cDstArray,3);
	//cb1->Write((PBYTE)cSecondSrcArray,3);
	//getchar();
	//return;

	// 开启IPCP服务器
	m_iocpServer = new CIOCPServer;

	//test call sql sp concurrently from threads 

	//UINT  nThreadID;
	//HANDLE hWorker;

	//for ( int i = 0; i < 5; i++ ) 
	//{
	//	hWorker = (HANDLE)_beginthreadex(NULL,					// Security
	//									0,						// Stack size - use default
	//									ThreadPoolFuncSP,     		// Thread fn entry point
	//									(void*) m_iocpServer,			// Param for thread
	//									0,						// Init flag
	//									&nThreadID);			// Thread address


	//	if (hWorker == NULL ) 
	//	{
	//		//CloseHandle( m_hCompletionPort );
	//		return ;
	//	}

	//	CloseHandle(hWorker);
	//}

	//getchar();
	//return;


 	if (m_iocpServer->Initialize(NotifyProc, 100000, 60001))
 	{

		char hostname[256]; 
		gethostname(hostname, sizeof(hostname));
		HOSTENT *host = gethostbyname(hostname);
		if (host != NULL)
		{ 
			for ( int i=0; ; i++ )
			{ 
				//str += inet_ntoa(*(IN_ADDR*)host->h_addr_list[i]);
				if ( host->h_addr_list[i] + host->h_length >= host->h_name )
					break;
				//str += "/";
			}
		}

  	//	m_wndStatusBar.SetPaneText(0, str);
 		//str.Format("端口: %d", nPort);
 		//m_wndStatusBar.SetPaneText(2, str);
 	}
 	else
 	{
 		//str.Format("端口%d绑定失败", nPort);
 		//m_wndStatusBar.SetPaneText(0, str);
 		//m_wndStatusBar.SetPaneText(2, "端口: 0");
 	}

	getchar();
	return;
}