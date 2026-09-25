param([Parameter(Mandatory = $true)][string]$Executable)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'verify-windows-xp-pe.ps1') -Executable $Executable
if ($osMajor -gt 5 -or ($osMajor -eq 5 -and $osMinor -gt 0) -or
    $subsystemMajor -gt 5 -or ($subsystemMajor -eq 5 -and $subsystemMinor -gt 0)) {
    throw 'The TLS probe requires a PE version newer than Windows 2000.'
}
$allowedWinsock = @('WSAStartup', 'WSACleanup', 'WSAGetLastError', '__WSAFDIsSet',
    'socket', 'closesocket', 'gethostbyname', 'htons', 'connect', 'select',
    'ioctlsocket', 'getsockopt', 'send', 'recv')
$unexpected = @($imports['ws2_32.dll'] | Where-Object { $_ -notin $allowedWinsock })
if ($unexpected.Count) { throw "Unexpected TLS Winsock imports: $($unexpected -join ', ')" }
if ($imports.ContainsKey('bcrypt.dll') -or $imports.ContainsKey('ncrypt.dll')) {
    throw 'The TLS probe must not statically import newer Windows crypto providers.'
}
Write-Host 'TLS PE version and socket/crypto import boundary passed; legacy execution still needs VM validation.'
