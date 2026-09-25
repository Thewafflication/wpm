/** @file https.c @brief Verified app-local TLS 1.2 and bounded HTTP/1.1 downloads. */
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/x509_crt.h"
#include "https.h"
#include "helpers.h"
#include "progress.h"
#include "wpm_ca_bundle.h"

#define HTTPS_URL_SIZE 4096
#define HTTPS_LINE_SIZE 8192
#define HTTPS_HEADER_LIMIT 65536
#define HTTPS_TIMEOUT_SECONDS 30

typedef struct https_url {
    char host[254];
    char authority[264];
    char path[HTTPS_URL_SIZE];
    unsigned short port;
} https_url;

typedef BOOL (WINAPI *crypto_acquire_fn)(ULONG_PTR*, const char*, const char*, DWORD, DWORD);
typedef BOOL (WINAPI *crypto_random_fn)(ULONG_PTR, DWORD, BYTE*);
typedef BOOL (WINAPI *crypto_release_fn)(ULONG_PTR, DWORD);

typedef struct https_connection {
    SOCKET socket;
    HMODULE crypto;
    ULONG_PTR provider;
    crypto_random_fn random;
    crypto_release_fn release;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    unsigned char buffer[16384];
    size_t used, available;
    DWORD phase_started;
    int handshake;
} https_connection;

typedef struct https_response {
    int status;
    int chunked;
    int has_length;
    unsigned long long length;
    char location[HTTPS_URL_SIZE];
} https_response;

static int https_copy(char* to, size_t size, const char* from, size_t length)
{
    if (length >= size) return 0;
    memcpy(to, from, length);
    to[length] = 0;
    return 1;
}

static int https_number(const char* text, unsigned base, unsigned long long* result)
{
    unsigned long long value = 0;
    if (!*text) return 0;
    for (; *text; text++) {
        unsigned digit;
        if (*text >= '0' && *text <= '9') digit = (unsigned)(*text - '0');
        else if (base == 16 && *text >= 'a' && *text <= 'f') digit = (unsigned)(*text - 'a' + 10);
        else if (base == 16 && *text >= 'A' && *text <= 'F') digit = (unsigned)(*text - 'A' + 10);
        else return 0;
        if (digit >= base || value > (ULLONG_MAX - digit) / base) return 0;
        value = value * base + digit;
    }
    *result = value;
    return 1;
}

static int https_parse_url(const char* text, https_url* url)
{
    const char *authority, *end, *colon, *p, *fragment;
    unsigned long long port = 443;
    if (_strnicmp(text, "https://", 8) != 0 || strlen(text) >= HTTPS_URL_SIZE) return 0;
    for (p = text; *p; p++) {
        if ((unsigned char)*p <= 32 || (unsigned char)*p >= 127 || *p == '\\') return 0;
    }
    authority = text + 8;
    end = authority + strcspn(authority, "/?#");
    if (!https_copy(url->authority, sizeof(url->authority), authority, (size_t)(end - authority))) return 0;
    colon = strchr(url->authority, ':');
    if (colon && (!https_number(colon + 1, 10, &port) || !port || port > 65535)) return 0;
    if (!https_copy(url->host, sizeof(url->host), url->authority,
        colon ? (size_t)(colon - url->authority) : strlen(url->authority)) || !url->host[0]) return 0;
    /* First implementation uses IPv4 DNS. Reject credentials and ambiguous hosts. */
    for (p = url->host; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '-' || *p == '.')) return 0;
    }
    url->port = (unsigned short)port;
    fragment = strchr(end, '#');
    if (!fragment) fragment = end + strlen(end);
    if (*end == '/') return https_copy(url->path, sizeof(url->path), end, (size_t)(fragment - end));
    url->path[0] = '/';
    return https_copy(url->path + 1, sizeof(url->path) - 1, end, (size_t)(fragment - end));
}

