//------------------------------------------------------------------------------
// File: Event server impl.hpp
//
// Desc: Event server
//
// Copyright (c) 2014-2018 opencvr.com. All rights reserved.
//------------------------------------------------------------------------------

#ifndef __VSC_EVENT_SERVER_IMPL_H_
#define __VSC_EVENT_SERVER_IMPL_H_

inline VEventServerCallbackTask::VEventServerCallbackTask(Factory &pFactory)
:m_Factory(pFactory)
{
}

inline VEventServerCallbackTask::~VEventServerCallbackTask()
{

}

inline void VEventServerCallbackTask::PushEvent(VEventData &pData)
{
	m_Queue.Push(pData);

	return;
}
/* Register the call back for the event */
inline BOOL VEventServerCallbackTask::RegEventNotify(void * pData, FunctionEventNotify callback)
{
	XGuard guard(m_cMutex);
	m_NotifyMap[pData] = callback;

	return TRUE;
}
inline BOOL VEventServerCallbackTask::UnRegEventNotify(void * pData)
{
	XGuard guard(m_cMutex);
	m_NotifyMap.erase(pData);

	return TRUE;
}

inline void VEventServerCallbackTask::run()
{
	while(m_Queue.BlockingPeek() == true)
	{
		VEventData sData = m_Queue.Pop();
		VDC_DEBUG( "%s Pop a Event \n",__FUNCTION__);
		/* Call the callback */
		XGuard guard(m_cMutex);
		FunctionEventNotifyMap::iterator it = m_NotifyMap.begin(); 
		for(; it!=m_NotifyMap.end(); ++it)
		{
			if ((*it).second)
			{
				(*it).second((*it).first, sData);
			}
		}
	}
}

inline VEventServerDbTask::VEventServerDbTask(Factory &pFactory)
:m_Factory(pFactory), m_pSqlSession(NULL), m_nYear(0), m_nMonth(0)
{
}

inline VEventServerDbTask::~VEventServerDbTask()
{
	if (m_pSqlSession)
	{
		delete m_pSqlSession;
	}
}

inline void VEventServerDbTask::PushEvent(VEventData &pData)
{
	m_Queue.Push(pData);

	return;
}


inline void VEventServerDbTask::UpdateDBSession(bool bIsFirst)
{
	std::time_t pTime = time(NULL);

	/* Get the Event Server Conf */
	VidEventDBConf cDBConf;
	m_Factory.GetEventDBConf(cDBConf);

	Poco::Timestamp pTimeStamp = Poco::Timestamp::fromEpochTime(pTime);

	/* Use next sec to check  */
	Poco::DateTime pTimeTime(pTimeStamp);

	int nYear = pTimeTime.year();
	int nMonth = pTimeTime.month();
	int nHour = pTimeTime.hour();

	if (bIsFirst != true)
	{

		/* Every day check if need new database */
		if (pTime%3600 != 0 || nHour != 0)
		{
			return;
		}

		if (nYear == m_nYear && nMonth == m_nMonth)
		{
			return;
		}
	}

	m_nYear = nYear;
	m_nMonth = nMonth;

	char strPlus[1024];
	sprintf(strPlus, "%d_%d", m_nYear, nMonth);
	
	if (cDBConf.ntype() == VID_DB_FIREBIRD)
	{
		//astring strEventDB = cDBConf.strdbpath() + "eventdb" + strPlus + ".db";
		astring strEventDB = "service=" + cDBConf.strdbpath() + "eventdb" + ".db";
		if (m_pSqlSession != NULL)
		{
			delete m_pSqlSession;
			m_pSqlSession = NULL;
		}
		m_pSqlSession = new soci::session(soci::firebird, strEventDB);
		*m_pSqlSession << "create table if not exists events (id integer primary key,"
			" strDevice text, strId text, strDevname text, strType text, nEvttime integer, "
			"strEvttimestr date, strDesc text)";

	}else
	{
		VDC_DEBUG( "%s Do not support the DB TYPE \n",__FUNCTION__);
		ve_sleep(1000);
	}	
}

inline void VEventServerDbTask::run()
{
	/* Update Session first */
	UpdateDBSession(true);
	
	/* Check if need  create new DB file*/
	while(m_Queue.BlockingPeek() == true)
	{
		
		VEventData sData = m_Queue.Pop();
		VDC_DEBUG( "%s Pop a Event \n",__FUNCTION__);
		/* Write the Data to DB */
		std::time_t pTime = time(NULL);
		if (pTime%3600 == 0)
		{
			UpdateDBSession(false);
		}
		/* Insert the data to db */
		*m_pSqlSession << "insert into events values(NULL, :strDevice, :strId,"
			" :strDevname :strType :nEvttime :strEvttimestr :strDesc)", 
			soci::use(sData.strDevice, "strDevice"), soci::use(sData.strId, "strId"), soci::use(sData.strDeviceName, "strDevname"),
			soci::use(sData.strType, "strType"), soci::use(sData.nTime, "nEvttime"), soci::use(sData.strTime, "strEvttimestr"),
			soci::use(sData.strDesc, "strDesc");
	}
}
		


inline VEventServer::VEventServer(Factory &pFactory)
:m_Factory(pFactory), m_DbTask(pFactory), m_CallbackTask(pFactory)
{
}

inline VEventServer::~VEventServer()
{

}

inline void VEventServer::PushEvent(VEventData &pData)
{
	m_DbTask.PushEvent(pData);
	m_CallbackTask.PushEvent(pData);

	return;
}
/* Register the call back for the event */
inline BOOL VEventServer::RegEventNotify(void * pData, FunctionEventNotify callback)
{
	return m_CallbackTask.RegEventNotify(pData, callback);
}
inline BOOL VEventServer::UnRegEventNotify(void * pData)
{
	return m_CallbackTask.UnRegEventNotify(pData);
}

inline BOOL VEventServer::Init()
{
	m_DbTask.start();
	m_CallbackTask.start();

	return TRUE;
}


#endif // __VSC_EVENT_SERVER_IMPL_H_