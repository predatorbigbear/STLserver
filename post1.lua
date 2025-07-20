wrk.method = "POST"
wrk.headers["Content-Type"] = "application/x-www-form-urlencoded"
wrk.headers["Host"] = "www.baidu.com:81"
wrk.headers["Transfer-Encoding"] = " gzip, identity, deflate"
wrk.headers["Connection"] = "kl ,keep-alive"

wrk.body = "test=%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08%3A%3A%08&parameter=123&number=123456"