#include<filesystem>
#include<algorithm>
#include<vector>
#include<functional>
#include<iostream>
#include<iterator>
#include<tuple>
#include<fstream>
#include<memory>

using std::lexicographical_compare;
using std::unique_ptr;
using std::lower_bound;
using std::make_shared;
using std::shared_ptr;
using std::ifstream;
namespace fs = std::filesystem;
using std::distance;
using std::lower_bound;
using std::vector;
using std::cout;
using std::tuple;
using std::string;
using std::getline;
using std::make_tuple;
using std::get;
using std::ofstream;
using std::sort;
using std::copy;

//19
void prepare(const char *dirName, vector<shared_ptr<tuple<ifstream, string>>> &vec)
{
	if (!dirName)
		return;

	vec.clear();
	if (fs::exists(dirName) && fs::is_directory(dirName))
	{
		for (auto const &singleFile : fs::directory_iterator(dirName))
		{
			shared_ptr <tuple<ifstream, string>> ptr{ make_shared<tuple<ifstream, string>>(std::ifstream(singleFile.path().string(),std::ios::binary),"") };
			
			ifstream &file{ get<0>(*ptr) };

			if (file)
			{
				string &str{ std::get<1>(*ptr) };
				while (!file.eof())
				{
					getline(file, str);
					if (!str.empty() && str.size() >= 19)
					{
						vec.emplace_back(ptr);
						break;
					}
				}
			}
		}
	}
}


void makeNewLog(const char *newFileName, vector<shared_ptr<tuple<ifstream, string>>> &vec)
{
	static const string newLine{ "\r\n" };
	try
	{
		if (!newFileName || vec.empty())
			return;

		ofstream outFile;

		outFile.open(newFileName, std::ios::trunc);

		if (!outFile)
			return;

		outFile << "";

		outFile.close();

		outFile.open(newFileName, std::ios::app|std::ios::binary);

		if (!outFile)
			return;

		unsigned int bufferSize{ 0 }, maxSize{ 4096 };
		unique_ptr<char[]>buffer{ new char[maxSize] };
		char *bufferPtr{ buffer.get() };

		sort(vec.begin(), vec.end(), [](shared_ptr<tuple<ifstream, string>> left, shared_ptr<tuple<ifstream, string>> right)
		{
			string &leftStr{ get<1>(*left) }, &rightStr{ get<1>(*right) };
			return lexicographical_compare(leftStr.cbegin(), leftStr.cbegin() + 19, rightStr.cbegin(), rightStr.cbegin() + 19);
		});

		shared_ptr<tuple<ifstream, string>> checkPtr = *vec.begin(),tempPtr;

		vector<shared_ptr<tuple<ifstream, string>>>::iterator begin{ vec.begin() }, end{vec.end()},findIter;

		copy(begin + 1, end, begin);

		--end;

		string &str{ get<1>(*checkPtr) };

		if (bufferSize + str.size() + 2 < maxSize)
		{
			copy(str.cbegin(), str.cend(), bufferPtr);
			bufferPtr += str.size();
			copy(newLine.cbegin(), newLine.cend(), bufferPtr);
			bufferPtr += newLine.size();
			bufferSize += str.size() + newLine.size();
		}
		else
		{
			outFile << str <<"\r\n";
		}

		bool change{ false };

		while (begin != end)
		{
			ifstream &file(get<0>(*checkPtr));
			change = false;
			string &str{ get<1>(*checkPtr) };

			while (!file.eof())
			{
				getline(file, str);
				if (!str.empty() && str.size() >= 19)
				{
					change = true;
					break;
				}
			}


			if (change)
			{
				findIter = lower_bound(begin, end, checkPtr,[](shared_ptr<tuple<ifstream, string>> left, shared_ptr<tuple<ifstream, string>> right)
				{
					string &leftStr{ get<1>(*left) }, &rightStr{ get<1>(*right) };
					return lexicographical_compare(leftStr.cbegin(), leftStr.cbegin() + 19, rightStr.cbegin(), rightStr.cbegin() + 19);
				});

				if (findIter == begin)
				{
					
				}
				if (findIter == end)
				{
					*end = checkPtr;
					checkPtr = *begin;
					copy(begin + 1, end + 1, begin);

				}
				else
				{
					tempPtr = *begin;
					copy(begin + 1, findIter, begin);
					*(findIter - 1) = checkPtr;
					checkPtr = tempPtr;

				}
			}
			else
			{
				checkPtr = *begin;
				copy(begin + 1, end, begin);
				--end;

			}

			string &thisStr{ get<1>(*checkPtr) };
			if (bufferSize + thisStr.size() < maxSize)
			{
				copy(thisStr.cbegin(), thisStr.cend(), bufferPtr);
				bufferPtr += thisStr.size();
				copy(newLine.cbegin(), newLine.cend(), bufferPtr);
				bufferPtr += newLine.size();
				bufferSize += thisStr.size() + newLine.size();
			}
			else
			{
				if (bufferSize)
				{
					outFile.write(buffer.get(), bufferSize);
					bufferSize = 0;
					bufferPtr = buffer.get();
				}
				outFile << thisStr << "\r\n";
			}
		}

		if (bufferSize)
		{
			outFile.write(buffer.get(), bufferSize);
			bufferSize = 0;
			bufferPtr = buffer.get();
		}
	}
	catch (const std::exception &e)
	{
		return;
	}
}






int main()
{
	vector<shared_ptr<tuple<ifstream, string>>> vec;	
	prepare("D:/deng", vec);
	cout << vec.size() <<'\n';

	makeNewLog("D:/sortLog.txt", vec);
	
	return 0;
}