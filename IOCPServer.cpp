// IOCPServer.cpp: implementation of the CIOCPServer class.
//
//////////////////////////////////////////////////////////////////////

#include <winsock2.h>
#include <winioctl.h>
#include "IOCPServer.h"
#include "../common/zlib.h"


//#ifdef _DEBUG
//#undef THIS_FILE
//static char THIS_FILE[]=__FILE__;
//#define new DEBUG_NEW
//#endif

// Change at your Own Peril

// 'G' 'h' '0' 's' 't' | PacketLen | UnZipLen
#define HDR_SIZE	13
#define FLAG_SIZE	5
#define HUERISTIC_VALUE 2
CRITICAL_SECTION CIOCPServer::m_cs;


unsigned __stdcall ThreadPoolFuncSP111 (LPVOID thisContext)    //�̳߳غ���
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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::CIOCPServer
// 
// DESCRIPTION:	C'tor initializes Winsock2 and miscelleanous events etc.
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
CIOCPServer::CIOCPServer()
{
	TRACE("CIOCPServer=%p\n",this);	

	// 
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);

	InitializeCriticalSection(&m_cs);     //��ʼ�� critical section  

	m_hThread		= NULL;    
	m_hKillEvent	= CreateEvent(NULL,   //SECURITY_ATTRIBUTES
								  TRUE,   //If this parameter is TRUE, the function creates a manual-reset event object
								  FALSE,  //If this parameter is TRUE, the initial state of the event object is signaled; otherwise, it is nonsignaled
								  NULL);  //The name of the event object
	m_socListen		= NULL;      // listen socket

	m_bTimeToKill		= false;
	m_bDisconnectAll	= false;

	m_hEvent		= NULL;
	m_hCompletionPort= NULL;

	m_bInit = false;
	m_nCurrentThreads	= 0;
	m_nBusyThreads		= 0;

	m_nSendKbps = 0;
	m_nRecvKbps = 0;

	m_nMaxConnections = 10000;
	m_nKeepLiveTime = 1000 * 60 * 3; // ������̽��һ��
	// Packet Flag;
	BYTE bPacketFlag[] = {'G', 'h', '0', 's', 't'};
	memcpy(m_bPacketFlag, bPacketFlag, sizeof(bPacketFlag));

	mpDB_Redious = new CDB_Redious();
	mpDB_Redious->Init("Provider=SQLOLEDB;Integrated Security=SSPI;Initial Catalog=SmalllSmartBilling;Data Source=(local)",30,30);
	mpDB_Redious->Connect();
}


