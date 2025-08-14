#pragma once

#include<cstdio>


namespace STATICSTRING
{
	static const char *test{ "test" };
	
	static const char *parameter{ "parameter" };

	static const char *number{ "number" };
	
	static const char *selectWord{ "selectWord" };

	static const char *indexStart{ "indexStart" };

	static const char *indexEnd{ "indexEnd" };

	static const char *doubleQuotes{ "\"\"" };

	static const char *vertical{ "|" };

	static const char *httpReady{ "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:" };

	static const char *newline{ "\r\n\r\n" };

	static const char *smallNewline{ "\r\n" };

	static const char *writeWord{ "writeWord" };

	static const char *redisWord{ "redisWord" };

	static const char *forwardSlash{ "/" };

	static const char *loginHtml{ "login.html" };

	static const char *result{ "result" };

	static const char *expires{ "expires" };

	static const char *id{ "id" };

	static const char *name{ "name" };

	static const char *age{ "age" };

	static const char *province{ "province" };

	static const char *city{ "city" };

	static const char *country{ "country" };

	static const char *phone{ "phone" };

	static const char *username{ "username" };

	static const char *password{ "password" };

	static const char *hmget{ "hmget" };

	static const char *user{ "user" };

	static const char *session{ "session" };

	static const char *sessionExpire{ "sessionExpire" };

	static const char *get{ "get" };

	static const char *hmset{ "hmset" };

	static const char *path{ "path" };

	static const char *success{ "success" };

	static const char *sessionID{ "sessionID" };

	static const char *httpSetCookieExpireValue{ "DAY, DD MMM YYYY HH:MM:SS GMT" };
	
	static const char *book{ "book" };

	static const char* noResult{ "noResult" };
	/*
	https://blog.csdn.net/cosmoslife/article/details/8703696?utm_medium=distribute.pc_relevant.none-task-blog-baidujs_title-8&spm=1001.2101.3001.4242

	                    DAY
                            The day of the week (Sun, Mon, Tue, Wed, Thu, Fri, Sat).
                        DD
                            The day in the month (such as 01 for the first day of the month).
                        MMM
                            The three-letter abbreviation for the month (Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec).
                        YYYY
                            The year.
                        HH
                            The hour value in military time (22 would be 10:00 P.M., for example).
                        MM
                            The minute value.
                        SS
                            The second value.
	
	*/

	static const char* tm_wday[7]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