static int https_redirect(const https_url* base, const char* location, char* next)
{
    char path[HTTPS_URL_SIZE];
    char* query;
    char* slash;
    int size;
    if (!location[0]) return 0;
    if (_strnicmp(location, "https://", 8) == 0)
        return https_copy(next, HTTPS_URL_SIZE, location, strlen(location));
    if (location[0] == '/' && location[1] == '/')
        size = snprintf(next, HTTPS_URL_SIZE, "https:%s", location);
    else if (location[0] == '/')
        size = snprintf(next, HTTPS_URL_SIZE, "https://%s%s", base->authority, location);
    else {
        /* A colon before the first slash indicates another URI scheme. */
        if (strcspn(location, ":/?#") < strcspn(location, "/?#")) return 0;
        strcpy(path, base->path);
        query = strchr(path, '?');
        if (query) *query = 0;
        if (*location != '?' && *location != '#') {
            slash = strrchr(path, '/');
            if (!slash) return 0;
            slash[1] = 0;
        }
        size = snprintf(next, HTTPS_URL_SIZE, "https://%s%s%s", base->authority, path, location);
    }
    return size > 0 && size < HTTPS_URL_SIZE;
}

static int https_wait(https_connection* connection, int writing)
{
    fd_set sockets;
    struct timeval timeout;
    if (connection->handshake && GetTickCount() - connection->phase_started > 60000UL) return 0;
    FD_ZERO(&sockets);
    FD_SET(connection->socket, &sockets);
    timeout.tv_sec = HTTPS_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    return select(0, writing ? NULL : &sockets, writing ? &sockets : NULL, NULL, &timeout) > 0;
}

