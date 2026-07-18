#ifndef WPM_REPOSITORY_H
#define WPM_REPOSITORY_H

int wpm_repo_add(const char* url, int priority);
int wpm_repo_list(void);
int wpm_repo_remove(const char* url);
int wpm_repo_update(int offline);
int wpm_repo_install(const char* package_name, const char* arch,
    const char* version, int offline, int allow_unsigned);
int wpm_repo_upgrade(const char** package_names, int package_count, int all,
    const char* arch, const char* version, int offline, int allow_unsigned,
    int assume_yes);
int wpm_config_prerelease_set(const char* package_name, int enabled);
int wpm_config_prerelease_get(const char* package_name);
int wpm_config_prerelease_unset(const char* package_name);

#endif
