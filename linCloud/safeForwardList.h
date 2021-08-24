#pragma once


#include "publicHeader.h"
#include "readBuffer.h"



template<typename KEY,typename VALUE>
struct SAFEMAP
{
	SAFEMAP();

	void insertMap(const KEY uuid,const VALUE &value);














private:
	map<KEY, VALUE> m_readMap;
	map<KEY, VALUE>::const_iterator iter;
	mutex m_readListMutex;
};



template<typename KEY, typename VALUE>
inline SAFEMAP<KEY, VALUE>::SAFEMAP()
{
}

template<typename KEY, typename VALUE>
inline void SAFEMAP<KEY, VALUE>::insertMap(const KEY uuid, const VALUE & value)
{
	try
	{
		std::lock_guard<mutex>m1{ m_readListMutex };
		m_readMap.insert(std::make_pair(uuid, value));
	}
	catch (const std::exception &e)
	{
		;
	}
}
