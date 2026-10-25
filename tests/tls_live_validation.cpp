// M6c: real loopback TLS validation. No shell/OpenSSL CLI or external network.
// This binary is only built when ENABLE_SSL=ON and BUILD_TESTING=ON.
#include <aeon.hpp>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#include <chrono>
#ifndef NET_PLATFORM_WINDOWS
#include <csignal>
#include <pthread.h>
#endif
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

struct ephemeral_certificate {
    EVP_PKEY* key = nullptr;
    X509* cert = nullptr;
    SSL_CTX* server_ctx = nullptr;
    std::filesystem::path ca_path;
    std::filesystem::path key_path;

    explicit ephemeral_certificate(const std::filesystem::path& path, const char* san,
                                   bool expired = false)
        : ca_path(path), key_path(path.string() + ".key") {
        EVP_PKEY_CTX* generator = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        require(generator != nullptr, "EVP_PKEY_CTX_new_id");
        const bool generated = EVP_PKEY_keygen_init(generator) == 1 &&
            EVP_PKEY_CTX_set_rsa_keygen_bits(generator, 2048) == 1 &&
            EVP_PKEY_keygen(generator, &key) == 1;
        EVP_PKEY_CTX_free(generator);
        require(generated, "generate RSA test key");

        cert = X509_new();
        require(cert != nullptr, "X509_new");
        require(X509_set_version(cert, 2) == 1, "set cert version");
        require(ASN1_INTEGER_set(X509_get_serialNumber(cert), san[0] == 'I' ? 3 : 4) == 1, "set serial");
        require(X509_gmtime_adj(X509_getm_notBefore(cert), expired ? -7200 : -60) != nullptr, "notBefore");
        require(X509_gmtime_adj(X509_getm_notAfter(cert), expired ? -60 : 3600) != nullptr, "notAfter");
        require(X509_set_pubkey(cert, key) == 1, "set pubkey");
        X509_NAME* subject = X509_get_subject_name(cert);
        require(subject != nullptr, "subject name");
        require(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>(san), -1, -1, 0) == 1, "set CN");
        require(X509_set_issuer_name(cert, subject) == 1, "set issuer");

        X509V3_CTX ctx{};
        X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
        X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name,
                                                         const_cast<char*>(san));
        require(extension != nullptr, "create SAN");
        const int added = X509_add_ext(cert, extension, -1);
        X509_EXTENSION_free(extension);
        require(added == 1, "add SAN");
        require(X509_sign(cert, key, EVP_sha256()) > 0, "sign test cert");

        BIO* pem = BIO_new_file(ca_path.string().c_str(), "wb");
        require(pem != nullptr, "create CA file");
        const bool written = PEM_write_bio_X509(pem, cert) == 1;
        BIO_free(pem);
        require(written, "write CA file");
        BIO* private_pem = BIO_new_file(key_path.string().c_str(), "wb");
        require(private_pem != nullptr, "create private key file");
        const bool private_written = PEM_write_bio_PrivateKey(
            private_pem, key, nullptr, nullptr, 0, nullptr, nullptr) == 1;
        BIO_free(private_pem);
        require(private_written, "write private key file");

        server_ctx = SSL_CTX_new(TLS_server_method());
        require(server_ctx != nullptr, "SSL_CTX_new server");
        require(SSL_CTX_use_certificate(server_ctx, cert) == 1, "set server cert");
        require(SSL_CTX_use_PrivateKey(server_ctx, key) == 1, "set server key");
        require(SSL_CTX_check_private_key(server_ctx) == 1, "check server key");
    }
    ~ephemeral_certificate() {
        if (server_ctx) SSL_CTX_free(server_ctx);
        if (cert) X509_free(cert);
        if (key) EVP_PKEY_free(key);
    }
    ephemeral_certificate(const ephemeral_certificate&) = delete;
    ephemeral_certificate& operator=(const ephemeral_certificate&) = delete;
};

struct live_server {
    aeon::server_socket listener;
    SSL_CTX* ctx;
    int count;
    std::thread worker;
    int port = 0;