////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::CIOCPServer
// 
// DESCRIPTION:	Tidy up
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
CIOCPServer::~CIOCPServer()
{
	try
	{
		Shutdown();
		WSACleanup();
	}catch(...){}
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	Init
// 
// DESCRIPTION:	Starts listener into motion
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
bool CIOCPServer::Initialize(NOTIFYPROC pNotifyProc, int nMaxConnections, int nPort)
{

	//TRACE(_T("Initialize()\n"));

	m_pNotifyProc	= pNotifyProc;    //���ûص�����
	m_nMaxConnections = nMaxConnections;     //���������

	//creates a socket that is bound to a specific transport-service provider
	m_socListen = WSASocket(AF_INET //The Internet Protocol version 4 (IPv4) 
						  , SOCK_STREAM  // type
						  , 0    //protocol  :  do not specify protocol
						  , NULL //protocol  info  ,  normal is NULL
						  , 0  // Group ID
						  , WSA_FLAG_OVERLAPPED   //Create a socket that supports overlapped I/O operations. 
						  );


	if (m_socListen == INVALID_SOCKET)
	{
		TRACE(_T("Could not create listen socket %ld\n"),WSAGetLastError());
		return false;
	}

	// Event for handling Network IO         
	m_hEvent = WSACreateEvent();

	if (m_hEvent == WSA_INVALID_EVENT)
	{
		TRACE(_T("WSACreateEvent() error %ld\n"),WSAGetLastError());
		closesocket(m_socListen);
		return false;
	}

	// The listener is ONLY interested in FD_ACCEPT
	// That is when a client connects to or IP/Port
	// Request async notification
	// �Ѹղŵ�event m_hEvent �� ֧��overlapped I/O ��socket ������
	int nRet = WSAEventSelect(m_socListen,
						  m_hEvent,
						  FD_ACCEPT);

	if (nRet == SOCKET_ERROR)
	{
		TRACE(_T("WSAAsyncSelect() error %ld\n"),WSAGetLastError());
		closesocket(m_socListen);
		return false;

	}

	SOCKADDR_IN		saServer;		


	// Listen on our designated Port#
	saServer.sin_port = htons(nPort);

	// Fill in the rest of the address structure
	saServer.sin_family = AF_INET;
	saServer.sin_addr.s_addr = INADDR_ANY;

	// bind our name to the socket       ��socket��ָ����IP�Ͷ˿�
	nRet = bind(m_socListen, 
				(LPSOCKADDR)&saServer, 
				sizeof(struct sockaddr));

	if (nRet == SOCKET_ERROR)
	{
		TRACE(_T("bind() error %ld\n"),WSAGetLastError());
		closesocket(m_socListen);
		return false;
	}

	// Set the socket to listen
	nRet = listen(m_socListen, SOMAXCONN);   // linsten ������
	if (nRet == SOCKET_ERROR)
	{
		TRACE(_T("listen() error %ld\n"),WSAGetLastError());
		closesocket(m_socListen);
		return false;
	}


	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	UINT	dwThreadId = 0;

	m_hThread =
			(HANDLE)_beginthreadex(NULL,				// Security
									 0,					// Stack size - use default
									 ListenThreadProc,  // Thread fn entry point
									 (void*) this,	    // pass parameter
									 0,					// Init flag
									 &dwThreadId);	// Thread address

	if (m_hThread != INVALID_HANDLE_VALUE)
	{
		InitializeIOCP();
		m_bInit = true;
		return true;
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::ListenThreadProc
// 
// DESCRIPTION:	Listens for incoming clients
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
unsigned CIOCPServer::ListenThreadProc(LPVOID lParam)
{
	CIOCPServer* pThis = reinterpret_cast<CIOCPServer*>(lParam);

	WSANETWORKEVENTS events;
	
	while(1)
	{
		//
		// Wait for something to happen
		//�����Ҫ��������̣��˳�
        if (WaitForSingleObject(pThis->m_hKillEvent, 100) == WAIT_OBJECT_0)
            break;

		DWORD dwRet;
		//function WSAWaitForMultipleEvents returns when one or all of the specified event objects are in the signaled state
		dwRet = WSAWaitForMultipleEvents(1,              //�ȴ���event count
									 &pThis->m_hEvent,
									 FALSE,
									 100,     // �ȴ�timeout 0.1 ��
									 FALSE);

		if (dwRet == WSA_WAIT_TIMEOUT)
			continue;

		//
		// Figure out what happened
		//
		int nRet = WSAEnumNetworkEvents(pThis->m_socListen,
								 pThis->m_hEvent,
								 &events);
		
		if (nRet == SOCKET_ERROR)
		{
			TRACE(_T("WSAEnumNetworkEvents error %ld\n"),WSAGetLastError());
			break;
		}

		// Handle Network events //
		// ACCEPT
		if (events.lNetworkEvents & FD_ACCEPT)    // �������¼��� FD_ACCEPT
		{
			if (events.iErrorCode[FD_ACCEPT_BIT] == 0)
				pThis->OnAccept();
			else
			{
				TRACE(_T("Unknown network event error %ld\n"),WSAGetLastError());
				break;
			}

		}

	} // while....

	return 0; // Normal Thread Exit Code...
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::OnAccept
// 
// DESCRIPTION:	Listens for incoming clients
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// Ulf Hedlund			 09072001			   Changes for OVERLAPPEDPLUS
////////////////////////////////////////////////////////////////////////////////
void CIOCPServer::OnAccept()
{

	SOCKADDR_IN	SockAddr;
	SOCKET		clientSocket;
	
	int			nRet;
	int			nLen;

	if (m_bTimeToKill || m_bDisconnectAll)
		return;

	//
	// accept the new socket descriptor
	//
	nLen = sizeof(SOCKADDR_IN);
	clientSocket = accept(m_socListen,
					    (LPSOCKADDR)&SockAddr,
						&nLen); 

	if (clientSocket == SOCKET_ERROR)
	{
		nRet = WSAGetLastError();
		if (nRet != WSAEWOULDBLOCK)
		{
			//
			// Just log the error and return
			//
			TRACE(_T("accept() error\n"),WSAGetLastError());
			return;
		}
	}


	// Create the Client context to be associted with the completion port
	//ÿһ��client �ж��Ե�context
	ClientContext* pContext = AllocateContext();
	// AllocateContext fail
	if (pContext == NULL)
		return;

    pContext->m_Socket = clientSocket;

	// Fix up In Buffer    ���������ý���buffer��len
	pContext->m_wsaInBuffer.buf = (char*)pContext->m_byInBuffer;
	pContext->m_wsaInBuffer.len = sizeof(pContext->m_byInBuffer);

   // Associate the new socket with a completion port.
   //����ÿһ���½����Ŀͻ������ӵ�ͬһ��IOCP m_hCompletionPort     �ǳ���Ҫ  
	if (!AssociateSocketWithCompletionPort(clientSocket, m_hCompletionPort, (DWORD) pContext))
    {
        delete pContext;
		pContext = NULL;

        closesocket( clientSocket );
        closesocket( m_socListen );
        return;
    }

	// �ر�nagle�㷨,����Ӱ�����ܣ���Ϊ����ʱ���ƶ�Ҫ���ͺܶ���������С�����ݰ�,Ҫ�����Ϸ���
	// �ݲ��رգ�ʵ���֪���������������кܴ�Ӱ��
	
	//const char chOpt = 1;

// 	int nErr = setsockopt(pContext->m_Socket, IPPROTO_TCP, TCP_NODELAY, &chOpt, sizeof(char));
// 	if (nErr == -1)
// 	{
// 		TRACE(_T("setsockopt() error\n"),WSAGetLastError());
// 		return;
// 	}

	// Set KeepAlive �����������
	//if (setsockopt(pContext->m_Socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&chOpt, sizeof(chOpt)) != 0)
	//{
	//	TRACE(_T("setsockopt() error\n"), WSAGetLastError());
	//}

	// ���ó�ʱ��ϸ��Ϣ
	//tcp_keepalive	klive;
	//klive.onoff = 1; // ���ñ���
	//klive.keepalivetime = m_nKeepLiveTime;
	//klive.keepaliveinterval = 1000 * 10; // ���Լ��Ϊ10�� Resend if No-Reply
	//WSAIoctl
	//	(
	//	pContext->m_Socket, 
	//	SIO_KEEPALIVE_VALS,
	//	&klive,
	//	sizeof(tcp_keepalive),
	//	NULL,
	//	0,
	//	(unsigned long *)&chOpt,
	//	0,
	//	NULL
	//	);

	CLock cs(m_cs, "OnAccept" );
	// Hold a reference to the context
	m_listContexts.push_back(pContext);


	// Trigger first IO Completion Request
	// Otherwise the Worker thread will remain blocked waiting for GetQueuedCompletionStatus...
	// The first message that gets queued up is ClientIoInitializing - see ThreadPoolFunc and 
	// IO_MESSAGE_HANDLER

	OVERLAPPEDPLUS	*pOverlap = new OVERLAPPEDPLUS(IOInitialize);

	//Posts an I/O completion packet to an I/O completion port  -------- PostQueuedCompletionStatus
	//ע�����ﴫ�ĵڶ���������0 �� Ҳ����˵�� ʵ�ʲ�û��recv��data

	BOOL bSuccess = PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD) pContext, &pOverlap->m_ol);
	
	if ( (!bSuccess && GetLastError( ) != ERROR_IO_PENDING))

	{            
        RemoveStaleClient(pContext,TRUE);
	    return;
    }

	m_pNotifyProc(pContext, NC_CLIENT_CONNECT);          //��Ҫ�������˻ص�����,Ϊʲô���������

	// Post to WSARecv Next
	PostRecv(pContext);               //�������������receive data from client
}


////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::InitializeIOCP
// 
// DESCRIPTION:	Create a dummy socket and associate a completion port with it.
//				once completion port is create we can dicard the socket
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
bool CIOCPServer::InitializeIOCP(void)
{

    SOCKET s;
    DWORD i;
    UINT  nThreadID;
    SYSTEM_INFO systemInfo;

    //
    // First open a temporary socket that we will use to create the
    // completion port.  In NT 3.51 it will not be necessary to specify
    // the FileHandle parameter of CreateIoCompletionPort()--it will
    // be legal to specify FileHandle as NULL.  However, for NT 3.5
    // we need an overlapped file handle.
    //

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( s == INVALID_SOCKET ) 
        return false;

    // Create the completion port that will be used by all the worker
    // threads.
    m_hCompletionPort = CreateIoCompletionPort( (HANDLE)s, NULL, 0, 0 );
    if ( m_hCompletionPort == NULL ) 
	{
        closesocket( s );
        return false;
    }

    // Close the socket, we don't need it any longer.
    closesocket( s );

    // Determine how many processors are on the system.
    GetSystemInfo( &systemInfo );

	m_nThreadPoolMin  = systemInfo.dwNumberOfProcessors * HUERISTIC_VALUE;
	m_nThreadPoolMax  = m_nThreadPoolMin;
	m_nCPULoThreshold = 10; 
	m_nCPUHiThreshold = 75; 

	m_cpu.Init();


    // We use two worker threads for eachprocessor on the system--this is choosen as a good balance
    // that ensures that there are a sufficient number of threads available to get useful work done 
	// but not too many that context switches consume significant overhead.
	//ÿcpu  2���߳�
	UINT nWorkerCnt = systemInfo.dwNumberOfProcessors * HUERISTIC_VALUE;

	// We need to save the Handles for Later Termination...
	HANDLE hWorker;
	m_nWorkerCnt = 0;

    for ( i = 0; i < nWorkerCnt; i++ ) 
	{
		hWorker = (HANDLE)_beginthreadex(NULL,					// Security
										0,						// Stack size - use default
										ThreadPoolFunc,     		// Thread fn entry point
										(void*) this,			// Param for thread
										0,						// Init flag
										&nThreadID);			// Thread address


        if (hWorker == NULL ) 
		{
            CloseHandle( m_hCompletionPort );
            return false;
        }

		m_nWorkerCnt++;

		CloseHandle(hWorker);
    }

	return true;
} 

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::ThreadPoolFunc 
// 
// DESCRIPTION:	This is the main worker routine for the worker threads.  
//				Worker threads wait on a completion port for I/O to complete.  
//				When it completes, the worker thread processes the I/O, then either pends 
//				new I/O or closes the client's connection.  When the service shuts 
//				down, other code closes the completion port which causes 
//				GetQueuedCompletionStatus() to wake up and the worker thread then 
//				exits.
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// Ulf Hedlund			 09062001              Changes for OVERLAPPEDPLUS
////////////////////////////////////////////////////////////////////////////////
unsigned CIOCPServer::ThreadPoolFunc (LPVOID thisContext)    //�̳߳غ���
{
	// Get back our pointer to the class
	ULONG ulFlags = MSG_PARTIAL;
	CIOCPServer* pThis = reinterpret_cast<CIOCPServer*>(thisContext);
	ASSERT(pThis);

    HANDLE hCompletionPort = pThis->m_hCompletionPort;
    
    DWORD dwIoSize;
    LPOVERLAPPED lpOverlapped;
    ClientContext* lpClientContext;
	OVERLAPPEDPLUS*	pOverlapPlus;
	bool			bError;
	bool			bEnterRead;

	InterlockedIncrement(&pThis->m_nCurrentThreads);
	InterlockedIncrement(&pThis->m_nBusyThreads);

	//
    // Loop round and round servicing I/O completions.
	// 

	for (BOOL bStayInPool = TRUE; bStayInPool && pThis->m_bTimeToKill == false; ) 
	{
		pOverlapPlus	= NULL;
		lpClientContext = NULL;
		bError			= false;
		bEnterRead		= false;
		// Thread is Block waiting for IO completion
		InterlockedDecrement(&pThis->m_nBusyThreads);


		// Get a completed IO request.    ����������,��complete port �ȴ�
		BOOL bIORet = GetQueuedCompletionStatus(
               hCompletionPort,      //A handle to the completion port
               &dwIoSize,  //A pointer to a variable that receives the number of bytes transferred during an I/O operation that has completed.
               (LPDWORD) &lpClientContext,
               &lpOverlapped, INFINITE);


		//printf("thread %d data recv bytes is %d \n" ,GetCurrentThreadId(), dwIoSize);
		DWORD dwIOError = GetLastError();
		pOverlapPlus = CONTAINING_RECORD(lpOverlapped, OVERLAPPEDPLUS, m_ol);


		int nBusyThreads = InterlockedIncrement(&pThis->m_nBusyThreads);

        if (!bIORet && dwIOError != WAIT_TIMEOUT )
		{
			if (lpClientContext && pThis->m_bTimeToKill == false)
			{
				pThis->RemoveStaleClient(lpClientContext, FALSE);
			}
			printf("WAIT_TIMEOUT\n");
			continue;

			// anyway, this was an error and we should exit
			bError = true;
		}
		//printf("there\n");
		if (!bError) 
		{
			
			// Allocate another thread to the thread Pool?
			if (nBusyThreads == pThis->m_nCurrentThreads)
			{
				if (nBusyThreads < pThis->m_nThreadPoolMax)
				{
					if (pThis->m_cpu.GetUsage() > pThis->m_nCPUHiThreshold)
					{
						UINT nThreadID = -1;

//						HANDLE hThread = (HANDLE)_beginthreadex(NULL,				// Security
//											 0,					// Stack size - use default
//											 ThreadPoolFunc,  // Thread fn entry point
///											 (void*) pThis,	    
//											 0,					// Init flag
//											 &nThreadID);	// Thread address

//						CloseHandle(hThread);
					}
				}
			}


			// Thread timed out - IDLE?
			if (!bIORet && dwIOError == WAIT_TIMEOUT)
			{
				if (lpClientContext == NULL)
				{
					if (pThis->m_cpu.GetUsage() < pThis->m_nCPULoThreshold)
					{
						// Thread has no outstanding IO - Server hasn't much to do so die
						if (pThis->m_nCurrentThreads > pThis->m_nThreadPoolMin)
							bStayInPool =  FALSE;
					}

					bError = true;
				}
			}
		}
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
		if (!bError)
		{
			if(bIORet && NULL != pOverlapPlus && NULL != lpClientContext) 
			{
				try
				{
					if(pOverlapPlus->m_ioType==IORead)
						TRACE(_T("this time is IOREAD()\n"));
					if(pOverlapPlus->m_ioType==IOWrite)
						TRACE(_T("this time is IOWrite()\n"));

					//if(pOverlapPlus->m_ioType==IORead)
					//	printf("this time is IOREAD()\n");
					//if(pOverlapPlus->m_ioType==IOWrite)
					//	printf("this time is IOWrite()\n");

					pThis->ProcessIOMessage(pOverlapPlus->m_ioType, lpClientContext, dwIoSize);
				}
				catch (...) {}
			}
		}

		if(pOverlapPlus)
			delete pOverlapPlus; // from previous call
    }

	InterlockedDecrement(&pThis->m_nWorkerCnt);

	InterlockedDecrement(&pThis->m_nCurrentThreads);
	InterlockedDecrement(&pThis->m_nBusyThreads);
   	return 0;
} 

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::Stop
// 
// DESCRIPTION:	Signal the listener to quit his thread
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
void CIOCPServer::Stop()
{
    ::SetEvent(m_hKillEvent);
    WaitForSingleObject(m_hThread, INFINITE);
	CloseHandle(m_hThread);
    CloseHandle(m_hKillEvent);
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::GetHostName
// 
// DESCRIPTION:	Get the host name of the connect client
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
CString CIOCPServer::GetHostName(SOCKET socket)
{
	sockaddr_in  sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));

	int nSockAddrLen = sizeof(sockAddr);
	
	BOOL bResult = getpeername(socket,(SOCKADDR*)&sockAddr, &nSockAddrLen);
	
	return bResult != INVALID_SOCKET ? inet_ntoa(sockAddr.sin_addr) : "";
}


void CIOCPServer::PostRecv(ClientContext* pContext)
{
	// issue a read request 
	OVERLAPPEDPLUS * pOverlap = new OVERLAPPEDPLUS(IORead);
	ULONG			ulFlags = MSG_PARTIAL;
	DWORD			dwNumberOfBytesRecvd;
	UINT nRetVal = WSARecv(pContext->m_Socket, 
		&pContext->m_wsaInBuffer,          // ���˽� client send ��data ���浽 m_wsaInBuffer
		1,
		&dwNumberOfBytesRecvd, 
		&ulFlags,
		&pOverlap->m_ol, 
		NULL);
	
	if ( nRetVal == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) 
	{
		RemoveStaleClient(pContext, FALSE);
	}
}
////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::Send
// 
// DESCRIPTION:	Posts a Write + Data to IO CompletionPort for transfer
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// Ulf Hedlund			 09062001			   Changes for OVERLAPPEDPLUS
////////////////////////////////////////////////////////////////////////////////
void CIOCPServer::Send(ClientContext* pContext, LPBYTE lpData, UINT nSize)
{
	if (pContext == NULL)
		return;

	try
	{
		if (nSize > 0)
		{
			// Compress data
			unsigned long	destLen = (double)nSize * 1.001  + 12;
			LPBYTE			pDest = new BYTE[destLen];


			// temp do not compress    ly 
			//int	nRet = compress(pDest, &destLen, lpData, nSize);
			//
			//if (nRet != Z_OK)
			//{
			//	delete [] pDest;
			//	return;
			//}

			//////////////////////////////////////////////////////////////////////////
			LONG nBufLen = destLen + HDR_SIZE;
			// 5 bytes packet flag
			pContext->m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
			// 4 byte header [Size of Entire Packet]
			pContext->m_WriteBuffer.Write((PBYTE) &nBufLen, sizeof(nBufLen));
			// 4 byte header [Size of UnCompress Entire Packet]
			pContext->m_WriteBuffer.Write((PBYTE) &nSize, sizeof(nSize));
			// Write Data
			pContext->m_WriteBuffer.Write(pDest, destLen);
			delete [] pDest;
			
			// ��������ٱ�������, ��Ϊ�п�����m_ResendWriteBuffer�����ڷ���,���Բ�ֱ��д��
			LPBYTE lpResendWriteBuffer = new BYTE[nSize];
			CopyMemory(lpResendWriteBuffer, lpData, nSize);
			pContext->m_ResendWriteBuffer.ClearBuffer();
			pContext->m_ResendWriteBuffer.Write(lpResendWriteBuffer, nSize);	// ���ݷ��͵�����
			delete [] lpResendWriteBuffer;
		}
		else // Ҫ���ط�
		{
			pContext->m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
			pContext->m_ResendWriteBuffer.ClearBuffer();
			pContext->m_ResendWriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));	// ���ݷ��͵�����	
		}
		// Wait for Data Ready signal to become available
		WaitForSingleObject(pContext->m_hWriteComplete, INFINITE);

		// Prepare Packet
	 //	pContext->m_wsaOutBuffer.buf = (CHAR*) new BYTE[nSize];
	 //	pContext->m_wsaOutBuffer.len = pContext->m_WriteBuffer.GetBufferLen();

 		OVERLAPPEDPLUS * pOverlap = new OVERLAPPEDPLUS(IOWrite);
 		PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD) pContext, &pOverlap->m_ol);

		pContext->m_nMsgOut++;
	}catch(...){}
}


