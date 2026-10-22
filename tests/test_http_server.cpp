#include <gtest/gtest.h>
#include "http_server.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace {
struct http_client {
    aeon::client_socket socket;
    std::string received;
    bool eof = false;
    bool connect(int port) {
        return socket.connect("127.0.0.1", port) && socket.set_blocking(false) == 0;
    }
    bool send(const std::string& text) {
        return socket.write(text) == static_cast<int>(text.size());
    }
    void drain() {
        char buffer[4096];
        for (int i = 0; i < 32; ++i) {
            const int n = static_cast<int>(::recv(socket.sockfd, buffer, sizeof(buffer), 0));
            if (n > 0) received.append(buffer, static_cast<std::size_t>(n));
            else {
                if (n == 0) eof = true;
                if (n < 0) {
#ifdef NET_PLATFORM_WINDOWS
                    const auto error = ::WSAGetLastError();
                    if (error == WSAECONNRESET || error == WSAECONNABORTED) eof = true;
#else
                    if (errno == ECONNRESET || errno == ECONNABORTED) eof = true;
#endif
                }
                break;
            }
        }
    }
};

template<class F>
bool until(aeon_examples::http_server& server,
           const std::vector<http_client*>& clients,
           F done, int milliseconds = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (server.poll(5) < 0) return false;
        for (auto* c : clients) c->drain();
        if (done()) return true;
    }
    return done();
}
std::string request(std::string path = "/", std::string extra = "") {
    return "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n" + extra + "\r\n";
}
int count(const std::string& haystack, const std::string& needle) {
    int n = 0;
    for (std::size_t at = 0; (at = haystack.find(needle, at)) != std::string::npos;
         at += needle.size()) ++n;
    return n;
}
}

TEST(HttpServer, StartupRestartAndShutdown) {
    aeon_examples::http_server server;
    EXPECT_EQ(server.local_port(), -1);
    EXPECT_EQ(server.poll(0), -1);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    EXPECT_GT(server.local_port(), 0);
    EXPECT_FALSE(server.start("127.0.0.1", 0));
    server.stop();
    server.stop();
    EXPECT_FALSE(server.running());
    ASSERT_TRUE(server.start("127.0.0.1", 0));
}

TEST(HttpServer, GetHealthHasContentLengthAndKeepAlive) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client c;
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(c.send(request("/health")));
    ASSERT_TRUE(until(server, {&c}, [&]{return c.received.find("\r\n\r\nok\n") != std::string::npos;}));
    EXPECT_NE(c.received.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(c.received.find("Content-Length: 3\r\n"), std::string::npos);
    EXPECT_NE(c.received.find("Connection: keep-alive\r\n"), std::string::npos);
    EXPECT_EQ(server.client_count(), 1u);
}

TEST(HttpServer, FragmentedRequestHeaders) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client c;
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(c.send("GET /hea"));
    ASSERT_TRUE(until(server, {&c}, [&] { return server.client_count() == 1; }));
    EXPECT_TRUE(c.received.empty());
    ASSERT_TRUE(c.send("lth HTTP/1.1\r\nHost: localhost\r\n"));
    EXPECT_TRUE(c.received.empty());
    ASSERT_TRUE(c.send("\r\n"));
    ASSERT_TRUE(until(server, {&c}, [&]{return c.received.find("\r\n\r\nok\n") != std::string::npos;}));
}

TEST(HttpServer, OrderedPipeliningAndReuse) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client c;
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(c.send(request("/health") + request("/missing")));
    ASSERT_TRUE(until(server, {&c}, [&]{return count(c.received, "HTTP/1.1 ") == 2;}));
    EXPECT_TRUE(c.received.find("200 OK") < c.received.find("404 Not Found"));
    ASSERT_TRUE(c.send(request("/")));
    ASSERT_TRUE(until(server, {&c}, [&]{return count(c.received, "HTTP/1.1 ") == 3;}));
    EXPECT_EQ(server.client_count(), 1u);
}

