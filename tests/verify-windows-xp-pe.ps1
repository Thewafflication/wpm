param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$bytes = [IO.File]::ReadAllBytes($Executable)

function Read-U16([int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 2 -gt $bytes.Length) { throw "Invalid PE offset: $Offset" }
    [BitConverter]::ToUInt16($bytes, $Offset)
}

function Read-U32([int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 4 -gt $bytes.Length) { throw "Invalid PE offset: $Offset" }
    [BitConverter]::ToUInt32($bytes, $Offset)
}

function Read-AsciiZ([int]$Offset) {
    $end = $Offset
    while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
    if ($end -eq $bytes.Length) { throw "Unterminated PE string at offset $Offset" }
    [Text.Encoding]::ASCII.GetString($bytes, $Offset, $end - $Offset)
}

if ($bytes.Length -lt 64 -or (Read-U16 0) -ne 0x5A4D) { throw 'The file is not a valid MZ executable.' }
$peOffset = [int](Read-U32 0x3C)
if ((Read-U32 $peOffset) -ne 0x00004550) { throw 'The file does not contain a valid PE signature.' }

$coffOffset = $peOffset + 4
$machine = Read-U16 $coffOffset
$sectionCount = Read-U16 ($coffOffset + 2)
$optionalSize = Read-U16 ($coffOffset + 16)
$optionalOffset = $coffOffset + 20
if ((Read-U16 $optionalOffset) -ne 0x10B) { throw 'The XP artifact must be a PE32 executable.' }
if ($machine -ne 0x14C) { throw ('The XP artifact must target x86 (machine 0x014C); found 0x{0:X4}.' -f $machine) }

$osMajor = Read-U16 ($optionalOffset + 40)
$osMinor = Read-U16 ($optionalOffset + 42)
$subsystemMajor = Read-U16 ($optionalOffset + 48)
$subsystemMinor = Read-U16 ($optionalOffset + 50)
if ($osMajor -gt 5 -or ($osMajor -eq 5 -and $osMinor -gt 1)) {
    throw "PE operating-system version $osMajor.$osMinor is newer than Windows XP."
}
if ($subsystemMajor -gt 5 -or ($subsystemMajor -eq 5 -and $subsystemMinor -gt 1)) {
    throw "PE subsystem version $subsystemMajor.$subsystemMinor is newer than Windows XP."
}

$sectionOffset = $optionalOffset + $optionalSize
$sections = for ($index = 0; $index -lt $sectionCount; $index++) {
    $offset = $sectionOffset + (40 * $index)
    [pscustomobject]@{
        VirtualSize = Read-U32 ($offset + 8)
        VirtualAddress = Read-U32 ($offset + 12)
        RawSize = Read-U32 ($offset + 16)
        RawOffset = Read-U32 ($offset + 20)
    }
}

function Convert-RvaToOffset([uint32]$Rva) {
    foreach ($section in $sections) {
        $size = [Math]::Max([uint32]$section.VirtualSize, [uint32]$section.RawSize)
        if ($Rva -ge $section.VirtualAddress -and $Rva -lt $section.VirtualAddress + $size) {
            return [int]($section.RawOffset + ($Rva - $section.VirtualAddress))
        }
    }
    throw ('RVA 0x{0:X8} is outside the PE sections.' -f $Rva)
}

$importRva = Read-U32 ($optionalOffset + 104)
$imports = @{}
if ($importRva -ne 0) {
    $descriptorOffset = Convert-RvaToOffset $importRva
    while ((Read-U32 $descriptorOffset) -ne 0 -or (Read-U32 ($descriptorOffset + 12)) -ne 0) {
        $lookupRva = Read-U32 $descriptorOffset
        $nameRva = Read-U32 ($descriptorOffset + 12)
        $addressRva = Read-U32 ($descriptorOffset + 16)
        $dll = (Read-AsciiZ (Convert-RvaToOffset $nameRva)).ToLowerInvariant()
        $names = [Collections.Generic.List[string]]::new()
        $thunkOffset = Convert-RvaToOffset $(if ($lookupRva) { $lookupRva } else { $addressRva })
        while (($thunk = Read-U32 $thunkOffset) -ne 0) {
            if (($thunk -band 0x80000000) -eq 0) {
                $names.Add((Read-AsciiZ ((Convert-RvaToOffset $thunk) + 2)))
            }
            $thunkOffset += 4
        }
        $imports[$dll] = $names
        $descriptorOffset += 20
    }
}

$forbiddenKernel32Imports = @(
    'CancelIoEx', 'GetFileInformationByHandleEx', 'GetFinalPathNameByHandleA',
    'GetFinalPathNameByHandleW', 'GetSystemTimePreciseAsFileTime', 'GetTickCount64',
    'InitializeCriticalSectionEx', 'SetFileInformationByHandle'
)
$kernel32Imports = @($imports['kernel32.dll'])
$forbidden = @($kernel32Imports | Where-Object { $_ -in $forbiddenKernel32Imports })
if ($forbidden.Count) { throw "Post-XP kernel32 imports found: $($forbidden -join ', ')" }

Write-Host ('Verified Windows XP PE: x86, OS {0}.{1}, subsystem {2}.{3}, {4} kernel32 imports.' -f `
    $osMajor, $osMinor, $subsystemMajor, $subsystemMinor, $kernel32Imports.Count)
