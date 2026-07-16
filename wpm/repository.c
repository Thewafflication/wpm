#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>
#include <urlmon.h>
#include "archive.h"
#include "repository.h"

#define PATH_SIZE 4096
#define MAX_REPOSITORIES 32
#define MAX_PACKAGES 64
#define INDEX_FRESH_MS (60ULL * 60ULL * 1000ULL)
#define WPM_DEFAULT_REPOSITORY "https://github.com/Thewafflication/wpm/releases/latest/download"

typedef struct { char url[PATH_SIZE]; int priority; int order; } repository;
typedef struct { char name[128]; char version[64]; char arch[16]; char url[PATH_SIZE]; int priority; int order; } package_entry;

static int join_path(char* result, size_t size, const char* left, const char* right) {
    int n = snprintf(result, size, "%s%s%s", left, left[strlen(left) - 1] == '\\' ? "" : "\\", right);
    return n >= 0 && (size_t)n < size;
}
static int ensure_directory(const char* path) {
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) ||
        CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}
static int ensure_cache_directory(const char* leaf) {
    char root[PATH_SIZE], cache[PATH_SIZE], path[PATH_SIZE];
    return wpm_get_data_root(root, sizeof(root)) && join_path(cache, sizeof(cache), root, "cache") &&
        join_path(path, sizeof(path), cache, leaf) && ensure_directory(path);
}
static int config_path(char* path, size_t size) { char root[PATH_SIZE]; return wpm_get_data_root(root, sizeof(root)) && join_path(path, size, root, "config\\repositories.txt"); }
static int cache_path(const char* url, char* path, size_t size) {
    unsigned long hash = 2166136261u; char root[PATH_SIZE]; const unsigned char* p = (const unsigned char*)url;
    while (*p) { hash ^= *p++; hash *= 16777619u; }
    return wpm_get_data_root(root, sizeof(root)) && snprintf(path, size, "%s\\cache\\repositories\\%08lx.json", root, hash) > 0;
}
static int url_normalize(const char* source, char* result, size_t size) {
    size_t length;
    if (!source || _strnicmp(source, "https://", 8) != 0 || strpbrk(source, " \t\r\n")) { printf("Error: repository URLs must use https://.\n"); return 0; }
    if (strcpy_s(result, size, source) != 0) return 0;
    length = strlen(result); while (length > 8 && result[length - 1] == '/') result[--length] = '\0';
    return 1;
}
static int parse_entry(char* line, int* priority, char** url) {
    char* tab = strchr(line, '\t'); char* end;
    if (!tab) return 0; *tab = '\0'; *priority = (int)strtol(line, &end, 10); if (*end) return 0;
    *url = tab + 1; (*url)[strcspn(*url, "\r\n")] = '\0'; return **url != '\0';
}
static int load_repositories(repository* result, int* count) {
    char path[PATH_SIZE], line[PATH_SIZE + 32]; FILE* input; *count = 0;
    if (!config_path(path, sizeof(path))) return 0; input = fopen(path, "r");
    if (input) {
        while (*count < MAX_REPOSITORIES && fgets(line, sizeof(line), input)) { char* url; int priority;
            if (parse_entry(line, &priority, &url)) { repository* r = &result[*count]; r->priority = priority; r->order = *count; strcpy_s(r->url, sizeof(r->url), url); (*count)++; }
        }
        fclose(input);
    }
    for (int i = 0; i < *count; i++) if (_stricmp(result[i].url, WPM_DEFAULT_REPOSITORY) == 0) return 1;
    if (*count < MAX_REPOSITORIES) { repository* r = &result[*count]; r->priority = 0; r->order = *count; strcpy_s(r->url, sizeof(r->url), WPM_DEFAULT_REPOSITORY); (*count)++; }
    return 1;
}
static int rewrite(const char* wanted, int priority, int remove) {
    char path[PATH_SIZE], temporary[PATH_SIZE], line[PATH_SIZE + 32]; FILE *input, *output; int found = 0;
    if (!config_path(path, sizeof(path)) || snprintf(temporary, sizeof(temporary), "%s.tmp", path) < 0) return 0;
    input = fopen(path, "r"); output = fopen(temporary, "w");
    if (!output) { if (input) fclose(input); printf("Error: could not write repository configuration.\n"); return 0; }
    if (input) { while (fgets(line, sizeof(line), input)) { char original[PATH_SIZE + 32], *url; int old_priority; strcpy_s(original, sizeof(original), line);
        if (parse_entry(line, &old_priority, &url) && _stricmp(url, wanted) == 0) { found = 1; if (!remove) fprintf(output, "%d\t%s\n", priority, wanted); } else fputs(original, output); } fclose(input); }
    if (!remove && !found) fprintf(output, "%d\t%s\n", priority, wanted);
    if (fclose(output) || !MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING)) { DeleteFileA(temporary); printf("Error: could not save repository configuration.\n"); return 0; }
    if (remove && !found) { printf("Error: repository is not configured: %s\n", wanted); return 0; }
    printf("Repository %s: %s\n", remove ? "removed" : (found ? "updated" : "added"), wanted); return 1;
}
int wpm_repo_add(const char* url, int priority) { char normalized[PATH_SIZE]; return url_normalize(url, normalized, sizeof(normalized)) && rewrite(normalized, priority, 0); }
int wpm_repo_remove(const char* url) { char normalized[PATH_SIZE], cached[PATH_SIZE]; if (!url_normalize(url, normalized, sizeof(normalized)) || !rewrite(normalized, 0, 1)) return 0; if (cache_path(normalized, cached, sizeof(cached))) DeleteFileA(cached); return 1; }
int wpm_repo_list(void) { repository repositories[MAX_REPOSITORIES]; int count, i; if (!load_repositories(repositories, &count)) return 0; if (!count) { printf("No repositories configured.\n"); return 1; } for (i = 0; i < count; i++) printf("%d\t%s\n", repositories[i].priority, repositories[i].url); return 1; }

