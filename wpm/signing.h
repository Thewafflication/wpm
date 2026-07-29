/** @file signing.h @brief Public package signing interface. */
#ifndef WPM_SIGNING_H
#define WPM_SIGNING_H

#include <stddef.h>

/** Generate an Ed25519 key pair and optionally select it as the default. */
int wpm_keygen(const char* private_path, const char* public_path, int make_default);
/** Validate and select a private signing key as the default. */
int wpm_set_default_key(const char* private_path);
/** Clear the configured default signing key. */
int wpm_clear_default_key(void);
/** Resolve and validate the configured default signing key. */
int wpm_get_default_key(char* result, size_t size);
/** Add a public key to the machine trust store. */
int wpm_trust_add(const char* public_path);
/** List active and revoked keys in the trust store. */
int wpm_trust_list(void);
/** Revoke a trusted key by its hexadecimal identifier. */
int wpm_trust_revoke(const char* key_id);
/** Sign the package index beneath a package source directory. */
int wpm_sign_package_index(const char* source_dir, const char* private_path);
/** Validate a staged package signature and return its signing-key ID. */
int wpm_validate_package_signature(const char* staging_dir, int allow_unsigned, char* key_id, size_t key_id_size);

#endif
