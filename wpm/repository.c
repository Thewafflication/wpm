#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <windows.h>
#include <urlmon.h>
#include "archive.h"
#include "helpers.h"
#include "repository.h"

#define PATH_SIZE 4096
#define MAX_REPOSITORIES 32
#define MAX_PACKAGES 96
#define MAX_INSTALLED 256
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
    if (!config_path(path, sizeof(path))) return 0; input = wpm_fopen(path, "r");
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
    input = wpm_fopen(path, "r"); output = wpm_fopen(temporary, "w");
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
static int report_available_updates(void);
int wpm_repo_update(int offline) { repository repositories[MAX_REPOSITORIES]; int count, i, refreshed = 0; if (!load_repositories(repositories, &count)) return 0; if (!count) { printf("No repositories configured.\n"); return 1; } for (i = 0; i < count; i++) if (refresh(&repositories[i], offline, 1)) refreshed = 1; if (!refreshed) return 0; return report_available_updates(); }

static const char* skip_ws(const char* p) { while (*p && isspace((unsigned char)*p)) p++; return p; }
static int json_string(const char** source, char* output, size_t size) { const char* p = skip_ws(*source); size_t n = 0; if (*p++ != '"') return 0; while (*p && *p != '"') { if (*p == '\\') { p++; if (!*p) return 0; } if (n + 1 >= size) return 0; output[n++] = *p++; } if (*p != '"') return 0; output[n] = '\0'; *source = p + 1; return 1; }
static int json_value_end(const char** source) { const char* p = skip_ws(*source); int depth = 0, quote = 0; while (*p) { if (quote) { if (*p == '\\' && p[1]) p += 2; else if (*p++ == '"') quote = 0; else p++; continue; } if (*p == '"') quote = 1; else if (*p == '{' || *p == '[') depth++; else if (*p == '}' || *p == ']') { if (!depth--) break; } else if (*p == ',' && depth == 0) break; p++; } *source = p; return 1; }
static int parse_index(const char* path, repository* repo, package_entry* packages, int* count) {
    FILE* f = wpm_fopen(path, "rb"); long size; char* text; const char* p; int version_ok = 0;
    if (!f) return 0; fseek(f, 0, SEEK_END); size = ftell(f); rewind(f); if (size < 2 || size > 4 * 1024 * 1024) { fclose(f); return 0; } text = malloc((size_t)size + 1); if (!text) { fclose(f); return 0; } if (fread(text, 1, (size_t)size, f) != (size_t)size) { free(text); fclose(f); return 0; } fclose(f); text[size] = '\0';
    version_ok = strstr(text, "\"version\":1") != NULL || strstr(text, "\"version\": 1") != NULL;
    p = text; while ((p = strchr(p, '"')) != NULL) { char key[32]; const char* value; if (!json_string(&p, key, sizeof(key))) break; p = skip_ws(p); if (*p++ != ':') continue; value = skip_ws(p); if (strcmp(key, "packages") == 0 && *value == '[') { p = value + 1; break; } json_value_end(&p); }
    if (!version_ok) { printf("Warning: repository index has unsupported schema: %s\n", repo->url); free(text); return 0; }
    while (*p && *p != ']') { char name[128] = "", version[64] = "", arch[16] = "", url[PATH_SIZE] = ""; p = skip_ws(p); if (*p++ != '{') { p++; continue; } while (*p && *p != '}') { char key[32]; p = skip_ws(p); if (!json_string(&p, key, sizeof(key))) break; p = skip_ws(p); if (*p++ != ':') break; if (strcmp(key,"name") == 0) json_string(&p,name,sizeof(name)); else if (strcmp(key,"version") == 0) json_string(&p,version,sizeof(version)); else if (strcmp(key,"arch") == 0) json_string(&p,arch,sizeof(arch)); else if (strcmp(key,"url") == 0) json_string(&p,url,sizeof(url)); else json_value_end(&p); p = skip_ws(p); if (*p == ',') p++; } if (*p == '}') p++; if (name[0] && version[0] && arch[0] && url[0] && *count < MAX_PACKAGES) { package_entry* e = &packages[(*count)++]; strcpy_s(e->name,sizeof(e->name),name); strcpy_s(e->version,sizeof(e->version),version); strcpy_s(e->arch,sizeof(e->arch),arch); strcpy_s(e->url,sizeof(e->url),url); e->priority=repo->priority; e->order=repo->order; } p = skip_ws(p); if (*p == ',') p++; }
    free(text); return 1;
}
static int package_url(const char* root, const char* item, char* result, size_t size) { if (_strnicmp(item,"https://",8)==0) return strcpy_s(result,size,item)==0; if (strstr(item,"://") || item[0]=='/' || strstr(item,"..")) return 0; return snprintf(result,size,"%s/%s",root,item)>0; }