    explicit live_server(SSL_CTX* server_ctx, int connections = 1,
                         const char* bind_ip = "127.0.0.1")
        : ctx(server_ctx), count(connections) {
        require(listener.listen(bind_ip, 0), "server.listen");
        sockaddr_storage local{};
        socklen_t len = static_cast<socklen_t>(sizeof(local));
        require(::getsockname(listener.sockfd, reinterpret_cast<sockaddr*>(&local), &len) == 0,
                "getsockname");
        port = local.ss_family == AF_INET6
            ? ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port)
            : ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
        require(port > 0, "ephemeral port");
        require(listener.set_accept_timeout(1500) == 0, "set_accept_timeout");
        worker = std::thread([this] {
#ifndef NET_PLATFORM_WINDOWS
            // OpenSSL server-side alert writes may hit EPIPE after a rejected
            // client handshake. Block SIGPIPE in this test server thread only.
            sigset_t blocked{};
            sigemptyset(&blocked);
            sigaddset(&blocked, SIGPIPE);
            (void)pthread_sigmask(SIG_BLOCK, &blocked, nullptr);
#endif
            for (int i = 0; i < count; ++i) {
                aeon::socket* peer = listener.accept(true);
                if (!peer) return;
                (void)peer->set_read_timeout(2000);
                (void)peer->set_write_timeout(2000);
                SSL* session = SSL_new(ctx);
                if (session) {
                    if (SSL_set_fd(session, static_cast<int>(peer->sockfd)) == 1) {
                        if (SSL_accept(session) == 1) {
                            // Keep the accepted transport alive until the client
                            // closes. Otherwise immediate server close can race
                            // the client's SSL_connect/SSL_shutdown and SIGPIPE.
                            char one_byte{};
                            (void)SSL_read(session, &one_byte, 1);
                        }
                    }
                    SSL_free(session);
                }
                (void)listener.remove_peer(peer);
            }
        });
    }
    ~live_server() {
        if (worker.joinable()) worker.join();
        (void)listener.close();
    }
    live_server(const live_server&) = delete;
    live_server& operator=(const live_server&) = delete;
};

