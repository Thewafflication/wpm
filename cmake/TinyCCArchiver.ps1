param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $TinyCC,

    [Parameter(Mandatory = $true, Position = 1)]
    [string] $Mode,

    [Parameter(Mandatory = $true, Position = 2)]
    [string] $Archive,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Inputs
)

$objects = foreach ($inputItem in $Inputs) {
    if ($inputItem.StartsWith('@')) {
        $responseFile = $inputItem.Substring(1)
        Get-Content -LiteralPath $responseFile | Where-Object { $_ -ne '' }
    }
    else {
        $inputItem
    }
}

& $TinyCC -ar $Mode $Archive $objects
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