typedef struct { unsigned long long major, minor, patch; const char* prerelease; size_t prerelease_length; } semver;
typedef struct { char name[128]; char version[64]; char arch[16]; } installed_entry;

static int parse_number(const char** p, unsigned long long* value) {
    const char* start = *p; unsigned long long result = 0;
    if (!isdigit((unsigned char)*start)) return 0;
    if (*start == '0' && isdigit((unsigned char)start[1])) return 0;
    while (isdigit((unsigned char)**p)) { unsigned digit = (unsigned)(**p - '0'); if (result > (ULLONG_MAX-digit)/10) return 0; result=result*10+digit; (*p)++; }
    *value=result; return *p > start;
}
static int valid_identifiers(const char* p, const char* end, int prerelease) {
    while (p < end) { const char* start=p; int numeric=1; while(p<end&&*p!='.') { if(!isalnum((unsigned char)*p)&&*p!='-') return 0; if(!isdigit((unsigned char)*p)) numeric=0; p++; } if(p==start||(prerelease&&numeric&&p-start>1&&*start=='0')) return 0; if(p<end)p++; } return 1;
}
static int semver_parse(const char* text, semver* result) {
    const char *p=text,*pre=NULL,*plus=NULL,*end;
    if(!text||!parse_number(&p,&result->major)||*p++!='.'||!parse_number(&p,&result->minor)||*p++!='.'||!parse_number(&p,&result->patch)) return 0;
    if(*p=='-'){ pre=++p; while(*p&&*p!='+')p++; }
    if(*p=='+'){ plus=++p; while(*p)p++; }
    if(*p) return 0; end=plus?plus-1:p;
    if(pre&&(pre==end||!valid_identifiers(pre,end,1))) return 0;
    if(plus&&(plus==p||!valid_identifiers(plus,p,0))) return 0;
    result->prerelease=pre; result->prerelease_length=pre?(size_t)(end-pre):0; return 1;
}
static int identifier_compare(const char* a,size_t an,const char* b,size_t bn){int ad=1,bd=1;size_t i,n;for(i=0;i<an;i++)if(!isdigit((unsigned char)a[i]))ad=0;for(i=0;i<bn;i++)if(!isdigit((unsigned char)b[i]))bd=0;if(ad&&bd){while(an>1&&*a=='0'){a++;an--;}while(bn>1&&*b=='0'){b++;bn--;}if(an!=bn)return an>bn?1:-1;}else if(ad!=bd)return ad?-1:1;n=an<bn?an:bn;for(i=0;i<n;i++)if(a[i]!=b[i])return(unsigned char)a[i]>(unsigned char)b[i]?1:-1;return an==bn?0:(an>bn?1:-1);}
static int semver_compare_parsed(const semver* a,const semver* b){const char *ap,*bp,*ae,*be;if(a->major!=b->major)return a->major>b->major?1:-1;if(a->minor!=b->minor)return a->minor>b->minor?1:-1;if(a->patch!=b->patch)return a->patch>b->patch?1:-1;if(!a->prerelease&&!b->prerelease)return 0;if(!a->prerelease)return 1;if(!b->prerelease)return-1;ap=a->prerelease;bp=b->prerelease;ae=ap+a->prerelease_length;be=bp+b->prerelease_length;while(ap<ae&&bp<be){const char *ax=memchr(ap,'.',(size_t)(ae-ap)),*bx=memchr(bp,'.',(size_t)(be-bp));size_t an=ax?(size_t)(ax-ap):(size_t)(ae-ap),bn=bx?(size_t)(bx-bp):(size_t)(be-bp);int c=identifier_compare(ap,an,bp,bn);if(c)return c;ap+=an+(ax!=NULL);bp+=bn+(bx!=NULL);}return ap==ae?(bp==be?0:-1):1;}
static int semver_compare(const char* a,const char* b,int* valid){semver av,bv;if(!semver_parse(a,&av)||!semver_parse(b,&bv)){if(valid)*valid=0;return 0;}if(valid)*valid=1;return semver_compare_parsed(&av,&bv);}
static int is_prerelease(const char* value){semver parsed;return semver_parse(value,&parsed)&&parsed.prerelease!=NULL;}

