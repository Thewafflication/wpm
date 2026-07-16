#ifndef WPM_REPOSITORY_H
#define WPM_REPOSITORY_H

int wpm_repo_add(const char* url, int priority);
int wpm_repo_list(void);
int wpm_repo_remove(const char* url);
int wpm_repo_update(int offline);
int wpm_repo_install(const char* package_name, int offline);

#endif
