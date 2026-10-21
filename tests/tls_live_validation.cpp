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

    explicit ephemeral_certificate(const std::filesystem::path& path, const char* san,
                                   bool expired = false)
        : ca_path(path) {
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
        fs::remove_all(folder);
        std::cout << "M6c live TLS: all cases passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "M6c TLS test FAILED: " << e.what() << '\n';
        fs::remove_all(folder);
        return 1;
    }
}