static int prerelease_path(char* path,size_t size){char root[PATH_SIZE];return wpm_get_data_root(root,sizeof(root))&&join_path(path,size,root,"config\\prerelease.txt");}
static int prerelease_effective(const char* package,int* overridden){char path[PATH_SIZE],line[512];FILE*f;int global=0,value=0,found=0;if(overridden)*overridden=0;if(!prerelease_path(path,sizeof(path)))return 0;f=wpm_fopen(path,"r");if(!f)return 0;while(fgets(line,sizeof(line),f)){char*tab;line[strcspn(line,"\r\n")]=0;if(_strnicmp(line,"global=",7)==0)global=_stricmp(line+7,"true")==0;else if((tab=strchr(line,'\t'))!=NULL){*tab=0;if(package&&_stricmp(line,package)==0){value=_stricmp(tab+1,"true")==0;found=1;}}}fclose(f);if(found){if(overridden)*overridden=1;return value;}return global;}
static int rewrite_prerelease(const char* package,int enabled,int remove){char path[PATH_SIZE],tmp[PATH_SIZE],line[512];FILE*in,*out;int found=0;if(!prerelease_path(path,sizeof(path))||snprintf(tmp,sizeof(tmp),"%s.tmp",path)<0)return 0;in=wpm_fopen(path,"r");out=wpm_fopen(tmp,"w");if(!out){if(in)fclose(in);return 0;}if(in){while(fgets(line,sizeof(line),in)){char copy[512],*tab;strcpy_s(copy,sizeof(copy),line);line[strcspn(line,"\r\n")]=0;if(!package&&_strnicmp(line,"global=",7)==0){found=1;if(!remove)fprintf(out,"global=%s\n",enabled?"true":"false");}else if(package&&(tab=strchr(line,'\t'))!=NULL){*tab=0;if(_stricmp(line,package)==0){found=1;if(!remove)fprintf(out,"%s\t%s\n",package,enabled?"true":"false");}else fputs(copy,out);}else fputs(copy,out);}fclose(in);}if(!remove&&!found){if(package)fprintf(out,"%s\t%s\n",package,enabled?"true":"false");else fprintf(out,"global=%s\n",enabled?"true":"false");}if(fclose(out)||!MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING)){DeleteFileA(tmp);return 0;}return !remove||found;}
int wpm_config_prerelease_set(const char* package_name,int enabled){if(!rewrite_prerelease(package_name,enabled,0)){printf("Error: could not save prerelease configuration.\n");return 0;}printf("Prerelease %s=%s.\n",package_name?package_name:"global",enabled?"true":"false");return 1;}
int wpm_config_prerelease_get(const char* package_name){int overridden=0,value=prerelease_effective(package_name,&overridden);printf("prerelease=%s (%s)\n",value?"true":"false",package_name?(overridden?"package override":"global setting"):"global setting");return 1;}
int wpm_config_prerelease_unset(const char* package_name){if(!package_name||!rewrite_prerelease(package_name,0,1)){printf("Error: package prerelease override was not found.\n");return 0;}printf("Prerelease override removed: %s\n",package_name);return 1;}

