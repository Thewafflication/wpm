#ifndef WPM_SIGNING_H
#define WPM_SIGNING_H

#include <stddef.h>

int wpm_keygen(const char* private_path, const char* public_path, int make_default);
int wpm_set_default_key(const char* private_path);
int wpm_clear_default_key(void);
int wpm_get_default_key(char* result, size_t size);
int wpm_trust_add(const char* public_path);
int wpm_trust_list(void);
int wpm_trust_revoke(const char* key_id);
int wpm_sign_package_index(const char* source_dir, const char* private_path);
int wpm_validate_package_signature(const char* staging_dir, int allow_unsigned, char* key_id, size_t key_id_size);

#endif
