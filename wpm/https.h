/** @file https.h @brief App-local TLS repository downloads. */
#ifndef WPM_HTTPS_H
#define WPM_HTTPS_H

/** Download a verified HTTPS response, replacing the destination only on success. */
int wpm_https_download(const char* url, const char* destination, const char* label);

#endif