	//tm_mon   0-11
	static const char* tm_mon[12]{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	//tm_year  tm_year+1900 后转字符串
	//tm_mday  1-31   直接取tm_msec[value]
	//tm_hour  0-23   直接取tm_msec[value]
	//tm_min   0-59   直接取tm_msec[value]
	//tm_sec   0-60   直接取tm_msec[value]
	static const char* tm_sec[61]{ "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41","42","43","44","45","46","47","48","49","50","51","52","53","54","55","56","57","58","59","60" };

	static const char *GMT{ "GMT" };

	static const char *CST{ "CST" };

	static const char *publicKey{ "publicKey" };

	static const char *signature{ "signature" };

	static const char *ciphertext{ "ciphertext" };

	static const char *hashValue{ "hashValue" };

	static const char *randomString{ "randomString" };

	static const char *helloWorld{ "hello World" };

	static const char *encryptKey{ "encryptKey" };

	static const char *keyTime{ "keyTime" };

	static const char *hdel{ "hdel" };

	static const char *Max_Age{ "Max-Age" };

	static const char *str17{ "17" };

	static const char *success_upload{ "success upload" };

	static const char *jsonType{ "jsonType" };

	static constexpr char *keyValue{ "keyValue" };

	static constexpr char *doubleValue{ "doubleValue" };

	static constexpr char *emptyValue{ "emptyValue" };

	static constexpr char *jsonNull{ "null" };

	static constexpr char *jsonBoolean{ "Boolean" };

	static constexpr char *jsonTrue{ "true" };

	static constexpr char *jsonFalse{ "false" };

	static constexpr char *jsonNumber{ "number" };

	static constexpr char *jsonObject{ "object" };

	static constexpr char *jsonArray{ "array" };

	static constexpr char *emptyObject{ "emptyObject" };

	static constexpr char *emptyArray{ "emptyArray" };

	static constexpr char *objectArray{ "objectArray" };

	static constexpr char *valueArray{ "valueArray" };

	static constexpr char *doubleQuotation{ "\"" };

	static const char *fileLock{ "fileLock" };

	static const char *strlenStr{ "strlen" };

	static const char *lock{ "lock" };

	static const char *EX{ "EX" };

	static const char *NX{ "NX" };

	static const char *stringlen{ "stringlen" };

	static const char *textplainUtf8{ "text/plain; charset=UTF-8" };

	static const char *Date{ "Date" };

	static const char *Keep_Alive{ "Keep-Alive" };

	static const char* key{ "key" };

	static const char* answer{ "answer" };

	static const char* verifyCode{ "verifyCode" };

////////////////////////////////////////////////////////////////////////////////////////
	static size_t serverRSASize{ 128 };

	static size_t serverHashLen{ 32 };

	static size_t verifyCodeLen{ strlen(verifyCode) };

	static size_t answerLen{ strlen(answer) };

	static size_t noResultLen{ strlen(noResult) };

	static size_t testLen{ strlen(test) };
	
	static size_t parameterLen{ strlen(parameter) };
	
	static size_t numberLen{ strlen(number) };

	static size_t selectWordLen{ strlen(selectWord) };

	static size_t indexStartLen{ strlen(indexStart) };

	static size_t indexEndLen{ strlen(indexEnd) };

	static size_t doubleQuotesLen{ strlen(doubleQuotes) };

	static size_t verticalLen{ strlen(vertical) };

	static size_t httpReadyLen{ strlen(httpReady) };

	static size_t newlineLen{ strlen(newline) };

	static size_t smallNewlineLen{ strlen(smallNewline) };

	static size_t writeWordLen{ strlen(writeWord) };

	static size_t redisWordLen{ strlen(redisWord) };

	static size_t forwardSlashLen{ strlen(forwardSlash) };

	static size_t loginHtmlLen{ strlen(loginHtml) };

	static size_t resultLen{ strlen(result) };

	static size_t expiresLen{ strlen(expires) };

	static size_t idLen{ strlen(id) };

	static size_t nameLen{ strlen(name) };

	static size_t ageLen{ strlen(age) };

	static size_t provinceLen{ strlen(province) };

	static size_t cityLen{ strlen(city) };

	static size_t bookLen{ strlen(book) };

	static size_t countryLen{ strlen(country) };

	static size_t phoneLen{ strlen(phone) };

	static size_t usernameLen{ strlen(username) };

	static size_t passwordLen{ strlen(password) };

	static size_t hmgetLen{ strlen(hmget) };

	static size_t userLen{ strlen(user) };

	static size_t sessionLen{ strlen(session) };

	static size_t sessionExpireLen{ strlen(sessionExpire) };

	static size_t getLen{ strlen(get) };

	static size_t hmsetLen{ strlen(hmset) };

	static size_t successLen{ strlen(success) };

	static size_t sessionIDLen{ strlen(sessionID) };

	static size_t pathLen{ strlen(path) };

	static size_t httpSetCookieExpireValueLen{ strlen(httpSetCookieExpireValue) };

	static size_t tm_wdayLen{ 3 };

	static size_t tm_monLen{ 3 };

	static size_t tm_mdayLen{ 2 };

	static size_t tm_secLen{ 2 };

	static size_t tm_yearLen{ 4 };

	static size_t GMTLen{ strlen(GMT) };

	static constexpr time_t fourHourSec{ 14400 };

	static size_t publicKeyLen{ strlen(publicKey) };

	static size_t signatureLen{ strlen(signature) };

	static size_t ciphertextLen{ strlen(ciphertext) };

	static size_t hashValueLen{ strlen(hashValue) };

	static size_t randomStringLen{ strlen(randomString) };

	static size_t helloWorldLen{ strlen(helloWorld) };

	static size_t encryptKeyLen{ strlen(encryptKey) };

	static size_t keyTimeLen{ strlen(keyTime) };

	static size_t hdelLen{ strlen(hdel) };

	static size_t Max_AgeLen{ strlen(Max_Age) };

	static size_t str17Len{ strlen(str17) };

	static size_t success_uploadLen{ strlen(success_upload) };

	static size_t jsonTypeLen{ strlen(jsonType) };



	static constexpr size_t keyValueLen{ strlen(keyValue) };

	static constexpr size_t doubleValueLen{ strlen(doubleValue) };

	static constexpr size_t emptyValueLen{ strlen(emptyValue) };

	static constexpr size_t jsonNullLen{ strlen(jsonNull) };

	static constexpr size_t jsonBooleanLen{ strlen(jsonBoolean) };

	static constexpr size_t jsonTrueLen{ strlen(jsonTrue) };

	static constexpr size_t jsonFalseLen{ strlen(jsonFalse) };

	static constexpr size_t jsonNumberLen{ strlen(jsonNumber) };

	static constexpr size_t jsonObjectLen{ strlen(jsonObject) };

	static constexpr size_t jsonArrayLen{ strlen(jsonArray) };

	static constexpr size_t emptyObjectLen{ strlen(emptyObject) };

	static constexpr size_t emptyArrayLen{ strlen(emptyArray) };

	static constexpr size_t objectArrayLen{ strlen(objectArray) };

	static constexpr size_t valueArrayLen{ strlen(valueArray) };

	static constexpr size_t doubleQuotationLen{ strlen(doubleQuotation) };

	static size_t fileLockLen{ strlen(fileLock) };

	static size_t strlenStrLen{ strlen(strlenStr) };

	static size_t lockLen{ strlen(lock) };

	static size_t EXLen{ strlen(EX) };

	static size_t NXLen{ strlen(NX) };

	static size_t stringlenLen{ strlen(stringlen) };

	static size_t textplainUtf8Len{ strlen(textplainUtf8) };

	static size_t DateLen{ strlen(Date) };
	
	static size_t CSTLen{ strlen(CST) };

	static size_t Keep_AliveLen{ strlen(Keep_Alive) };

	static size_t keyLen{ strlen(key) };

	
};