TEST(HttpServer, HeadOmitsBodyButReportsLength) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client c;
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(c.send("HEAD /health HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    ASSERT_TRUE(until(server, {&c}, [&]{return c.received.find("\r\n\r\n") != std::string::npos;}));
    EXPECT_NE(c.received.find("Content-Length: 3\r\n"), std::string::npos);
    EXPECT_EQ(c.received.substr(c.received.find("\r\n\r\n") + 4), "");
}

TEST(HttpServer, CloseResponseDrainsBeforeSocketIsDestroyed) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client c;
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(c.send(request("/health", "Connection: close\r\n")));
    ASSERT_TRUE(until(server, {&c}, [&]{return c.eof;}));
    EXPECT_NE(c.received.find("\r\n\r\nok\n"), std::string::npos);
    EXPECT_NE(c.received.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(server.client_count(), 0u);
}

TEST(HttpServer, RejectsMissingHostAndDuplicateHost) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client a, b;
    ASSERT_TRUE(a.connect(server.local_port()));
    ASSERT_TRUE(b.connect(server.local_port()));
    ASSERT_TRUE(a.send("GET / HTTP/1.1\r\n\r\n"));
    ASSERT_TRUE(b.send(request("/", "Host: another\r\n")));
    ASSERT_TRUE(until(server, {&a,&b}, [&]{return a.eof && b.eof;}));
    EXPECT_NE(a.received.find("400 Bad Request"), std::string::npos);
    EXPECT_NE(b.received.find("400 Bad Request"), std::string::npos);
}

TEST(HttpServer, RejectsBodyFramingAndUnsupportedMethods) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client a, b, c;
    ASSERT_TRUE(a.connect(server.local_port()));
    ASSERT_TRUE(b.connect(server.local_port()));
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(a.send(request("/", "Content-Length: 123\r\n") + "abc"));
    ASSERT_TRUE(b.send(request("/", "Transfer-Encoding: chunked\r\n")));
    ASSERT_TRUE(c.send("POST / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    ASSERT_TRUE(until(server, {&a,&b,&c}, [&]{return a.eof && b.eof && c.eof;}));
    EXPECT_NE(a.received.find("413 Content Too Large"), std::string::npos);
    EXPECT_NE(b.received.find("501 Not Implemented"), std::string::npos);
    EXPECT_NE(c.received.find("405 Method Not Allowed"), std::string::npos);
    EXPECT_NE(c.received.find("Allow: GET, HEAD"), std::string::npos);
}

TEST(HttpServer, HeaderLimitAndHttp10Close) {
    aeon_examples::http_limits limits;
    limits.max_header_bytes = 128;
    aeon_examples::http_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client a, b;
    ASSERT_TRUE(a.connect(server.local_port()));
    ASSERT_TRUE(b.connect(server.local_port()));
    ASSERT_TRUE(a.send("GET / HTTP/1.1\r\nHost: localhost\r\nX-Long: " + std::string(200, 'z')));
    ASSERT_TRUE(b.send("GET /health HTTP/1.0\r\n\r\n"));
    ASSERT_TRUE(until(server, {&a,&b}, [&]{return a.eof && b.eof;}));
    EXPECT_NE(a.received.find("431 Request Header Fields Too Large"), std::string::npos);
    EXPECT_NE(b.received.find("200 OK"), std::string::npos);
    EXPECT_NE(b.received.find("Connection: close"), std::string::npos);
}

TEST(HttpServer, UnsupportedHttpVersionAndBadFraming) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client a, b;
    ASSERT_TRUE(a.connect(server.local_port()));
    ASSERT_TRUE(b.connect(server.local_port()));
    ASSERT_TRUE(a.send("GET / HTTP/2.0\r\nHost: localhost\r\n\r\n"));
    ASSERT_TRUE(b.send("GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n"));
    ASSERT_TRUE(until(server, {&a,&b}, [&]{return a.eof && b.eof;}));
    EXPECT_NE(a.received.find("505 HTTP Version Not Supported"), std::string::npos);
    EXPECT_NE(b.received.find("400 Bad Request"), std::string::npos);
}

TEST(HttpServer, TwoClientsIndependentAndPeerShutdown) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client a, b;
    ASSERT_TRUE(a.connect(server.local_port()));
    ASSERT_TRUE(b.connect(server.local_port()));
    ASSERT_TRUE(a.send(request("/")));
    ASSERT_TRUE(b.send(request("/health")));
    ASSERT_TRUE(until(server, {&a,&b}, [&]{return count(a.received,"HTTP/1.1 ")==1 && count(b.received,"HTTP/1.1 ")==1;}));
    EXPECT_NE(a.received.find("libAeon HTTP example"), std::string::npos);
    EXPECT_NE(b.received.find("\r\n\r\nok\n"), std::string::npos);
    a.socket.close();
    ASSERT_TRUE(until(server, {&b}, [&]{return server.client_count()==1;}));
    ASSERT_TRUE(b.send(request("/health")));
    ASSERT_TRUE(until(server, {&b}, [&]{return count(b.received,"HTTP/1.1 ")==2;}));
}

