/** @file signing.h @brief Public package signing interface. */
#ifndef WPM_SIGNING_H
#define WPM_SIGNING_H

#include <stddef.h>

/** Generate an Ed25519 key pair and optionally select it as the default. */
int wpm_keygen(const char *private_path, const char *public_path,
               int make_default);
/** Validate and select a private signing key as the default. */
int wpm_set_default_key(const char *private_path);
/** Clear the configured default signing key. */
int wpm_clear_default_key(void);
/** Resolve and validate the configured default signing key. */
int wpm_get_default_key(char *result, size_t size);
/** Add a public key to the machine trust store. */
int wpm_trust_add(const char *public_path);
/** List active and revoked keys in the trust store. */
int wpm_trust_list(void);
/** Revoke a trusted key by its hexadecimal identifier. */
int wpm_trust_revoke(const char *key_id);
/** Sign the package index beneath a package source directory. */
int wpm_sign_package_index(const char *source_dir, const char *private_path);
/** Validate a staged package signature and return its signing-key ID. */
int wpm_validate_package_signature(const char *staging_dir, int allow_unsigned,
                                   char *key_id, size_t key_id_size);

/** Validate in-memory signed index bytes using existing trust; never add keys.
 */
int wpm_validate_signature_bytes(const char *signature,
                                 const unsigned char *index, size_t length,
                                 int allow_unsigned, char *key_id, size_t size);

/** Inspect an explicitly selected public index key without trust-store writes.
 */
int wpm_index_key_id(const char *public_path, char *id, size_t size);
/** Sign exact catalog bytes with an explicit matching private/public pair.
 * Produces public-only JSON; clears private buffers and writes no files. */
int wpm_sign_repository_bytes(const unsigned char *data, size_t length,
                              const char *private_path, const char *public_path,
                              char *signature, size_t capacity, char *id,
                              size_t id_size);
/** Verify exact catalog bytes against the explicitly pinned index public key.
 * Package-signing trust does not authorize this separate role. */
int wpm_verify_repository_bytes(const unsigned char *data, size_t length,
                                const char *public_path, const char *id,
                                const char *public_text,
                                const char *signature_text);

#endif