void case_test(const char* name, const std::function<void()>& test) {
    test();
    std::cout << "PASS " << name << '\n';
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path folder = fs::temp_directory_path() /
        ("aeon_m6c_tls_" + std::to_string(seed));
    fs::create_directories(folder);
    try {
        ephemeral_certificate good(folder / "valid.pem", "IP:127.0.0.1,DNS:localhost");
        ephemeral_certificate bad(folder / "wrong.pem", "DNS:wrong.example");
        ephemeral_certificate expired(folder / "expired.pem", "IP:127.0.0.1", true);

        case_test("trusted-ip-matches", [&] {
            live_server server(good.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0, "load trusted CA");
            require(client.connect("127.0.0.1", server.port), "valid strict IP cert rejected");
            require(client.connected, "connected state");
            (void)client.close();
        });
        case_test("trusted-ip-mismatch-rejected", [&] {
            live_server server(bad.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_ca_file(bad.ca_path.string().c_str()) == 0, "load trusted wrong CA");
            require(!client.connect("127.0.0.1", server.port), "mismatched IP was accepted");
            require(client.get_error() == aeon::ssl_error_cert, "wrong mismatch error");
            require(!client.connected && !client.is_valid_socket(), "failed handshake not closed");
        });
        case_test("trusted-dns-name-matches", [&] {
            aeon::server_socket probe;
            if (!probe.listen("::1", 0)) {
                std::cout << "SKIP trusted-dns-name-matches (IPv6 loopback disabled)\n";
                return;
            }
            (void)probe.close();
            live_server server(good.server_ctx, 1, "::1");
            aeon::ssl_client_socket client;
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0, "DNS CA");
            require(client.connect("localhost", server.port), "valid DNS SAN rejected");
        });
        case_test("trusted-dns-name-mismatch", [&] {
            aeon::server_socket probe;
            if (!probe.listen("::1", 0)) {
                std::cout << "SKIP trusted-dns-name-mismatch (IPv6 loopback disabled)\n";
                return;
            }
            (void)probe.close();
            live_server server(bad.server_ctx, 1, "::1");
            aeon::ssl_client_socket client;
            require(client.set_ca_file(bad.ca_path.string().c_str()) == 0, "DNS CA mismatch");
            require(!client.connect("localhost", server.port), "mismatched DNS SAN accepted");
            require(client.get_error() == aeon::ssl_error_cert, "DNS mismatch error");
        });
        case_test("untrusted-rejected-by-default", [&] {
            live_server server(good.server_ctx);
            aeon::ssl_client_socket client;
            require(!client.connect("127.0.0.1", server.port), "untrusted cert accepted by default");
            require(client.get_error() == aeon::ssl_error_cert, "wrong untrusted error");
        });
        case_test("explicit-self-signed-leaf-allowed", [&] {
            live_server server(good.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_allow_self_signed(true) == 0, "enable selfsigned");
            require(client.connect("127.0.0.1", server.port), "explicit selfsigned leaf rejected");
        });
        case_test("self-signed-does-not-bypass-identity", [&] {
            live_server server(bad.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_allow_self_signed(true) == 0, "enable selfsigned");
            require(!client.connect("127.0.0.1", server.port), "selfsigned bypassed IP mismatch");
            require(client.get_error() == aeon::ssl_error_cert, "wrong selfsigned mismatch error");
        });
        case_test("self-signed-does-not-bypass-expiration", [&] {
            live_server server(expired.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_allow_self_signed(true) == 0, "enable selfsigned");
            require(!client.connect("127.0.0.1", server.port), "expired selfsigned accepted");
            require(client.get_error() == aeon::ssl_error_cert, "expired certificate error");
        });
        case_test("verification-off-is-explicit", [&] {
            live_server server(bad.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_peer_verification(aeon::ssl_verify_mode::off) == 0, "verify off");
            require(client.connect("127.0.0.1", server.port), "verify off should connect");
        });
        case_test("direct-address-verifies-ip", [&] {
            live_server server(bad.server_ctx);
            sockaddr_in ip{};
            ip.sin_family = AF_INET;
            ip.sin_port = htons(static_cast<std::uint16_t>(server.port));
            require(inet_pton(AF_INET, "127.0.0.1", &ip.sin_addr) == 1, "inet_pton");
            aeon::ssl_client_socket client;
            require(client.set_ca_file(bad.ca_path.string().c_str()) == 0, "load CA");
            require(!client.connect(aeon::address(ip)), "direct address bypassed IP check");
            require(client.get_error() == aeon::ssl_error_cert, "wrong direct address error");
        });
        case_test("reconnect-after-rejected-handshake", [&] {
            live_server wrong(bad.server_ctx);
            live_server correct(good.server_ctx);
            aeon::ssl_client_socket client;
            require(client.set_ca_file(bad.ca_path.string().c_str()) == 0, "load wrong CA");
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0, "load good CA");
            require(!client.connect("127.0.0.1", wrong.port), "wrong cert accepted");
            require(client.connect("127.0.0.1", correct.port), "reconnect after rejection failed");
        });
        case_test("explicit-close-then-reconnect", [&] {
            live_server server(good.server_ctx, 2);
            aeon::ssl_client_socket client;
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0, "load CA");
            require(client.connect("127.0.0.1", server.port), "first connect");
            require(static_cast<aeon::socket*>(&client)->close(), "base virtual close after handshake");
            require(!client.connected && !client.is_valid_socket(), "close state");
            require(client.connect(), "reconnect to retained hostname failed");
        });
        case_test("shared-context-per-client-verification", [&] {
            live_server server(bad.server_ctx);
            SSL_CTX* shared = SSL_CTX_new(TLS_client_method());
            require(shared != nullptr, "shared context");
            // Do not destroy the context until after all clients referencing it.
            {
                aeon::ssl_client_socket client(shared);
                require(client.set_ca_file(bad.ca_path.string().c_str()) == 0, "shared CA");
                require(client.set_peer_verification(aeon::ssl_verify_mode::strict) == 0, "strict shared");
                require(!client.connect("127.0.0.1", server.port), "shared ctx bypassed IP");
            }
            SSL_CTX_free(shared);
        });
        case_test("server-requires-certificate", [&] {
            aeon::ssl_server_socket server;
            require(!server.listen("127.0.0.1", 0), "TLS listener without identity");
            require(server.get_error() == aeon::ssl_error_cert, "missing identity error");
            require(server.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "server certificate");
            require(!server.listen("127.0.0.1", 0), "TLS listener without key");
            require(server.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "server key");
            require(server.set_handshake_timeout(0) != 0, "unbounded handshake accepted");
            require(server.set_handshake_timeout(1500) == 0, "handshake timeout");
        });
        case_test("native-server-owned-tls-peer-and-broadcast", [&] {
            aeon::ssl_server_socket server;
            require(server.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "server cert");
            require(server.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "server key");
            require(server.listen("127.0.0.1", 0), "TLS server listen");
            require(server.set_accept_timeout(2000) == 0, "TLS accept timeout");
            sockaddr_storage local{};
            socklen_t len = static_cast<socklen_t>(sizeof(local));
            require(::getsockname(server.sockfd, reinterpret_cast<sockaddr*>(&local),
                                  &len) == 0, "TLS getsockname");
            const auto port = ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
            bool passed = false;
            std::jthread worker([&] {
#ifndef NET_PLATFORM_WINDOWS
                sigset_t blocked{};
                sigemptyset(&blocked);
                sigaddset(&blocked, SIGPIPE);
                (void)pthread_sigmask(SIG_BLOCK, &blocked, nullptr);
#endif
                aeon::ssl_socket* peer = server.accept(true);
                if (!peer || peer->parent_server() != &server || server.peer_count() != 1)
                    return;
                char input[8]{};
                if (peer->read(input, sizeof(input)) != 4 ||
                    std::memcmp(input, "PING", 4) != 0) return;
                if (server.peers().broadcast("PONG", 4) != 1) return;
                passed = server.remove_peer(peer) && server.peer_count() == 0;
            });
            aeon::ssl_client_socket client;
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0,
                    "TLS server client CA");
            const bool connected = client.connect("127.0.0.1", port);
            bool roundtrip = false;
            if (connected && client.write("PING", 4) == 4) {
                char response[8]{};
                roundtrip = client.read(response, sizeof(response)) == 4 &&
                            std::memcmp(response, "PONG", 4) == 0;
            }
            worker.join();
            require(connected && roundtrip && passed, "TLS server peer round trip");
        });
