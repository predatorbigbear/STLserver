#pragma once

#include "publicHeader.h"

template<typename KEY,typename VALUE>
struct SAFEUNORDEREDMAP
{
	bool insert(KEY &key,VALUE &value);

	bool insert(KEY &&key, VALUE &&value);

	bool erase(KEY &key);

	bool eraseAndGetElem(KEY &key, VALUE &v1);


private:
	std::unordered_map<KEY, VALUE> m_map;
	std::mutex m_mapMutex;
	typename std::unordered_map<KEY, VALUE>::const_iterator umapIter;
};



template<typename KEY, typename VALUE>
inline bool SAFEUNORDEREDMAP<KEY, VALUE>::insert(KEY &key, VALUE &value)
{
	try
	{
		std::lock_guard<std::mutex>m1{ m_mapMutex };
		m_map.insert(make_pair(key, value));
		return true;
	}
	catch(const std::exception &e)
	{
		return false;
	}
}



template<typename KEY, typename VALUE>
inline bool SAFEUNORDEREDMAP<KEY, VALUE>::insert(KEY && key, VALUE && value)
{
	try
	{
		std::lock_guard<std::mutex>m1{ m_mapMutex };
		m_map.insert(make_pair(std::forward<KEY>(key), std::forward<VALUE>(value)));
		return true;
	}
	catch (const std::exception &e)
	{
		return false;
	}
}



template<typename KEY, typename VALUE>
inline bool SAFEUNORDEREDMAP<KEY, VALUE>::erase(KEY & key)
{
	try
	{
		std::lock_guard<std::mutex>m1{ m_mapMutex };
		if (!m_map.empty())
		{
			m_map.erase(key);
		}
		return true;
	}
	catch (const std::exception &e)
	{
		return false;
	}
}

template<typename KEY, typename VALUE>
inline bool SAFEUNORDEREDMAP<KEY, VALUE>::eraseAndGetElem(KEY & key, VALUE & v1)
{
	try
	{
		std::lock_guard<std::mutex>m1{ m_mapMutex };
		if (!m_map.empty())
		{
			umapIter = m_map.find(key);
			if (umapIter != m_map.end())
			{
				v1 = umapIter->second;
				m_map.erase(umapIter);
			}
			else
				return false;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		return false;
	}
}