////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CClientListener::OnClientInitializing
// 
// DESCRIPTION:	Called when client is initailizing
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// Ulf Hedlund           09062001		       Changes for OVERLAPPEDPLUS
////////////////////////////////////////////////////////////////////////////////
bool CIOCPServer::OnClientInitializing(ClientContext* pContext, DWORD dwIoSize)
{
	// We are not actually doing anything here, but we could for instance make
	// a call to Send() to send a greeting message or something

	return true;		// make sure to issue a read after this
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::OnClientReading
// 
// DESCRIPTION:	Called when client is reading //   I  think is reading from client
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// Ulf Hedlund           09062001		       Changes for OVERLAPPEDPLUS
////////////////////////////////////////////////////////////////////////////////
bool CIOCPServer::OnClientReading(ClientContext* pContext, DWORD dwIoSize)
{
	//printf("thread %d there\n" , GetCurrentThreadId());
	//CLock cs(CIOCPServer::m_cs, "OnClientReading");    //��Ϊ���Ĵ��ڣ� OnClientReading ����������
	try
	{
		//////////////////////////////////////////////////////////////////////////

		static DWORD nLastTick = GetTickCount();
		static DWORD nBytes = 0;
		nBytes += dwIoSize;
		
		if (GetTickCount() - nLastTick >= 1000)
		{
			nLastTick = GetTickCount();
			InterlockedExchange((LPLONG)&(m_nRecvKbps), nBytes);
			nBytes = 0;
		}

		//////////////////////////////////////////////////////////////////////////
		//printf("thread %d there\n" , GetCurrentThreadId());
		if (dwIoSize == 0)
		{
			printf("dwIoSize is 0\n");
			RemoveStaleClient(pContext, FALSE);
			return false;
		}
		
		//printf("thread %d there\n" , GetCurrentThreadId());
		if (dwIoSize == FLAG_SIZE && memcmp(pContext->m_byInBuffer, m_bPacketFlag, FLAG_SIZE) == 0)
		{
			printf("thread %d ask resend \n" , GetCurrentThreadId());
			// ���·���
			Send(pContext, pContext->m_ResendWriteBuffer.GetBuffer(), pContext->m_ResendWriteBuffer.GetBufferLen());
			// ������Ͷ��һ����������
			PostRecv(pContext);
			return true;
		}

		// Add the message to out message
		// Dont forget there could be a partial, 1, 1 or more + partial mesages

		/*  ����������send �������ֶ�д��  client context �� buffer ���棬 dwIoSize �� client send ʱ���͵��ֽ�
		�����  iResult = send(ConnectSocket, request, 14, 0 );   dwIoSize �͵���14
		*/
		pContext->m_CompressionBuffer.Write(pContext->m_byInBuffer,dwIoSize);
			
		m_pNotifyProc( pContext, NC_RECEIVE);


		// Check real Data
		//while (pContext->m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
		while (pContext->m_CompressionBuffer.GetBufferLen() > 10)
		{
			BYTE bPacketFlag[FLAG_SIZE];
			BYTE bPacketData[6];
			CopyMemory(bPacketFlag, pContext->m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));

			if (memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag)) != 0)
				throw "bad buffer";


			int nUnCompressLength = 0;
			// Read off header
			pContext->m_CompressionBuffer.Read((PBYTE) bPacketFlag, sizeof(bPacketFlag));
			pContext->m_CompressionBuffer.Read((PBYTE) bPacketData, 6);

			time_t timer;
			char buffer[26];
			struct tm* tm_info;

			time(&timer);
			tm_info = localtime(&timer);

			strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);


			printf("thread %d this time data recv is %s at time %s\n",GetCurrentThreadId(),bPacketData,buffer);

			nUnCompressLength = 1;

			// create thread to call procedure 


			//UINT  nThreadID;
			//HANDLE hWorker;


			//hWorker = (HANDLE)_beginthreadex(NULL,					// Security
			//								0,						// Stack size - use default
			//								ThreadPoolFuncSP111,     		// Thread fn entry point
			//								(void*) bPacketData,			// Param for thread
			//								0,						// Init flag
			//								&nThreadID);			// Thread address


			//if (hWorker == NULL ) 
			//{
			//	//CloseHandle( m_hCompletionPort );
			//	return false;
			//}

			//CloseHandle(hWorker);
			

			PostRecv(pContext);

			int				rtn;
			std::string	msg;
			//CDB_Redious*    mpDB_Redious = new CDB_Redious();
			//mpDB_Redious->Init("Provider=SQLOLEDB;Integrated Security=SSPI;Initial Catalog=SmalllSmartBilling;Data Source=(local)",30,30);
			//if(mpDB_Redious->Connect())
			//{
			//	//printf("thread %d connect success\n" , GetCurrentThreadId());
			//}
			//else
			//{
			//	printf("thread %d connect fail\n" , GetCurrentThreadId());
			//}

			mpDB_Redious->sp_Test_alwayRetTrue(1,&rtn,&msg);

		
			//time_t timer;
			//char buffer[26];
			//struct tm* tm_info;

			time(&timer);
			tm_info = localtime(&timer);

			strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);


			printf("sp in thread %d return %s at time %s\n" , GetCurrentThreadId() , msg.c_str(),buffer);
		

		}
		// Post to WSARecv Next
		//PostRecv(pContext);
	}catch(...)
	{
		//std::cout << ex.what() << std::endl;
		//printf("thread %d exception happen at %s and %d \n" , GetCurrentThreadId(),__FILE__, __LINE__);
		printf("thread %d exception happen \n" , GetCurrentThreadId());
		pContext->m_CompressionBuffer.ClearBuffer();
		// Ҫ���ط����ͷ���0, �ں��Զ����������־
		Send(pContext, NULL, 0);
		PostRecv(pContext);
	}



	//	// Check real Data
	//	while (pContext->m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
	//	{
	//		BYTE bPacketFlag[FLAG_SIZE];
	//		CopyMemory(bPacketFlag, pContext->m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));

	//		if (memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag)) != 0)
	//			throw "bad buffer";

	//		int nSize = 0;
	//		CopyMemory(&nSize, pContext->m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(int));
	//		
	//		// Update Process Variable
	//		pContext->m_nTransferProgress = pContext->m_CompressionBuffer.GetBufferLen() * 100 / nSize;

	//		if (nSize && (pContext->m_CompressionBuffer.GetBufferLen()) >= nSize)
	//		{
	//			int nUnCompressLength = 0;
	//			// Read off header
	//			pContext->m_CompressionBuffer.Read((PBYTE) bPacketFlag, sizeof(bPacketFlag));

	//			pContext->m_CompressionBuffer.Read((PBYTE) &nSize, sizeof(int));
	//			pContext->m_CompressionBuffer.Read((PBYTE) &nUnCompressLength, sizeof(int));
	//			
	//			////////////////////////////////////////////////////////
	//			////////////////////////////////////////////////////////
	//			// SO you would process your data here
	//			// 
	//			// I'm just going to post message so we can see the data
	//			int	nCompressLength = nSize - HDR_SIZE;
	//			PBYTE pData = new BYTE[nCompressLength];
	//			PBYTE pDeCompressionData = new BYTE[nUnCompressLength];
	//			
	//			if (pData == NULL || pDeCompressionData == NULL)
	//				throw "bad Allocate";

	//			pContext->m_CompressionBuffer.Read(pData, nCompressLength);

	//			//////////////////////////////////////////////////////////////////////////
	//			unsigned long	destLen = nUnCompressLength;


	//			//temp do not uncompress
	//			//int	nRet = uncompress(pDeCompressionData, &destLen, pData, nCompressLength);
	//			////////////////////////////////////////////////////////////////////////////
	//			//if (nRet == Z_OK)
	//			//{
	//			//	pContext->m_DeCompressionBuffer.ClearBuffer();
	//			//	pContext->m_DeCompressionBuffer.Write(pDeCompressionData, destLen);
	//			//	m_pNotifyProc((LPVOID) m_pFrame, pContext, NC_RECEIVE_COMPLETE);
	//			//}
	//			//else
	//			//{
	//			//	throw "bad buffer";
	//			//}

	//			delete [] pData;
	//			delete [] pDeCompressionData;
	//			pContext->m_nMsgIn++;
	//		}
	//		else
	//			break;
	//	}
	//	// Post to WSARecv Next
	//	PostRecv(pContext);
	//}catch(...)
	//{
	//	pContext->m_CompressionBuffer.ClearBuffer();
	//	// Ҫ���ط����ͷ���0, �ں��Զ����������־
	//	Send(pContext, NULL, 0);
	//	PostRecv(pContext);
	//}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::OnClientWriting
