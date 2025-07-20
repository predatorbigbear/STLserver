wrk.method = "POST"
wrk.headers["Content-Type"] = "application/x-www-form-urlencoded"
wrk.headers["Host"] = "www.baidu.com:81"
wrk.headers["Transfer-Encoding"] = " gzip, identity, deflate"
wrk.headers["Connection"] = "kl ,keep-alive"

wrk.body = "jsonType=number"