TEST(HttpServer, ClientLimitRejectsOnlyNewArrival) {
    aeon_examples::http_limits limits;
    limits.max_clients = 1;
    aeon_examples::http_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client a,b;
    ASSERT_TRUE(a.connect(server.local_port()));
    ASSERT_TRUE(until(server, {&a}, [&]{return server.client_count()==1;}));
    ASSERT_TRUE(b.connect(server.local_port()));
    ASSERT_TRUE(until(server, {&a,&b}, [&]{return b.eof;}));
    EXPECT_EQ(server.client_count(),1u);
    ASSERT_TRUE(a.send(request("/health")));
    ASSERT_TRUE(until(server, {&a}, [&]{return a.received.find("200 OK")!=std::string::npos;}));
}

TEST(HttpServer, ManyClientsShareTheSameApplicationPoller) {
    constexpr std::size_t total = 105;
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    std::vector<std::unique_ptr<http_client>> owners;
    std::vector<http_client*> clients;
    for (std::size_t i = 0; i < total; ++i) {
        auto c = std::make_unique<http_client>();
        ASSERT_TRUE(c->connect(server.local_port()));
        clients.push_back(c.get());
        owners.push_back(std::move(c));
        if (i % 16 == 15) ASSERT_GE(server.poll(0), 0);
    }
    ASSERT_TRUE(until(server, clients, [&]{return server.client_count()==total;}, 5000));
    for (auto* c : clients) ASSERT_TRUE(c->send(request("/health")));
    ASSERT_TRUE(until(server, clients, [&]{return std::all_of(clients.begin(),clients.end(),[](const http_client* c){return c->received.find("\r\n\r\nok\n")!=std::string::npos;});}, 6000));
    server.stop();
    EXPECT_EQ(server.client_count(),0u);
}

TEST(HttpServer, Http10ExplicitKeepAliveAndCorrectVersion) {
    aeon_examples::http_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client c;
    ASSERT_TRUE(c.connect(server.local_port()));
    ASSERT_TRUE(c.send("GET /health HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"));
    ASSERT_TRUE(until(server, {&c}, [&]{return count(c.received, "HTTP/1.0 200 OK")==1;}));
    EXPECT_NE(c.received.find("Connection: keep-alive"), std::string::npos);
    ASSERT_TRUE(c.send("GET /health HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"));
    ASSERT_TRUE(until(server, {&c}, [&]{return count(c.received, "HTTP/1.0 200 OK")==2;}));
}

TEST(HttpServer, IdleConnectionExpiresWithoutDisruptingServer) {
    aeon_examples::http_limits limits;
    limits.idle_timeout_ms = 100;
    aeon_examples::http_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client idle;
    ASSERT_TRUE(idle.connect(server.local_port()));
    ASSERT_TRUE(until(server, {&idle}, [&]{return server.client_count()==1;}));
    ASSERT_TRUE(until(server, {&idle}, [&]{return idle.eof;}, 2000));
    EXPECT_EQ(server.client_count(), 0u);
    http_client next;
    ASSERT_TRUE(next.connect(server.local_port()));
    ASSERT_TRUE(next.send(request("/health")));
    ASSERT_TRUE(until(server, {&next}, [&]{return next.received.find("200 OK")!=std::string::npos;}));
}

TEST(HttpServer, BoundedOutputRetiresOnlyOverloadedClient) {
    aeon_examples::http_limits limits;
    limits.max_queued_bytes = 256;
    aeon_examples::http_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    http_client oversized;
    ASSERT_TRUE(oversized.connect(server.local_port()));
    ASSERT_TRUE(oversized.send(request("/") + request("/") + request("/")));
    ASSERT_TRUE(until(server, {&oversized}, [&]{return oversized.eof;}));
    EXPECT_EQ(server.client_count(), 0u);
    http_client healthy;
    ASSERT_TRUE(healthy.connect(server.local_port()));
    ASSERT_TRUE(healthy.send(request("/health")));
    ASSERT_TRUE(until(server, {&healthy}, [&]{return healthy.received.find("200 OK")!=std::string::npos;}));
}