// 
// DESCRIPTION:	Called when client is writing
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// Ulf Hedlund           09062001		       Changes for OVERLAPPEDPLUS
////////////////////////////////////////////////////////////////////////////////
bool CIOCPServer::OnClientWriting(ClientContext* pContext, DWORD dwIoSize)
{
	TRACE(_T("OnClientWriting()\n"));
	try
	{
		//////////////////////////////////////////////////////////////////////////
		static DWORD nLastTick = GetTickCount();
		static DWORD nBytes = 0;
		
		nBytes += dwIoSize;
		
		if (GetTickCount() - nLastTick >= 1000)
		{
			nLastTick = GetTickCount();
			InterlockedExchange((LPLONG)&(m_nSendKbps), nBytes);
			nBytes = 0;
		}
		//////////////////////////////////////////////////////////////////////////

		ULONG ulFlags = MSG_PARTIAL;

		// Finished writing - tidy up
		pContext->m_WriteBuffer.Delete(dwIoSize);
		if (pContext->m_WriteBuffer.GetBufferLen() == 0)
		{
			pContext->m_WriteBuffer.ClearBuffer();
			// Write complete
			SetEvent(pContext->m_hWriteComplete);
			return true;			// issue new read after this one
		}
		else
		{
			OVERLAPPEDPLUS * pOverlap = new OVERLAPPEDPLUS(IOWrite);

			m_pNotifyProc( pContext, NC_TRANSMIT);


			pContext->m_wsaOutBuffer.buf = (char*) pContext->m_WriteBuffer.GetBuffer();
			pContext->m_wsaOutBuffer.len = pContext->m_WriteBuffer.GetBufferLen();

			int nRetVal = WSASend(pContext->m_Socket, 
							&pContext->m_wsaOutBuffer,
							1,
							&pContext->m_wsaOutBuffer.len, 
							ulFlags,
							&pOverlap->m_ol, 
							NULL);


			if ( nRetVal == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
			{
				RemoveStaleClient( pContext, FALSE );
				printf("send back to client error\n");
			}
			printf("send back to client\n");


		}
	}catch(...){}
	return false;			// issue new read after this one
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::CloseCompletionPort
// 
// DESCRIPTION:	Close down the IO Complete Port, queue and associated client context structs
//				which in turn will close the sockets...
//				
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
void CIOCPServer::CloseCompletionPort()
{

	while (m_nWorkerCnt)
	{
		PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD) NULL, NULL);
		Sleep(100);
	}

	// Close the CompletionPort and stop any more requests
	CloseHandle(m_hCompletionPort);

	ClientContext* pContext = NULL;

	//do 
	//{
	//	POSITION pos  = m_listContexts.pop_front();
	//	if (pos)
	//	{
	//		pContext = m_listContexts.GetNext(pos);			
	//		RemoveStaleClient(pContext, FALSE);
	//	}
	//}
	//while (!m_listContexts.IsEmpty());

	m_listContexts.clear();

}