static int https_send(void* context, const unsigned char* bytes, size_t size)
{
    https_connection* connection = (https_connection*)context;
    int result;
    if (!https_wait(connection, 1)) return MBEDTLS_ERR_SSL_TIMEOUT;
    result = send(connection->socket, (const char*)bytes, (int)(size > INT_MAX ? INT_MAX : size), 0);
    if (result == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_WRITE;
    return result > 0 ? result : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int https_recv(void* context, unsigned char* bytes, size_t size)
{
    https_connection* connection = (https_connection*)context;
    int result;
    if (!https_wait(connection, 0)) return MBEDTLS_ERR_SSL_TIMEOUT;
    result = recv(connection->socket, (char*)bytes, (int)(size > INT_MAX ? INT_MAX : size), 0);
    if (result == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_READ;
    return result >= 0 ? result : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int https_random(void* context, unsigned char* bytes, size_t size)
{
    https_connection* connection = (https_connection*)context;
    if (size > MAXDWORD || !connection->random(connection->provider, (DWORD)size, bytes))
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return 0;
}

static void https_close(https_connection* connection)
{
    mbedtls_ssl_free(&connection->ssl);
    mbedtls_ssl_config_free(&connection->config);
    if (connection->socket != INVALID_SOCKET) closesocket(connection->socket);
    if (connection->provider && connection->release) connection->release(connection->provider, 0);
    if (connection->crypto) FreeLibrary(connection->crypto);
}

static int https_open(https_connection* connection, const https_url* url, mbedtls_x509_crt* roots)
{
    struct hostent* host;
    HMODULE crypto;
    crypto_acquire_fn acquire;
    int i, result = -1;
    memset(connection, 0, sizeof(*connection));
    connection->socket = INVALID_SOCKET;
    mbedtls_ssl_init(&connection->ssl);
    mbedtls_ssl_config_init(&connection->config);
    crypto = connection->crypto = LoadLibraryA("advapi32.dll");
    if (!crypto) return 0;
    acquire = (crypto_acquire_fn)GetProcAddress(crypto, "CryptAcquireContextA");
    connection->random = (crypto_random_fn)GetProcAddress(crypto, "CryptGenRandom");
    connection->release = (crypto_release_fn)GetProcAddress(crypto, "CryptReleaseContext");
    if (!acquire || !connection->random || !connection->release ||
        !acquire(&connection->provider, NULL, NULL, 1, 0xf0000000UL)) return 0;
    host = gethostbyname(url->host);
    if (!host || host->h_addrtype != AF_INET || host->h_length != 4) return 0;
    for (i = 0; host->h_addr_list[i]; i++) {
        struct sockaddr_in address;
        u_long nonblocking = 1;
        int socket_error = 0, error_size = sizeof(socket_error);
        connection->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connection->socket == INVALID_SOCKET) continue;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(url->port);
        memcpy(&address.sin_addr, host->h_addr_list[i], 4);
        if (ioctlsocket(connection->socket, FIONBIO, &nonblocking) != 0) return 0;
        result = connect(connection->socket, (struct sockaddr*)&address, sizeof(address));
        if (result == 0 || (WSAGetLastError() == WSAEWOULDBLOCK && https_wait(connection, 1) &&
            getsockopt(connection->socket, SOL_SOCKET, SO_ERROR, (char*)&socket_error, &error_size) == 0 && !socket_error)) break;
        closesocket(connection->socket);
        connection->socket = INVALID_SOCKET;
    }
    if (connection->socket == INVALID_SOCKET) return 0;
    result = mbedtls_ssl_config_defaults(&connection->config, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0) return 0;
    mbedtls_ssl_conf_authmode(&connection->config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&connection->config, roots, NULL);
    mbedtls_ssl_conf_rng(&connection->config, https_random, connection);
    if (mbedtls_ssl_setup(&connection->ssl, &connection->config) != 0 ||
        mbedtls_ssl_set_hostname(&connection->ssl, url->host) != 0) return 0;
    mbedtls_ssl_set_bio(&connection->ssl, connection, https_send, https_recv, NULL);
    connection->handshake = 1;
    connection->phase_started = GetTickCount();
    do { result = mbedtls_ssl_handshake(&connection->ssl); }
    while (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);
    connection->handshake = 0;
    if (result != 0 || mbedtls_ssl_get_verify_result(&connection->ssl) != 0) {
        char error[256];
        mbedtls_strerror(result, error, sizeof(error));
        printf("Error: TLS verification/handshake failed for %s: %s (certificate flags 0x%lx).\n",
            url->host, error, (unsigned long)mbedtls_ssl_get_verify_result(&connection->ssl));
        return 0;
    }
    return 1;
}

static int https_write(https_connection* connection, const char* request)
{
    size_t size = strlen(request), offset = 0;
    while (offset < size) {
        int result = mbedtls_ssl_write(&connection->ssl, (const unsigned char*)request + offset, size - offset);
        if (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (result <= 0) return 0;
        offset += result;
    }
    return 1;
}

/* EOF is authenticated close_notify; an unframed abrupt TLS EOF is an error. */
static int https_read(https_connection* connection, unsigned char* bytes, size_t size)
{
    size_t count;
    int result;
    if (!size) return 0;
    if (connection->used == connection->available) {
        do { result = mbedtls_ssl_read(&connection->ssl, connection->buffer, sizeof(connection->buffer)); }
        while (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);
        if (result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
        if (result <= 0) return -1;
        connection->used = 0;
        connection->available = (size_t)result;
    }
    count = connection->available - connection->used;
    if (count > size) count = size;
    memcpy(bytes, connection->buffer + connection->used, count);
    connection->used += count;
    return (int)count;
}

static int https_line(https_connection* connection, char* line, size_t* budget)
{
    size_t size = 0;
    unsigned char c;
    while (*budget && size + 1 < HTTPS_LINE_SIZE) {
        if (https_read(connection, &c, 1) != 1) return 0;
        --*budget;
        if (c == '\r') {
            if (!*budget || https_read(connection, &c, 1) != 1 || c != '\n') return 0;
            --*budget;
            line[size] = 0;
            return 1;
        }
        if ((c < 32 && c != '\t') || c == 127) return 0;
        line[size++] = (char)c;
    }
    return 0;
}

static char* https_field(char* line)
{
    char *colon = strchr(line, ':'), *p, *end;
    if (!colon || colon == line) return NULL;
    for (p = line; p < colon; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || strchr("!#$%&'*+-.^_`|~", *p))) return NULL;
    }
    *colon++ = 0;
    while (*colon == ' ' || *colon == '\t') colon++;
    end = colon + strlen(colon);
    while (end > colon && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
    return colon;
}

static int https_headers(https_connection* connection, https_response* response)
{
    char line[HTTPS_LINE_SIZE];
    size_t budget = HTTPS_HEADER_LIMIT;
    int interim;
    for (interim = 0; interim < 5; interim++) {
        memset(response, 0, sizeof(*response));
        if (!https_line(connection, line, &budget) || strlen(line) < 12 ||
            (strncmp(line, "HTTP/1.1 ", 9) && strncmp(line, "HTTP/1.0 ", 9)) ||
            line[9] < '1' || line[9] > '5' || line[10] < '0' || line[10] > '9' ||
            line[11] < '0' || line[11] > '9' || (line[12] && line[12] != ' ')) return 0;
        response->status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';
        for (;;) {
            char* value;
            if (!https_line(connection, line, &budget)) return 0;
            if (!line[0]) break;
            value = https_field(line);
            if (!value) return 0;
            if (_stricmp(line, "Content-Length") == 0) {
                if (response->has_length || !https_number(value, 10, &response->length)) return 0;
                response->has_length = 1;
            } else if (_stricmp(line, "Transfer-Encoding") == 0) {
                if (response->chunked || _stricmp(value, "chunked")) return 0;
                response->chunked = 1;
            } else if (_stricmp(line, "Content-Encoding") == 0) {
                if (_stricmp(value, "identity")) return 0;
            } else if (_stricmp(line, "Location") == 0) {
                if (response->location[0] || !https_copy(response->location,
                    sizeof(response->location), value, strlen(value))) return 0;
            }
        }
        if (response->chunked && response->has_length) return 0;
        if (response->status >= 200) return 1;
        if (response->status == 101 || response->chunked || response->has_length) return 0;
    }
    return 0;
}

static int https_body(https_connection* connection, const https_response* response,
    HANDLE output, wpm_progress* progress)
{
    unsigned char bytes[16384];
    unsigned long long remaining = response->length;
    char line[HTTPS_LINE_SIZE];
    size_t budget;
    if (response->has_length) wpm_progress_set(progress, 0, response->length);
    for (;;) {
        int result;
        size_t requested = sizeof(bytes);
        if (response->chunked) {
            char* extension;
            budget = HTTPS_LINE_SIZE;
            if (!https_line(connection, line, &budget)) return 0;
            extension = strchr(line, ';');
            if (extension) *extension = 0;
            if (!https_number(line, 16, &remaining)) return 0;
            if (!remaining) {
                budget = HTTPS_HEADER_LIMIT;
                do {
                    if (!https_line(connection, line, &budget)) return 0;
                    if (line[0] && (!https_field(line) || !_stricmp(line, "Content-Length") ||
                        !_stricmp(line, "Transfer-Encoding"))) return 0;
                } while (line[0]);
                return 1;
            }
        }
        do {
            DWORD written;
            if (response->has_length || response->chunked) {
                if (!remaining) return 1;
                if (remaining < requested) requested = (size_t)remaining;
            }
            result = https_read(connection, bytes, requested);
            if (result < 0) return 0;
            if (!result) return !response->has_length && !response->chunked;
            if (progress->current > ULLONG_MAX - (unsigned)result ||
                !WriteFile(output, bytes, (DWORD)result, &written, NULL) || written != (DWORD)result) return 0;
            wpm_progress_add(progress, (unsigned)result);
            if (response->has_length || response->chunked) remaining -= (unsigned)result;
        } while (!response->chunked || remaining);
        budget = 2;
        if (!https_line(connection, line, &budget) || line[0]) return 0;
    }
}

int wpm_https_download(const char* initial, const char* destination, const char* label)
{
    WSADATA data;
    mbedtls_x509_crt roots;
    https_connection* connection = NULL;
    char current[HTTPS_URL_SIZE], temporary[HTTPS_URL_SIZE], ca_path[HTTPS_URL_SIZE];
    unsigned char* ca_bytes = NULL;
    HANDLE output = INVALID_HANDLE_VALUE;
    wpm_progress progress;
    int redirect, success = 0, started = 0, opened = 0, created = 0;
    DWORD ca_length;
    int ca_result;
    int length = snprintf(temporary, sizeof(temporary), "%s.%08lx.%08lx.download", destination,
        (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount());
    if (length < 0 || length >= (int)sizeof(temporary) ||
        !https_copy(current, sizeof(current), initial, strlen(initial))) return 0;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    mbedtls_x509_crt_init(&roots);
    /* An explicit CA file replaces bundled trust, useful for private repositories. */
    ca_length = GetEnvironmentVariableA("WPM_TLS_CA_FILE", ca_path, sizeof(ca_path));
    if (ca_length) {
        FILE* file;
        long size;
        if (ca_length >= sizeof(ca_path)) goto cleanup;
        file = wpm_fopen(ca_path, "rb");
        if (!file) goto cleanup;
        if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 || size > 4 * 1024 * 1024 ||
            fseek(file, 0, SEEK_SET) != 0) { fclose(file); goto cleanup; }
        ca_bytes = (unsigned char*)malloc((size_t)size + 1);
        if (!ca_bytes || fread(ca_bytes, 1, (size_t)size, file) != (size_t)size) { fclose(file); goto cleanup; }
        fclose(file);
        ca_bytes[size] = 0;
        ca_result = mbedtls_x509_crt_parse(&roots, ca_bytes, (size_t)size + 1);
    } else ca_result = mbedtls_x509_crt_parse(&roots, (const unsigned char*)wpm_ca_pem, sizeof(wpm_ca_pem));
    if (ca_result != 0) {
        printf("Error: could not load complete TLS CA bundle (Mbed TLS result %d).\n", ca_result);
        goto cleanup;
    }
    connection = (https_connection*)calloc(1, sizeof(*connection));
    if (!connection) goto cleanup;
    wpm_progress_start(&progress, "Downloading", "Download", "Downloaded", label, 0);
    started = 1;
    for (redirect = 0; redirect <= 10; redirect++) {
        https_url url;
        https_response response;
        char request[HTTPS_URL_SIZE + 512];
        if (!https_parse_url(current, &url)) goto cleanup;
        opened = 1;
        if (!https_open(connection, &url, &roots)) goto cleanup;
        length = snprintf(request, sizeof(request),
            "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: WPM\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
            url.path, url.authority);
        if (length < 0 || length >= (int)sizeof(request) || !https_write(connection, request) ||
            !https_headers(connection, &response)) goto cleanup;
        if (response.status == 301 || response.status == 302 || response.status == 303 ||
            response.status == 307 || response.status == 308) {
            if (!https_redirect(&url, response.location, current)) goto cleanup;
            https_close(connection);
            opened = 0;
            continue;
        }
        if (response.status != 200) {
            printf("Error: HTTPS server returned status %d.\n", response.status);
            goto cleanup;
        }
        output = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (output == INVALID_HANDLE_VALUE) goto cleanup;
        created = 1;
        if (!https_body(connection, &response, output, &progress)) goto cleanup;
        if (!FlushFileBuffers(output)) goto cleanup;
        length = CloseHandle(output);
        output = INVALID_HANDLE_VALUE;
        if (!length || !MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING)) goto cleanup;
        success = 1;
        break;
    }
cleanup:
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    if (opened) https_close(connection);
    free(connection);
    free(ca_bytes);
    mbedtls_x509_crt_free(&roots);
    WSACleanup();
    if (started) wpm_progress_finish(&progress, success);
    if (!success) {
        if (created) DeleteFileA(temporary);
        printf("Error: bundled HTTPS download failed: %s\n", initial);
    }
    return success;
}
