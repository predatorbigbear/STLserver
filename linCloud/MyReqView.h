#pragma once


#include "publicHeader.h"


struct MYREQVIEW
{
	MYREQVIEW() = default;

	MYREQVIEW& swap(MYREQVIEW &m1)
	{
		methodStr.swap(m1.methodStr);
		bodyStr.swap(m1.bodyStr);
		paraStr.swap(m1.paraStr);
		versionStr.swap(m1.versionStr);
		targetStr.swap(m1.targetStr);
		return *this;
	}


	void setMethod(const char *ch, const size_t len)
	{
		std::string_view s1{ ch, len };
		methodStr.swap(s1);
	}

	void setBody(const char *ch, const size_t len)
	{
		std::string_view s1{ ch, len };
		bodyStr.swap(s1);
	}

	void setPara(const char *ch, const size_t len)
	{
		std::string_view s1{ ch, len };
		paraStr.swap(s1);
	}


	void setVersion(const char *ch, const size_t len)
	{
		std::string_view s1{ ch, len };
		versionStr.swap(s1);
	}

	void setTarget(const char *ch, const size_t len)
	{
		std::string_view s1{ ch, len };
		targetStr.swap(s1);
	}

	const std::string_view& method()const
	{
		return { methodStr };
	}

	const std::string_view& body()const
	{
		return { bodyStr };
	}

	const std::string_view& para()const
	{
		return { paraStr };
	}

	const std::string_view& version()const
	{
		return { versionStr };
	}

	const std::string_view& target()const
	{
		return { targetStr };
	}

	int & method()
	{
		return methodNum;
	}
	

	~MYREQVIEW()
	{
		
	}

	void clear()
	{
		methodNum = 0;
		std::string_view s1{ nullptr,0 };
		methodStr.swap(s1);
		bodyStr.swap(s1);
		paraStr.swap(s1);
		versionStr.swap(s1);
		targetStr.swap(s1);
	}

private:
	std::string_view methodStr;
	std::string_view bodyStr;
	std::string_view paraStr;
	std::string_view versionStr;
	std::string_view targetStr;
	int methodNum{};
};