BOOL CIOCPServer::AssociateSocketWithCompletionPort(SOCKET socket, HANDLE hCompletionPort, DWORD dwCompletionKey)
{
	HANDLE h = CreateIoCompletionPort((HANDLE) socket, hCompletionPort, dwCompletionKey, 0);
	return h == hCompletionPort;
}

////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::RemoveStaleClient
// 
// DESCRIPTION:	Client has died on us, close socket and remove context from our list
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
void CIOCPServer::RemoveStaleClient(ClientContext* pContext, BOOL bGraceful)
{
 //   CLock cs(m_cs, "RemoveStaleClient");

	//TRACE("CIOCPServer::RemoveStaleClient\n");

 //   LINGER lingerStruct;


 //   //
 //   // If we're supposed to abort the connection, set the linger value
 //   // on the socket to 0.
 //   //

 //   if ( !bGraceful ) 
	//{

 //       lingerStruct.l_onoff = 1;
 //       lingerStruct.l_linger = 0;
 //       setsockopt( pContext->m_Socket, SOL_SOCKET, SO_LINGER,
 //                   (char *)&lingerStruct, sizeof(lingerStruct) );
 //   }



 //   //
 //   // Free context structures
	//if (m_listContexts.find(pContext)) 
	//{

	//	//
	//	// Now close the socket handle.  This will do an abortive or  graceful close, as requested.  
	//	CancelIo((HANDLE) pContext->m_Socket);

	//	closesocket( pContext->m_Socket );
	//	pContext->m_Socket = INVALID_SOCKET;

 //       while (!HasOverlappedIoCompleted((LPOVERLAPPED)pContext)) 
 //               Sleep(0);

	//	m_pNotifyProc((LPVOID) m_pFrame, pContext, NC_CLIENT_DISCONNECT);

	//	MoveToFreePool(pContext);

	//}
}