static int download(const char* url, const char* destination) {
    HRESULT result; char temporary[PATH_SIZE];
    if (_strnicmp(url, "https://", 8) != 0) return 0;
    if (snprintf(temporary, sizeof(temporary), "%s.download", destination) < 0) return 0;
    DeleteFileA(temporary); result = URLDownloadToFileA(NULL, url, temporary, 0, NULL);
    if (FAILED(result) || !MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING)) { DeleteFileA(temporary); return 0; }
    return 1;
}
static int index_is_fresh(const char* path) { WIN32_FILE_ATTRIBUTE_DATA data; FILETIME now; ULARGE_INTEGER a, b; GetSystemTimeAsFileTime(&now); if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return 0; a.LowPart = now.dwLowDateTime; a.HighPart = now.dwHighDateTime; b.LowPart = data.ftLastWriteTime.dwLowDateTime; b.HighPart = data.ftLastWriteTime.dwHighDateTime; return a.QuadPart >= b.QuadPart && (a.QuadPart - b.QuadPart) <= INDEX_FRESH_MS * 10000ULL; }
static int index_url(const char* root, char* result, size_t size) { return snprintf(result, size, "%s/index.json", root) > 0; }
static int refresh(repository* repo, int offline, int required) {
    char cached[PATH_SIZE], url[PATH_SIZE]; if (!cache_path(repo->url, cached, sizeof(cached))) return 0;
    if (offline) { if (GetFileAttributesA(cached) == INVALID_FILE_ATTRIBUTES) printf("Error: no cached index for %s while offline.\n", repo->url); return GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES; }
    if (!required && index_is_fresh(cached)) return 1;
    if (ensure_cache_directory("repositories") && index_url(repo->url, url, sizeof(url)) && download(url, cached)) { printf("Updated repository index: %s\n", repo->url); return 1; }
    if (GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES) { printf("Warning: could not refresh %s; using cached index.\n", repo->url); return 1; }
    printf("Warning: could not retrieve repository index: %s\n", repo->url); return 0;
}
int wpm_repo_update(int offline) { repository repositories[MAX_REPOSITORIES]; int count, i, refreshed = 0; if (!load_repositories(repositories, &count)) return 0; if (!count) { printf("No repositories configured.\n"); return 1; } for (i = 0; i < count; i++) if (refresh(&repositories[i], offline, 1)) refreshed = 1; return refreshed; }

