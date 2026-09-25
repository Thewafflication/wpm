param(
    [Parameter(Mandatory = $true)][string]$Probe,
    [Parameter(Mandatory = $true)][string]$Python
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../out'))
$fixture = Join-Path $root ('https-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$rsa = [Security.Cryptography.RSA]::Create(2048)
$rootRequest = [Security.Cryptography.X509Certificates.CertificateRequest]::new(
    'CN=WPM ephemeral TLS test CA', $rsa, [Security.Cryptography.HashAlgorithmName]::SHA256,
    [Security.Cryptography.RSASignaturePadding]::Pkcs1)
$rootRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($true, $false, 0, $true))
$rootRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new(
    [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyCertSign, $true))
$ca = $rootRequest.CreateSelfSigned([DateTimeOffset]::UtcNow.AddDays(-10), [DateTimeOffset]::UtcNow.AddDays(10))
try {
    [IO.File]::WriteAllText((Join-Path $fixture 'ca.pem'), $ca.ExportCertificatePem())
    foreach ($kind in @('valid', 'wrong-host', 'expired')) {
        $key = [Security.Cryptography.RSA]::Create(2048)
        try {
            $name = if ($kind -eq 'wrong-host') { 'wrong.invalid' } else { 'localhost' }
            $request = [Security.Cryptography.X509Certificates.CertificateRequest]::new(
                "CN=$name", $key, [Security.Cryptography.HashAlgorithmName]::SHA256,
                [Security.Cryptography.RSASignaturePadding]::Pkcs1)
            $san = [Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
            $san.AddDnsName($name)
            $request.CertificateExtensions.Add($san.Build())
            $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($false, $false, 0, $true))
            $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new(
                [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature, $true))
            $oids = [Security.Cryptography.OidCollection]::new()
            $null = $oids.Add([Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.1'))
            $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($oids, $false))
            $expires = if ($kind -eq 'expired') { [DateTimeOffset]::UtcNow.AddDays(-1) } else { [DateTimeOffset]::UtcNow.AddDays(2) }
            $serial = [byte[]]::new(16)
            [Security.Cryptography.RandomNumberGenerator]::Fill($serial)
            $leaf = $request.Create($ca, [DateTimeOffset]::UtcNow.AddDays(-2), $expires, $serial)
            try {
                [IO.File]::WriteAllText((Join-Path $fixture "$kind.pem"), $leaf.ExportCertificatePem())
                [IO.File]::WriteAllText((Join-Path $fixture "$kind.key"), $key.ExportPkcs8PrivateKeyPem())
            } finally { $leaf.Dispose() }
        } finally { $key.Dispose() }
    }
    & $Python (Join-Path $PSScriptRoot 'https-integration.py') --probe $Probe --fixtures $fixture
    if ($LASTEXITCODE -ne 0) { throw 'HTTPS integration tests failed.' }
} finally {
    $ca.Dispose()
    $rsa.Dispose()
    $resolved = [IO.Path]::GetFullPath($fixture)
    if (-not $resolved.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'TLS fixture cleanup escaped the test output directory.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