#if LIBAEON_HAS_EVENTS
        case_test("event-server-requires-credentials", [&] {
            aeon::ssl_server_event_socket server;
            require(!server.listen("127.0.0.1", 0), "event server without credentials");
            require(server.get_error() == aeon::ssl_error_cert, "missing event certificate");
            require(server.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "event certificate");
            require(!server.listen("127.0.0.1", 0), "event server without key");
            require(server.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "event private key");
        });
        case_test("event-tls-nonblocking-binary-and-server-ownership", [&] {
            aeon::ssl_server_event_socket server;
            require(server.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "event server certificate");
            require(server.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "event server key");
            require(server.listen("127.0.0.1", 0), "event server listen");
            sockaddr_in local{};
            socklen_t len = sizeof(local);
            require(::getsockname(server.sockfd, reinterpret_cast<sockaddr*>(&local),
                                  &len) == 0, "event server port");
            const int port = ntohs(local.sin_port);
            aeon::ssl_event_socket client;
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0,
                    "event client CA");
            aeon::event_socket_set poller;
            aeon::ssl_event_socket* peer = nullptr;
            require(poller.add(&server), "register event listener");
            int listener_hits = 0, client_connects = 0, peer_connects = 0;
            int set_connects = 0, drains = 0, event_errors = 0;
            poller.on_socket_listen([&](aeon::event_socket* ready) {
                require(ready == &server, "event listener identity");
                ++listener_hits;
                peer = server.accept(false);
                require(peer != nullptr && peer->parent_server() == &server,
                        "server-owned TLS event peer");
                peer->on_connect([&] { ++peer_connects; });
                peer->on_error([&](int) { ++event_errors; });
                require(poller.add(peer), "register TLS event peer");
            });
            client.on_connect([&] { ++client_connects; });
            client.on_error([&](int) { ++event_errors; });
            poller.on_socket_connect([&](aeon::event_socket* ready) {
                require(ready == &client || ready == peer, "TLS connect identity");
                ++set_connects;
            });
            poller.on_socket_drain([&](aeon::event_socket*) { ++drains; });
            require(client.connect("127.0.0.1", port), "event TCP connect");
            require(!client.handshake_complete(), "TLS handshake not yet polled");
            require(poller.add(&client), "register TLS event client");
            for (int i = 0; i < 100 && (!peer || !peer->handshake_complete() ||
                                        !client.handshake_complete()); ++i)
                require(poller.poll(100) >= 0, "event handshake poll");
            require(peer && client.handshake_complete() && peer->handshake_complete(),
                    "TLS handshake complete");
            require(listener_hits == 1 && client_connects == 1 &&
                    peer_connects == 1 && set_connects == 2 && !event_errors,
                    "TLS callbacks exactly once");
            require(server.peer_count() == 1, "event server peer ownership");
            std::string payload(4096, 'z');
            payload[213] = '\0'; payload[2048] = '\x01';
            std::string received;
            peer->on_data([&](const char* data, int bytes) { received.append(data, bytes); });
            require(client.queue_write(payload), "queue encrypted event payload");
            for (int i = 0; i < 200 && (received.size() < payload.size() ||
                                         client.queued_bytes()); ++i)
                require(poller.poll(100) >= 0, "event inbound encrypted poll");
            require(received == payload && drains == 1,
                    "queued encrypted binary payload and drain");
            std::string echo;
            client.on_data([&](const char* data, int bytes) { echo.append(data, bytes); });
            require(peer->queue_write(payload), "server queued TLS reply");
            for (int i = 0; i < 200 && (echo.size() < payload.size() ||
                                         peer->queued_bytes()); ++i)
                require(poller.poll(100) >= 0, "event outgoing encrypted poll");
            require(echo == payload && drains == 2 && !event_errors,
                    "encrypted reply through event_socket_set");
            // Calls through the ordinary socket API must still decrypt TLS.
            require(client.write("DIRECT", 6) == 6, "direct TLS event write");
            char directly_read[16]{};
            int directly_received = -1;
            for (int i = 0; i < 50 && directly_received <= 0; ++i) {
                directly_received = static_cast<aeon::socket*>(peer)->read(
                    directly_read, static_cast<int>(sizeof(directly_read)));
                if (directly_received <= 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            require(directly_received == 6 &&
                    std::memcmp(directly_read, "DIRECT", 6) == 0,
                    "virtual socket::read decrypts TLS");
            require(poller.remove_socket(peer) && poller.remove_socket(&client) &&
                    poller.remove_socket(&server), "deregister before peer destruction");
            require(server.remove_peer(peer) && server.peer_count() == 0,
                    "TLS event peer cleanup");
        });
        case_test("event-tls-client-rejects-untrusted-server", [&] {
            aeon::ssl_server_event_socket server;
            require(server.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "event reject server certificate");
            require(server.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "event reject server key");
            require(server.listen("127.0.0.1", 0), "event reject listen");
            sockaddr_in local{};
            socklen_t len = sizeof(local);
            require(::getsockname(server.sockfd, reinterpret_cast<sockaddr*>(&local),
                                  &len) == 0, "event reject getsockname");
            aeon::ssl_event_socket client;
            require(client.set_ca_file(bad.ca_path.string().c_str()) == 0,
                    "event client trusts only unrelated certificate");
            require(client.connect("127.0.0.1", ntohs(local.sin_port)), "reject TCP connect");
            auto* peer = server.accept(false);
            require(peer != nullptr, "event reject accept");
            aeon::event_socket_set poller;
            require(poller.add(&client) && poller.add(peer), "event reject register");
            int data_hits = 0, error_hits = 0;
            client.on_data([&](const char*, int) { ++data_hits; });
            client.on_error([&](int) { ++error_hits; });
            for (int i = 0; i < 100 && client.is_valid_socket(); ++i)
                require(poller.poll(100) >= 0, "event reject poll");
            require(!client.handshake_complete() && !client.is_valid_socket() &&
                    client.get_error() == aeon::ssl_error_cert &&
                    error_hits == 1 && data_hits == 0, "event TLS strict rejection");
            require(poller.remove_socket(peer) && poller.remove_socket(&client),
                    "event reject unregister");
            require(server.remove_peer(peer), "event reject cleanup");
        });
        case_test("event-tls-server-silent-peer-deadline", [&] {
            aeon::ssl_server_event_socket listener;
            require(listener.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "deadline server certificate");
            require(listener.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "deadline server private key");
            require(listener.set_handshake_timeout(0) != 0, "reject disabled deadline");
            require(listener.set_handshake_timeout(150) == 0, "server deadline setting");
            require(listener.listen("127.0.0.1", 0), "deadline listener");
            sockaddr_in local{};
            socklen_t length = sizeof(local);
            require(::getsockname(listener.sockfd, reinterpret_cast<sockaddr*>(&local),
                                  &length) == 0, "deadline getsockname");
            aeon::client_socket raw;
            require(raw.connect("127.0.0.1", ntohs(local.sin_port)),
                    "silent TCP connection");
            auto* peer = listener.accept(false);
            require(peer != nullptr, "accept silent peer");
            aeon::event_socket_set poller;
            require(poller.add(peer), "register silent TLS peer");
            int errors = 0, closes = 0, data = 0;
            peer->on_error([&](int reason) {
                require(reason == aeon::ssl_error_handshake, "deadline error code");
                ++errors;
            });
            peer->on_close([&] { ++closes; });
            peer->on_data([&](const char*, int) { ++data; });
            // The watchdog provides a finite escape if poll(-1) incorrectly
            // ignores the deadline. It touches only the independent raw peer.
            std::jthread watchdog([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                (void)raw.close();
            });
            const auto started = std::chrono::steady_clock::now();
            const int handled = poller.poll(-1);
            const auto elapsed = std::chrono::steady_clock::now() - started;
            watchdog.join();
            require(handled > 0 && elapsed < std::chrono::milliseconds(800),
                    "infinite poll ignored TLS handshake deadline");
            require(!peer->is_valid_socket() && !peer->handshake_complete() &&
                    errors == 1 && closes == 1 && data == 0,
                    "silent TLS peer times out with one error and close");
            require(poller.remove_socket(peer) && listener.remove_peer(peer),
                    "remove timed-out TLS peer");
        });
        case_test("event-tls-client-silent-server-deadline-and-cancel", [&] {
            aeon::server_socket listener;
            require(listener.listen("127.0.0.1", 0), "plain listener for stalled TLS");
            sockaddr_in local{};
            socklen_t length = sizeof(local);
            require(::getsockname(listener.sockfd, reinterpret_cast<sockaddr*>(&local),
                                  &length) == 0, "client deadline getsockname");
            aeon::ssl_event_socket client;
            require(client.set_handshake_timeout(25) != 0,
                    "reject too-short client deadline");
            require(client.set_handshake_timeout(150) == 0,
                    "client handshake deadline");
            require(client.connect("127.0.0.1", ntohs(local.sin_port)),
                    "TLS client TCP connected");
            aeon::socket* silent = listener.accept(true);
            require(silent != nullptr, "accept TLS client on plain listener");
            aeon::event_socket_set poller;
            require(poller.add(&client), "register pending TLS client");
            int errors = 0, closes = 0, data = 0;
            client.on_error([&](int reason) {
                require(reason == aeon::ssl_error_handshake,
                        "client deadline error code");
                ++errors;
            });
            client.on_close([&] { ++closes; });
            client.on_data([&](const char*, int) { ++data; });
            const auto started = std::chrono::steady_clock::now();
            for (int n = 0; n < 6 && client.is_valid_socket(); ++n)
                require(poller.poll(1000) >= 0, "client timeout poll");
            const auto elapsed = std::chrono::steady_clock::now() - started;
            require(elapsed < std::chrono::seconds(2) && !client.is_valid_socket() &&
                    errors == 1 && closes == 1 && data == 0,
                    "silent server TLS handshake timed out cleanly");
            require(poller.remove_socket(&client), "remove timed-out client");
            require(listener.remove_peer(silent), "remove silent peer");
            {
                live_server real(good.server_ctx);
                require(client.set_ca_file(good.ca_path.string().c_str()) == 0,
                        "reconnect CA after timeout");
                require(client.connect("127.0.0.1", real.port),
                        "reconnect TLS socket after deadline");
                require(poller.add(&client), "register reconnected TLS client");
                for (int n = 0; n < 100 && !client.handshake_complete(); ++n)
                    require(poller.poll(100) >= 0, "reconnect TLS poll");
                require(client.handshake_complete(), "reconnect handshake completed");
                require(poller.remove_socket(&client), "remove reconnected TLS client");
                require(client.close(), "close reconnected client");
            }

            // An explicitly cancelled TLS handshake must not leak its SSL
            // session or leave a descriptor attached to the object.
            aeon::ssl_event_socket cancelled;
            require(cancelled.connect("127.0.0.1", ntohs(local.sin_port)),
                    "second pending handshake");
            require(cancelled.close(), "cancel in-progress handshake");
            require(!cancelled.handshake_complete() && !cancelled.is_valid_socket(),
                    "cancel tears down pending TLS session");
            require(!cancelled.close(), "cancel close idempotence");
        });
        case_test("event-tls-reconnect-replaces-pending-session", [&] {
            aeon::server_socket silent_listener;
            require(silent_listener.listen("127.0.0.1", 0),
                    "reconnect silent listener");
            sockaddr_in local{};
            socklen_t length = sizeof(local);
            require(::getsockname(silent_listener.sockfd,
                                  reinterpret_cast<sockaddr*>(&local), &length) == 0,
                    "reconnect silent port");
            aeon::ssl_event_socket client;
            require(client.connect("127.0.0.1", ntohs(local.sin_port)),
                    "initial pending TCP connection");
            auto* silent = silent_listener.accept(true);
            require(silent != nullptr, "first accepted pending peer");
            live_server real(good.server_ctx);
            require(client.set_ca_file(good.ca_path.string().c_str()) == 0,
                    "reconnect trust store");
            require(client.connect("127.0.0.1", real.port),
                    "replace pending TLS connection");
            aeon::event_socket_set poller;
            require(poller.add(&client), "poll reconnected pending TLS");
            for (int n = 0; n < 100 && !client.handshake_complete(); ++n)
                require(poller.poll(100) >= 0, "replacement handshake poll");
            require(client.handshake_complete(), "replacement TLS handshake");
            require(poller.remove_socket(&client), "unregister replacement client");
            require(client.close(), "close replacement TLS connection");
            require(silent_listener.remove_peer(silent), "retire previous TCP peer");
        });
        case_test("event-tls-rejects-plaintext-without-data-callback", [&] {
            aeon::ssl_server_event_socket server;
            require(server.set_certificate_file(good.ca_path.string().c_str()) == 0,
                    "plaintext test certificate");
            require(server.set_private_key_file(good.key_path.string().c_str()) == 0,
                    "plaintext test key");
            require(server.listen("127.0.0.1", 0), "plaintext test listen");
            sockaddr_in local{};
            socklen_t len = sizeof(local);
            require(::getsockname(server.sockfd, reinterpret_cast<sockaddr*>(&local),
                                  &len) == 0, "plaintext event server port");
            aeon::client_socket raw;
            require(raw.connect("127.0.0.1", ntohs(local.sin_port)), "plain TCP connect");
            auto* peer = server.accept(false);
            require(peer != nullptr, "plain connection accepted for TLS handshake");
            aeon::event_socket_set poller;
            require(poller.add(peer), "plain peer register");
            int data = 0, errors = 0;
            peer->on_data([&](const char*, int) { ++data; });
            peer->on_error([&](int) { ++errors; });
            require(raw.write("NOT TLS!", 8) == 8, "send plaintext probe");
            for (int i = 0; i < 100 && peer->is_valid_socket(); ++i)
                require(poller.poll(100) >= 0, "plain rejection poll");
            require(!peer->handshake_complete() && !peer->is_valid_socket() &&
                    data == 0 && errors == 1, "TLS never dispatched plaintext");
            require(poller.remove_socket(peer) && server.remove_peer(peer),
                    "plain rejection cleanup");
        });
#endif
        fs::remove_all(folder);
        std::cout << "M6c/M6f live TLS: all cases passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "M6c TLS test FAILED: " << e.what() << '\n';
        fs::remove_all(folder);
        return 1;
    }
}