static const char* skip_ws(const char* p) { while (*p && isspace((unsigned char)*p)) p++; return p; }
static int json_string(const char** source, char* output, size_t size) { const char* p = skip_ws(*source); size_t n = 0; if (*p++ != '"') return 0; while (*p && *p != '"') { if (*p == '\\') { p++; if (!*p) return 0; } if (n + 1 >= size) return 0; output[n++] = *p++; } if (*p != '"') return 0; output[n] = '\0'; *source = p + 1; return 1; }
static int json_value_end(const char** source) { const char* p = skip_ws(*source); int depth = 0, quote = 0; while (*p) { if (quote) { if (*p == '\\' && p[1]) p += 2; else if (*p++ == '"') quote = 0; else p++; continue; } if (*p == '"') quote = 1; else if (*p == '{' || *p == '[') depth++; else if (*p == '}' || *p == ']') { if (!depth--) break; } else if (*p == ',' && depth == 0) break; p++; } *source = p; return 1; }
static int parse_index(const char* path, repository* repo, package_entry* packages, int* count) {
    FILE* f = fopen(path, "rb"); long size; char* text; const char* p; int version_ok = 0;
    if (!f) return 0; fseek(f, 0, SEEK_END); size = ftell(f); rewind(f); if (size < 2 || size > 4 * 1024 * 1024) { fclose(f); return 0; } text = malloc((size_t)size + 1); if (!text) { fclose(f); return 0; } if (fread(text, 1, (size_t)size, f) != (size_t)size) { free(text); fclose(f); return 0; } fclose(f); text[size] = '\0';
    p = text; while ((p = strchr(p, '"')) != NULL) { char key[32]; const char* value; if (!json_string(&p, key, sizeof(key))) break; p = skip_ws(p); if (*p++ != ':') continue; value = skip_ws(p); if (strcmp(key, "version") == 0 && *value == '1') version_ok = 1; if (strcmp(key, "packages") == 0 && *value == '[') { p = value + 1; break; } json_value_end(&p); }
    if (!version_ok) { printf("Warning: repository index has unsupported schema: %s\n", repo->url); free(text); return 0; }
    while (*p && *p != ']') { char name[128] = "", version[64] = "", arch[16] = "", url[PATH_SIZE] = ""; p = skip_ws(p); if (*p++ != '{') { p++; continue; } while (*p && *p != '}') { char key[32]; p = skip_ws(p); if (!json_string(&p, key, sizeof(key))) break; p = skip_ws(p); if (*p++ != ':') break; if (strcmp(key,"name") == 0) json_string(&p,name,sizeof(name)); else if (strcmp(key,"version") == 0) json_string(&p,version,sizeof(version)); else if (strcmp(key,"arch") == 0) json_string(&p,arch,sizeof(arch)); else if (strcmp(key,"url") == 0) json_string(&p,url,sizeof(url)); else json_value_end(&p); p = skip_ws(p); if (*p == ',') p++; } if (*p == '}') p++; if (name[0] && version[0] && arch[0] && url[0] && *count < MAX_PACKAGES) { package_entry* e = &packages[(*count)++]; strcpy_s(e->name,sizeof(e->name),name); strcpy_s(e->version,sizeof(e->version),version); strcpy_s(e->arch,sizeof(e->arch),arch); strcpy_s(e->url,sizeof(e->url),url); e->priority=repo->priority; e->order=repo->order; } p = skip_ws(p); if (*p == ',') p++; }
    free(text); return 1;
}
static int semver_compare(const char* left, const char* right) { unsigned long a[3] = {0}, b[3] = {0}; const char *p = left, *q = right; int i; for (i=0;i<3;i++) { char* e; a[i]=strtoul(p,&e,10); if(e==p) return -1; p=(*e=='.')?e+1:e; b[i]=strtoul(q,&e,10); if(e==q) return 1; q=(*e=='.')?e+1:e; } for(i=0;i<3;i++) if(a[i]!=b[i]) return a[i]>b[i]?1:-1; if (*p == '-' && *q != '-') return -1; if (*p != '-' && *q == '-') return 1; return _stricmp(p,q); }
static int package_url(const char* root, const char* item, char* result, size_t size) { if (_strnicmp(item,"https://",8)==0) return strcpy_s(result,size,item)==0; if (strstr(item,"://") || item[0]=='/' || strstr(item,"..")) return 0; return snprintf(result,size,"%s/%s",root,item)>0; }
int wpm_repo_install(const char* package_name, int offline) {
    repository repositories[MAX_REPOSITORIES]; package_entry entries[MAX_PACKAGES], *selected = NULL; int repositories_count, entries_count = 0, i; char cached[PATH_SIZE], url[PATH_SIZE], root[PATH_SIZE], package_cache[PATH_SIZE];
    if (!load_repositories(repositories, &repositories_count)) return 0;
    for (i = 0; i < repositories_count; i++) { if (refresh(&repositories[i], offline, 0) && cache_path(repositories[i].url, cached, sizeof(cached))) parse_index(cached, &repositories[i], entries, &entries_count); }
    for (i = 0; i < entries_count; i++) if (_stricmp(entries[i].name, package_name) == 0 && (_stricmp(entries[i].arch, "any") == 0 || _stricmp(entries[i].arch, WPM_TARGET_ARCH) == 0) && (!selected || semver_compare(entries[i].version, selected->version) > 0 || (semver_compare(entries[i].version, selected->version) == 0 && (entries[i].priority > selected->priority || (entries[i].priority == selected->priority && entries[i].order < selected->order))))) selected = &entries[i];
    if (!selected) { printf("Error: package was not found in configured repositories: %s\n", package_name); return 0; }
    if (strpbrk(selected->name, "\\/:*") || strpbrk(selected->version, "\\/:*" ) || strpbrk(selected->arch, "\\/:*" ) || !package_url(repositories[selected->order].url, selected->url, url, sizeof(url)) || !wpm_get_data_root(root,sizeof(root)) || snprintf(package_cache,sizeof(package_cache),"%s\\cache\\packages\\%s-%s-%s.zip",root,selected->name,selected->version,selected->arch) < 0) { printf("Error: invalid package URL in repository index.\n"); return 0; }
    if (GetFileAttributesA(package_cache) == INVALID_FILE_ATTRIBUTES) { if (offline) { printf("Error: package is not cached while offline: %s\n", package_name); return 0; } if (!ensure_cache_directory("packages") || !download(url,package_cache)) { printf("Error: could not download package: %s\n", url); return 0; } }
    printf("Installing %s %s from %s\n", selected->name, selected->version, repositories[selected->order].url); return wpm_archive_install(package_cache, 0);
}
