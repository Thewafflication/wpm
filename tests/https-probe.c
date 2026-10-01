/* Test the production transport and its URL parsing without package side
 * effects. */
#include "../wpm/https.c"

/* The transport probe has no package database/logger lifecycle. Production
   uses logging.c; preserve diagnostics on stdout in this isolated harness. */
int wpm_printf(const char *format, ...) {
    char message[8192];
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (result >= 0) {
        fputs(message, stdout);
        fflush(stdout);
    }
    return result;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        static const char *invalid[] = {"http://localhost/",
                                        "https://user@localhost/",
                                        "https://localhost:0/",
                                        "https://localhost:65536/",
                                        "https://localhost/a\r\nb",
                                        "https://localhost\\evil/",
                                        "https:///",
                                        "https://localhost:123bad/"};
        https_url url;
        char next[HTTPS_URL_SIZE];
        unsigned i;
        unsigned long long value;
        if (https_select_backend(NULL, NULL, 5, 1) != WPM_HTTPS_BUNDLED ||
            https_select_backend("auto", NULL, 5, 0) != WPM_HTTPS_BUNDLED ||
            https_select_backend(NULL, NULL, 6, 1) != WPM_HTTPS_BUNDLED ||
            https_select_backend("", NULL, 6, 2) != WPM_HTTPS_URLMON ||
            https_select_backend("auto", NULL, 10, 0) != WPM_HTTPS_URLMON ||
            https_select_backend(NULL, "private.pem", 10, 0) !=
                WPM_HTTPS_BUNDLED ||
            https_select_backend("bundled", NULL, 10, 0) != WPM_HTTPS_BUNDLED ||
            https_select_backend("urlmon", NULL, 5, 1) != WPM_HTTPS_URLMON ||
            https_select_backend("urlmon", "private.pem", 10, 0) !=
                WPM_HTTPS_INVALID ||
            https_select_backend("typo", NULL, 10, 0) != WPM_HTTPS_INVALID) {
            return 1;
        }
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
            if (https_parse_url(invalid[i], &url)) {
                return 1;
            }
        }
        if (!https_parse_url("https://localhost:8443/a/b?q=1#fragment", &url) ||
            strcmp(url.host, "localhost") || strcmp(url.path, "/a/b?q=1") ||
            url.port != 8443 || !https_redirect(&url, "../next", next) ||
            strcmp(next, "https://localhost:8443/a/../next") ||
            https_redirect(&url, "http://localhost/", next) ||
            https_number("18446744073709551616", 10, &value) ||
            !https_number("18446744073709551615", 10, &value) ||
            value != ULLONG_MAX) {
            return 1;
        }
        puts("HTTPS URL and length parsing passed");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--progress-test") == 0) {
        wpm_progress progress;
        wpm_progress_start(&progress, "Downloading", "Download", "Downloaded",
                           "short index", 17);
        wpm_progress_set(&progress, 17, 17);
        wpm_progress_finish(&progress, 1);
        wpm_progress_start(&progress, "Downloading", "Download", "Downloaded",
                           "large unknown", 0);
        wpm_progress_add(&progress, 4294967297ULL);
        wpm_progress_finish(&progress, 1);
        wpm_progress_start(&progress, "Downloading", "Download", "Downloaded",
                           "partial", 0);
        wpm_progress_add(&progress, 23);
        wpm_progress_finish(&progress, 0);
        /* Exercise the compact interactive completion path with a long label
           even when the test runner captures stdout through a pipe. */
        {
            char label[512];
            memset(label, 'x', sizeof(label) - 1);
            label[sizeof(label) - 1] = 0;
            wpm_progress_start(&progress, "Downloading", "Download",
                               "Downloaded", label, 0);
            progress.interactive = 1;
            wpm_progress_add(&progress, ULLONG_MAX);
            wpm_progress_finish(&progress, 1);
        }
        return 0;
    }
    if (argc != 3) {
        return 2;
    }
    return wpm_https_download(argv[1], argv[2], "TLS test") ? 0 : 1;
}
