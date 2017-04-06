
#include "IOCPLog.h"

    //-----------------------------------------------------------------------
    Log::Log( const std::string& name, bool debuggerOuput, bool suppressFile ) : 
        mLogLevel(LL_NORMAL), mDebugOut(debuggerOuput),
        mSuppressFile(suppressFile), mTimeStamp(true), mLogName(name)
    {
		if (!mSuppressFile)
		{
			mLog.open(name.c_str());    //mLog is std::ofstream 
		}
    }
    //-----------------------------------------------------------------------
    Log::~Log()
    {
		if (!mSuppressFile)
		{
	        mLog.close();
		}
    }
    //-----------------------------------------------------------------------
    void Log::logMessage( const std::string& message, LogMessageLevel lml, bool maskDebug )
    {
        if ((mLogLevel + lml) >= OGRE_LOG_THRESHOLD)
        {
			bool skipThisMessage = false;
            //for( mtLogListener::iterator i = mListeners.begin(); i != mListeners.end(); ++i )
            //    (*i)->messageLogged( message, lml, maskDebug, mLogName, skipThisMessage);
			
			if (!skipThisMessage)
			{
                if (mDebugOut && !maskDebug)
                    std::cerr << message << std::endl;

				// Write time into log
				if (!mSuppressFile)
				{
					if (mTimeStamp)
					{
						struct tm *pTime;
						time_t ctTime; time(&ctTime);
						pTime = localtime( &ctTime );
						mLog << std::setw(2) << std::setfill('0') << pTime->tm_hour
							<< ":" << std::setw(2) << std::setfill('0') << pTime->tm_min
							<< ":" << std::setw(2) << std::setfill('0') << pTime->tm_sec
							<< ": ";
					}
					mLog << message << std::endl;

					// Flush stcmdream to ensure it is written (incase of a crash, we need log to be up to date)
					mLog.flush();
				}
			}
        }
    }
    
    //-----------------------------------------------------------------------
    void Log::setTimeStampEnabled(bool timeStamp)
    {
        mTimeStamp = timeStamp;
    }

    //-----------------------------------------------------------------------
    void Log::setDebugOutputEnabled(bool debugOutput)
    {
        mDebugOut = debugOutput;
    }

	//-----------------------------------------------------------------------
    void Log::setLogDetail(LoggingLevel ll)
    {
        mLogLevel = ll;
    }

    //-----------------------------------------------------------------------
    void Log::addListener(LogListener* listener)
    {
        mListeners.push_back(listener);
    }

    //-----------------------------------------------------------------------
    void Log::removeListener(LogListener* listener)
    {
        mListeners.erase(std::find(mListeners.begin(), mListeners.end(), listener));
    }
	//---------------------------------------------------------------------
	Log::Stream Log::stream(LogMessageLevel lml, bool maskDebug) 
	{
		return Stream(this, lml, maskDebug);
	}

