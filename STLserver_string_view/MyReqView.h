#pragma once


#include<string_view>


struct MYREQVIEW
{
	MYREQVIEW() = default;

	MYREQVIEW& swap(MYREQVIEW &m1)
	{
		methodStr.swap(m1.methodStr);
		versionStr.swap(m1.versionStr);
		targetStr.swap(m1.targetStr);
		return *this;
	}


	void setMethod(const char *ch, const size_t len)
	{
		std::string_view s1{ ch, len };
		methodStr.swap(s1);
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
		std::string_view s1{};
		methodStr = s1;
		versionStr = s1;
		targetStr = s1;
	}

private:
	std::string_view methodStr;
	std::string_view versionStr;
	std::string_view targetStr;
	int methodNum{};
};


