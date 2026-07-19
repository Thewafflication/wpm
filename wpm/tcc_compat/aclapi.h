#pragma once

/* Minimal ACL API surface used by WPM. TinyCC's compact WinAPI headers omit
 * aclapi.h, while the declarations are ABI-compatible with Windows XP. */
typedef enum _MULTIPLE_TRUSTEE_OPERATION {
    NO_MULTIPLE_TRUSTEE,
    TRUSTEE_IS_IMPERSONATE
} MULTIPLE_TRUSTEE_OPERATION;

typedef enum _TRUSTEE_FORM {
    TRUSTEE_IS_SID,
    TRUSTEE_IS_NAME,
    TRUSTEE_BAD_FORM,
    TRUSTEE_IS_OBJECTS_AND_SID,
    TRUSTEE_IS_OBJECTS_AND_NAME
} TRUSTEE_FORM;

typedef enum _TRUSTEE_TYPE {
    TRUSTEE_IS_UNKNOWN,
    TRUSTEE_IS_USER,
    TRUSTEE_IS_GROUP,
    TRUSTEE_IS_DOMAIN,
    TRUSTEE_IS_ALIAS,
    TRUSTEE_IS_WELL_KNOWN_GROUP,
    TRUSTEE_IS_DELETED,
    TRUSTEE_IS_INVALID,
    TRUSTEE_IS_COMPUTER
} TRUSTEE_TYPE;

typedef struct _TRUSTEE_A {
    struct _TRUSTEE_A *pMultipleTrustee;
    MULTIPLE_TRUSTEE_OPERATION MultipleTrusteeOperation;
    TRUSTEE_FORM TrusteeForm;
    TRUSTEE_TYPE TrusteeType;
    LPSTR ptstrName;
} TRUSTEE_A;

typedef enum _ACCESS_MODE {
    NOT_USED_ACCESS,
    GRANT_ACCESS,
    SET_ACCESS,
    DENY_ACCESS,
    REVOKE_ACCESS,
    SET_AUDIT_SUCCESS,
    SET_AUDIT_FAILURE
} ACCESS_MODE;

typedef struct _EXPLICIT_ACCESS_A {
    DWORD grfAccessPermissions;
    ACCESS_MODE grfAccessMode;
    DWORD grfInheritance;
    TRUSTEE_A Trustee;
} EXPLICIT_ACCESSA;

typedef enum _SE_OBJECT_TYPE {
    SE_UNKNOWN_OBJECT_TYPE,
    SE_FILE_OBJECT
} SE_OBJECT_TYPE;

#define NO_INHERITANCE 0

DWORD WINAPI SetEntriesInAclA(ULONG count, EXPLICIT_ACCESSA *entries,
                              PACL old_acl, PACL *new_acl);
DWORD WINAPI SetNamedSecurityInfoA(LPSTR object_name, SE_OBJECT_TYPE object_type,
                                   SECURITY_INFORMATION security_info,
                                   PSID owner, PSID group, PACL dacl, PACL sacl);
