#include "userInfo.h"

void USERINFO::setAccount(const char* begin, const char* end)
{
	if (begin && end && std::distance(begin, end) > 0)
	{
		try
		{
			account.assign(begin, end);
		}
		catch (std::bad_alloc& e)
		{
			account.clear();
		}
	}
}



void USERINFO::setAccount(const std::string_view name)
{
	if (!name.empty())
	{
		try
		{
			account.assign(name.data(), name.data() + name.size());
		}
		catch (std::bad_alloc& e)
		{
			account.clear();
		}
	}
}



const std::string& USERINFO::getAccount()
{
	return account;
}


const std::string_view USERINFO::getAccountView()
{
	if (account.empty())
		return std::string_view();
	else
		return std::string_view(account.data(), account.size());
}



void USERINFO::clear()
{
	account.clear();
	level = 0;
}