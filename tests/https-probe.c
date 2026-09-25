/* Test the production transport and its URL parsing without package side effects. */
#include "../wpm/https.c"

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        static const char* invalid[] = { "http://localhost/", "https://user@localhost/",
            "https://localhost:0/", "https://localhost:65536/", "https://localhost/a\r\nb",
            "https://localhost\\evil/", "https:///", "https://localhost:123bad/" };
        https_url url;
        char next[HTTPS_URL_SIZE];
        unsigned i;
        unsigned long long value;
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
            if (https_parse_url(invalid[i], &url)) return 1;
        if (!https_parse_url("https://localhost:8443/a/b?q=1#fragment", &url) ||
            strcmp(url.host, "localhost") || strcmp(url.path, "/a/b?q=1") || url.port != 8443 ||
            !https_redirect(&url, "../next", next) || strcmp(next, "https://localhost:8443/a/../next") ||
            https_redirect(&url, "http://localhost/", next) ||
            https_number("18446744073709551616", 10, &value) ||
            !https_number("18446744073709551615", 10, &value) || value != ULLONG_MAX) return 1;
        puts("HTTPS URL and length parsing passed");
        return 0;
    }
    if (argc != 3) return 2;
    return wpm_https_download(argv[1], argv[2], "TLS test") ? 0 : 1;
}