static int load_entries(repository* repositories,int* repository_count,package_entry* entries,int* entry_count,int offline){char cached[PATH_SIZE];int usable=0;*entry_count=0;if(!load_repositories(repositories,repository_count))return 0;for(int i=0;i<*repository_count;i++)if(refresh(&repositories[i],offline,0)&&cache_path(repositories[i].url,cached,sizeof(cached))){if(parse_index(cached,&repositories[i],entries,entry_count))usable=1;}return usable;}
static int load_installed(installed_entry* result,int* count){char root[PATH_SIZE],store[PATH_SIZE],search[PATH_SIZE],path[PATH_SIZE];WIN32_FIND_DATAA item;HANDLE find;*count=0;if(!wpm_get_data_root(root,sizeof(root))||!join_path(store,sizeof(store),root,"packages")||!join_path(search,sizeof(search),store,"*.zip"))return 0;find=FindFirstFileA(search,&item);if(find==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;do{wpm_package_info info;semver parsed;if(*count>=MAX_INSTALLED||!join_path(path,sizeof(path),store,item.cFileName))continue;if(!wpm_archive_inspect(path,&info)){printf("Warning: unreadable installed package record: %s\n  Path: %s\n",item.cFileName,path);continue;}if(!semver_parse(info.version,&parsed)){printf("Warning: installed package record has non-SemVer version '%s': %s\n  Path: %s\n  This legacy record is ignored for update selection; review the archive before deciding whether it is obsolete.\n",info.version,item.cFileName,path);continue;}installed_entry*e=&result[(*count)++];strcpy_s(e->name,sizeof(e->name),info.name);strcpy_s(e->version,sizeof(e->version),info.version);strcpy_s(e->arch,sizeof(e->arch),info.arch);}while(FindNextFileA(find,&item));FindClose(find);return 1;}
static int better_candidate(package_entry* candidate,package_entry* selected){int valid,c;if(!selected)return 1;c=semver_compare(candidate->version,selected->version,&valid);if(!valid)return 0;return c>0||(c==0&&(candidate->priority>selected->priority||(candidate->priority==selected->priority&&candidate->order<selected->order)));}
static package_entry* select_candidate(package_entry* entries,int count,const char* name,const char* arch,const char* exact_version,int install_mode){package_entry*selected=NULL;int allow_pre=prerelease_effective(name,NULL);for(int i=0;i<count;i++){semver parsed;if(_stricmp(entries[i].name,name)!=0||!semver_parse(entries[i].version,&parsed)){if(_stricmp(entries[i].name,name)==0)printf("Warning: ignoring invalid SemVer %s for %s.\n",entries[i].version,name);continue;}if(exact_version&&strcmp(entries[i].version,exact_version)!=0)continue;if(is_prerelease(entries[i].version)&&!allow_pre)continue;if(install_mode&&!arch){if(_stricmp(entries[i].arch,WPM_TARGET_ARCH)!=0&&_stricmp(entries[i].arch,"any")!=0)continue;if(selected&&_stricmp(selected->arch,WPM_TARGET_ARCH)==0&&_stricmp(entries[i].arch,"any")==0)continue;if(selected&&_stricmp(selected->arch,"any")==0&&_stricmp(entries[i].arch,WPM_TARGET_ARCH)==0){selected=&entries[i];continue;}}else if(_stricmp(entries[i].arch,arch)!=0)continue;if(better_candidate(&entries[i],selected))selected=&entries[i];}return selected;}
static int obtain(repository* repositories,package_entry* selected,int offline,char* path,size_t size){char root[PATH_SIZE],url[PATH_SIZE],legacy[PATH_SIZE];if(strpbrk(selected->name,"\\/:*")||strpbrk(selected->version,"\\/:*")||strpbrk(selected->arch,"\\/:*")||!package_url(repositories[selected->order].url,selected->url,url,sizeof(url))||!wpm_get_data_root(root,sizeof(root))||snprintf(path,size,"%s\\cache\\packages\\%s-%s-%s.zip",root,selected->name,selected->arch,selected->version)<0||snprintf(legacy,sizeof(legacy),"%s\\cache\\packages\\%s-%s-%s.zip",root,selected->name,selected->version,selected->arch)<0)return 0;if(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesA(legacy)!=INVALID_FILE_ATTRIBUTES)strcpy_s(path,size,legacy);if(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES){if(offline){printf("Error: package is not cached while offline: %s\n",selected->name);return 0;}if(!ensure_cache_directory("packages")||!download(url,path)){printf("Error: could not download package: %s\n",url);return 0;}}return 1;}

int wpm_repo_install(const char* package_name,const char* arch,const char* version,int offline,int allow_unsigned){repository repositories[MAX_REPOSITORIES];package_entry entries[MAX_PACKAGES],*selected;int rc,ec;char package[PATH_SIZE];semver parsed;if(version&&!semver_parse(version,&parsed)){printf("Error: --version requires valid SemVer.\n");return 0;}if(arch&&_stricmp(arch,"any")&&_stricmp(arch,"x86")&&_stricmp(arch,"x64")&&_stricmp(arch,"arm64")){printf("Error: unsupported architecture: %s\n",arch);return 0;}if(!load_entries(repositories,&rc,entries,&ec,offline))return 0;selected=select_candidate(entries,ec,package_name,arch,version,1);if(!selected){printf("Error: package was not found with the requested selectors: %s\n",package_name);return 0;}if(!obtain(repositories,selected,offline,package,sizeof(package))){printf("Error: invalid package URL in repository index.\n");return 0;}printf("Installing %s %s %s from %s\n",selected->name,selected->arch,selected->version,repositories[selected->order].url);return wpm_archive_install(package,allow_unsigned);}

static int requested(const char* name,const char** names,int count){for(int i=0;i<count;i++)if(_stricmp(name,names[i])==0)return 1;return 0;}
static int installed_is_latest(installed_entry* installed,int count,int index){int valid;for(int j=0;j<count;j++)if(index!=j&&_stricmp(installed[index].name,installed[j].name)==0&&_stricmp(installed[index].arch,installed[j].arch)==0&&semver_compare(installed[j].version,installed[index].version,&valid)>0)return 0;return 1;}
static int conflicting_any(installed_entry* installed,int count,int index){if(_stricmp(installed[index].arch,"any")!=0)return 0;for(int j=0;j<count;j++)if(index!=j&&_stricmp(installed[index].name,installed[j].name)==0&&_stricmp(installed[j].arch,"any")!=0)return 1;return 0;}
static package_entry* newer_candidate(package_entry* entries,int entry_count,installed_entry* item,const char* exact_version){package_entry*selected=select_candidate(entries,entry_count,item->name,item->arch,exact_version,0);int valid;if(!selected||semver_compare(selected->version,item->version,&valid)<=0||!valid)return NULL;return selected;}
static int print_upgrade_plan(installed_entry* installed,int installed_count,package_entry* entries,int entry_count,const char* arch){int count=0;for(int i=0;i<installed_count;i++){package_entry*selected;if((arch&&_stricmp(arch,installed[i].arch)!=0)||!installed_is_latest(installed,installed_count,i)||conflicting_any(installed,installed_count,i))continue;selected=newer_candidate(entries,entry_count,&installed[i],NULL);if(!selected)continue;if(!count)printf("Planned upgrades:\n");printf("  %s %s: %s -> %s\n",installed[i].name,installed[i].arch,installed[i].version,selected->version);count++;}return count;}
static int confirm_upgrade_plan(int assume_yes){char answer[16];if(assume_yes)return 1;printf("Proceed with these upgrades? [y/N]: ");fflush(stdout);if(!fgets(answer,sizeof(answer),stdin))return 0;answer[strcspn(answer,"\r\n")]=0;return _stricmp(answer,"y")==0||_stricmp(answer,"yes")==0;}

static int report_available_updates(void){repository repositories[MAX_REPOSITORIES];package_entry entries[MAX_PACKAGES];installed_entry installed[MAX_INSTALLED];int rc,ec,ic,count;if(!load_installed(installed,&ic)||!load_entries(repositories,&rc,entries,&ec,1))return 0;count=print_upgrade_plan(installed,ic,entries,ec,NULL);if(!count)printf("All installed packages are current.\n");else printf("%d package %s can be upgraded. Run 'wpm upgrade --all' to review and apply.\n",count,count==1?"identity":"identities");return 1;}

int wpm_repo_upgrade(const char** package_names,int package_count,int all,const char* arch,const char* version,int offline,int allow_unsigned,int assume_yes){repository repositories[MAX_REPOSITORIES];package_entry entries[MAX_PACKAGES];installed_entry installed[MAX_INSTALLED];int rc,ec,ic,failures=0,processed=0;semver parsed;if(version&&!semver_parse(version,&parsed)){printf("Error: --version requires valid SemVer.\n");return 0;}if(!load_installed(installed,&ic)||!load_entries(repositories,&rc,entries,&ec,offline))return 0;if(all){int planned=print_upgrade_plan(installed,ic,entries,ec,arch);if(planned&&!confirm_upgrade_plan(assume_yes)){printf("Upgrade cancelled; no packages were changed.\n");return 1;}if(planned)printf("\n");}for(int i=0;i<ic;i++){package_entry*selected;int valid,comparison;char cached[PATH_SIZE];int current=1,conflict=0,is_self;if(!all&&!requested(installed[i].name,package_names,package_count))continue;if(arch&&_stricmp(arch,installed[i].arch)!=0)continue;current=installed_is_latest(installed,ic,i);conflict=conflicting_any(installed,ic,i);if(!current)continue;if(conflict){printf("Warning: ignoring conflicting any installation for %s.\n",installed[i].name);if(arch&&_stricmp(arch,"any")==0)failures++;continue;}processed++;selected=select_candidate(entries,ec,installed[i].name,installed[i].arch,version,0);if(!selected){printf("%s %s %s is current.\n",installed[i].name,installed[i].arch,installed[i].version);printf("Result: %s %s current\n",installed[i].name,installed[i].arch);continue;}comparison=semver_compare(selected->version,installed[i].version,&valid);if(!valid||comparison<=0){if(version){printf("Error: requested upgrade version must be newer than installed %s.\n",installed[i].version);printf("Result: %s %s failed\n",installed[i].name,installed[i].arch);failures++;}else{printf("%s %s %s is current.\n",installed[i].name,installed[i].arch,installed[i].version);printf("Result: %s %s current\n",installed[i].name,installed[i].arch);}continue;}is_self=_stricmp(selected->name,"wpm")==0;if(!obtain(repositories,selected,offline,cached,sizeof(cached))||(is_self?!wpm_archive_schedule_self_upgrade(cached,allow_unsigned,selected->version,selected->arch,installed[i].version):!wpm_archive_upgrade(cached,allow_unsigned,selected->name,selected->version,selected->arch,installed[i].version))){printf("Result: %s %s failed\n",installed[i].name,installed[i].arch);failures++;continue;}printf("Result: %s %s %s\n",installed[i].name,installed[i].arch,is_self?"scheduled":"upgraded");}
for(int n=0;n<package_count;n++){int found=0;for(int i=0;i<ic;i++)if(_stricmp(package_names[n],installed[i].name)==0&&(!arch||_stricmp(arch,installed[i].arch)==0))found=1;if(!found){printf("Error: requested package identity is not installed: %s%s%s\n",package_names[n],arch?" ":"",arch?arch:"");failures++;}}
if(!processed&&!failures){printf("No installed packages matched the request.\n");return 0;}return failures==0;}
