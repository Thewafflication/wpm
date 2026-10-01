/** @file https.h @brief App-local TLS repository downloads. */
#ifndef WPM_HTTPS_H
#define WPM_HTTPS_H

typedef enum wpm_https_backend {
    WPM_HTTPS_INVALID,
    WPM_HTTPS_BUNDLED,
    WPM_HTTPS_URLMON
} wpm_https_backend;

/** Select transport before connecting, honoring explicit backend and CA
 * settings. */
wpm_https_backend wpm_https_get_backend(void);

/** Download a verified HTTPS response, replacing the destination only on
 * success. */
int wpm_https_download(const char *url, const char *destination,
                       const char *label);

#endif
