#pragma once

#include "publicHeader.h"


struct MYREQ
{
	MYREQ() = default;

	MYREQ& operator=(const MYREQ &m1)
	{
		methodStr.assign(m1.methodStr);
		bodyStr.assign(m1.bodyStr);
		versionStr.assign(m1.versionStr);
		targetStr.assign(m1.targetStr);
		innerHeaderMap = m1.innerHeaderMap;
		return *this;
	}

	MYREQ& swap(MYREQ &m1)
	{
		methodStr.swap(m1.methodStr);
		bodyStr.swap(m1.bodyStr);
		versionStr.swap(m1.versionStr);
		targetStr.swap(m1.targetStr);
		innerHeaderMap.swap(m1.innerHeaderMap);
		return *this;
	}


	void setMethod(const std::string::const_iterator iterBegin, const std::string::const_iterator iterEnd)
	{
		methodStr.assign(iterBegin, iterEnd);
	}

	void setBody(const std::string::const_iterator iterBegin, const std::string::const_iterator iterEnd)
	{
		bodyStr.assign(iterBegin, iterEnd);
	}


	void setVersion(const std::string::const_iterator iterBegin, const std::string::const_iterator iterEnd)
	{
		versionStr.assign(iterBegin, iterEnd);
	}

	void setTarget(const std::string::const_iterator iterBegin, const std::string::const_iterator iterEnd)
	{
		targetStr.assign(iterBegin, iterEnd);
	}

	void insertHeader(const std::string &header, const std::string &str)
	{
		innerHeaderMap.insert(make_pair(header, str));
	}

	const std::string& method()const
	{
		return { methodStr };
	}

	const std::string& body()const
	{
		return { bodyStr };
	}

	const std::string& version()const
	{
		return { versionStr };
	}

	const std::string& target()const
	{
		return { targetStr };
	}

	const std::map<std::string, std::string>& headerMap()const
	{
		return innerHeaderMap;
	}

	void clear()
	{
		methodStr.clear();
		bodyStr.clear();
		versionStr.clear();
		targetStr.clear();
		innerHeaderMap.clear();
	}

	~MYREQ()
	{
		innerHeaderMap.clear();
		methodStr.clear();
		bodyStr.clear();
		versionStr.clear();
		targetStr.clear();
	}

private:
	std::string methodStr;
	std::string bodyStr;
	std::string versionStr;
	std::string targetStr;
	std::map<std::string, std::string>innerHeaderMap;
};