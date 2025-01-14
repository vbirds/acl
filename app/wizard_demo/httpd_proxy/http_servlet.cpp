#include "stdafx.h"
#include "tcp_transfer.h"
#include "http_transfer.h"
#include "http_servlet.h"

http_servlet::http_servlet(acl::socket_stream* stream, acl::session* session,
	int port /* 80 */)
: acl::HttpServlet(stream, session)
, port_(port)
{
	handlers_["/hello"] = &http_servlet::on_hello;
}

http_servlet::~http_servlet(void)
{
}

bool http_servlet::doError(request_t&, response_t& res)
{
	res.setStatus(400);
	res.setContentType("text/xml; charset=utf-8");

	// 发送 http 响应体
	acl::string buf;
	buf.format("<root error='some error happened!' />\r\n");
	res.write(buf);
	res.write(NULL, 0);
	return false;
}

bool http_servlet::doOther(request_t&, response_t& res, const char* method)
{
	res.setStatus(400);
	res.setContentType("text/xml; charset=utf-8");
	// 发送 http 响应体
	acl::string buf;
	buf.format("<root error='unkown request method %s' />\r\n", method);
	res.write(buf);
	res.write(NULL, 0);
	return false;
}

bool http_servlet::doGet(request_t& req, response_t& res)
{
	const char* path = req.getPathInfo();
	handler_t handler = path && *path ? handlers_[path] : NULL;
	return handler ? (this->*handler)(req, res) : transfer_get(req, res);
}

bool http_servlet::doPost(request_t& req, response_t& res)
{
	const char* path = req.getPathInfo();
	handler_t handler = path && *path ? handlers_[path] : NULL;
	return handler ? (this->*handler)(req, res) : transfer_post(req, res);
}

bool http_servlet::on_hello(request_t& req, response_t& res)
{
	res.setContentType("text/html; charset=utf-8")	// 设置响应字符集
		.setKeepAlive(req.isKeepAlive())	// 设置是否保持长连接
		.setContentEncoding(true)		// 自动支持压缩传输
		.setChunkedTransferEncoding(true);	// 采用 chunk 传输方式

	acl::string buf;
	buf.format("<html><body>xxxxxxx<br>\r\n");
	if (res.write(buf) == false) {
		printf("write error\r\n");
		return false;
	}

	acl::json* json = req.getJson();
	if (json == NULL) {
		printf("json null\r\n");
	} else {
		printf("json is [%s]\r\n", json->to_string().c_str());
	}
	for (size_t i = 0; i < 1; i++) {
		buf.format("hello world=%d<br>\r\n", (int) i);
		if (res.write(buf) == false) {
			printf("write error\r\n");
			return false;
		}

		if (i % 10000 == 0)
		{
			sleep(1);
			printf("i=%d\n", (int) i);
		}
	}

	buf = "</body></html><br>\r\n";
	printf("write ok\n");

	return res.write(buf) && res.write(NULL, 0);
}

bool http_servlet::transfer_get(request_t& req, response_t& res)
{
	http_transfer fiber_peer(acl::HTTP_METHOD_GET, req, res, port_);
	fiber_peer.start();

	bool keep_alive;
	fiber_peer.wait(&keep_alive);
	return keep_alive && req.isKeepAlive();
}

bool http_servlet::transfer_post(request_t& req, response_t& res)
{
	http_transfer fiber_peer(acl::HTTP_METHOD_POST, req, res, port_);
	fiber_peer.start();

	bool keep_alive;
	fiber_peer.wait(&keep_alive);

	printf("transfer_post finished\r\n");
	return keep_alive && req.isKeepAlive();
}

bool http_servlet::doConnect(request_t& req, response_t& res)
{
	// CONNECT 127.0.0.1:22 HTTP/1.0
	// HTTP/1.1 200 Connection Established

	const char* host = req.getRemoteHost();
	if (host == NULL || *host == 0) {
		logger_error("getRemoteHost null");
		return false;
	}

	printf("remote host=%s\r\n", host);

	acl::socket_stream peer;
	if (peer.open(host, 0, 0) == false) {
		logger_error("connect %s error %s", host, acl::last_serror());
		return false;
	}

	//const char* ok = "HTTP/1.1 200 Connection Established\r\n";
	//acl::ostream& out = res.getOutputStream();

	const char* ok = "";
	res.setContentLength(0);
	if (res.write(ok, 1) == false) {
		return false;
	}

	acl::socket_stream& local = req.getSocketStream();
	transfer_tcp(local, peer);
	return false;
}

bool http_servlet::transfer_tcp(acl::socket_stream& local, acl::socket_stream& peer)
{
	tcp_transfer fiber_local(local, peer);
	tcp_transfer fiber_peer(peer, local);

	fiber_local.set_peer(fiber_peer);
	fiber_peer.set_peer(fiber_local);

	fiber_local.start();
	fiber_peer.start();

	fiber_local.wait();
	fiber_peer.wait();

	printf("transfer_tcp finished, local fd=%d, peer fd=%d\r\n",
		fiber_local.get_input().sock_handle(),
		fiber_local.get_output().sock_handle());
	return true;
}