void CIOCPServer::Shutdown()
{
	if (m_bInit == false)
		return;

	m_bInit = false;
	m_bTimeToKill = true;

	// Stop the listener
	Stop();


	closesocket(m_socListen);	
	WSACloseEvent(m_hEvent);


	CloseCompletionPort();
	
	DeleteCriticalSection(&m_cs);

 	//while (!m_listFreePool.empty())
 	//	delete m_listFreePool.RemoveTail();

}


////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::MoveToFreePool
// 
// DESCRIPTION:	Checks free pool otherwise allocates a context
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
void CIOCPServer::MoveToFreePool(ClientContext *pContext)
{
	CLock cs(m_cs, "MoveToFreePool");
    // Free context structures

	ContextList::iterator findIter = std::find(m_listContexts.begin(), m_listContexts.end(),pContext);
	//POSITION pos = m_listContexts.Find(pContext);
	
	if (findIter != m_listContexts.end()) 
	{
		pContext->m_CompressionBuffer.ClearBuffer();
		pContext->m_WriteBuffer.ClearBuffer();
		pContext->m_DeCompressionBuffer.ClearBuffer();
		pContext->m_ResendWriteBuffer.ClearBuffer();
		m_listFreePool.push_back(pContext);
		m_listContexts.remove(*findIter);
	}
}



////////////////////////////////////////////////////////////////////////////////
// 
// FUNCTION:	CIOCPServer::MoveToFreePool
// 
// DESCRIPTION:	Moves an 'used/stale' Context to the free pool for reuse
// 
// INPUTS:		
// 
// NOTES:	
// 
// MODIFICATIONS:
// 
// Name                  Date       Version    Comments
// N T ALMOND            06042001	1.0        Origin
// 
////////////////////////////////////////////////////////////////////////////////
ClientContext*  CIOCPServer::AllocateContext()
{
	ClientContext* pContext = NULL;

	CLock cs(CIOCPServer::m_cs, "AllocateContext");

	if (!m_listFreePool.empty())
	{
		pContext = m_listFreePool.front();
	}
	else
	{
		pContext = new ClientContext;
	}

	ASSERT(pContext);
	
	if (pContext != NULL)
	{

		ZeroMemory(pContext, sizeof(ClientContext));
		pContext->m_bIsMainSocket = false;
		memset(pContext->m_Dialog, 0, sizeof(pContext->m_Dialog));
	}
	return pContext;
}


void CIOCPServer::ResetConnection(ClientContext* pContext)
{

	CString strHost;
	ClientContext* pCompContext = NULL;

	CLock cs(CIOCPServer::m_cs, "ResetConnection");

	//POSITION pos  = m_listContexts.GetHeadPosition();
	//while (pos)
	//{
	//	pCompContext = m_listContexts.GetNext(pos);			
	//	if (pCompContext == pContext)
	//	{
	//		RemoveStaleClient(pContext, TRUE);
	//		break;
	//	}
	//}

	m_listContexts.clear();
}

void CIOCPServer::DisconnectAll()
{
	m_bDisconnectAll = true;
	CString strHost;
	ClientContext* pContext = NULL;

	CLock cs(CIOCPServer::m_cs, "DisconnectAll");

	//POSITION pos  = m_listContexts.GetHeadPosition();
	//while (pos)
	//{
	//	pContext = m_listContexts.GetNext(pos);			
	//	RemoveStaleClient(pContext, TRUE);
	//}

	m_listContexts.clear();

	m_bDisconnectAll = false;

}

bool CIOCPServer::IsRunning()
{
	return m_bInit;
}
