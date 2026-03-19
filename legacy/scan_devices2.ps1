# Real Hardware Scanner - Clean version
Write-Host "===== ERKANNTE USB/HID GERAETE =====" -ForegroundColor Cyan
Write-Host ""

$devices = @()

Get-WmiObject Win32_PnPEntity | Where-Object {
    $_.DeviceID -like "HID\VID_*" -or $_.DeviceID -like "USB\VID_*"
} | ForEach-Object {
    $name = $_.Name
    $deviceId = $_.DeviceID

    # Extract VID and PID from DeviceID
    $vidMatch = [regex]::Match($deviceId, "VID_([0-9A-Fa-f]{4})")
    $pidMatch = [regex]::Match($deviceId, "PID_([0-9A-Fa-f]{4})")

    if ($vidMatch.Success -and $pidMatch.Success) {
        $vid = $vidMatch.Groups[1].Value
        $productId = $pidMatch.Groups[1].Value

        $obj = [PSCustomObject]@{
            Name = $name
            VID = $vid
            ProductID = $productId
            DeviceID = $deviceId
        }
        $devices += $obj
    }
}

# Remove duplicates and sort
$uniqueDevices = $devices | Sort-Object VID, ProductID, Name -Unique

Write-Host "Gefundene Geraete:" -ForegroundColor Yellow
Write-Host ""

$uniqueDevices | ForEach-Object {
    Write-Host "[$($_.VID):$($_.ProductID)] $($_.Name)" -ForegroundColor Green
    Write-Host "    DeviceID: $($_.DeviceID)"
    Write-Host ""
}

Write-Host ""
Write-Host "===== ZUSAMMENFASSUNG =====" -ForegroundColor Cyan

# Group by VID
$byVendor = $uniqueDevices | Group-Object VID

foreach ($vendor in $byVendor) {
    $vendorName = switch ($vendor.Name) {
        "0B05" { "ASUS" }
        "1038" { "SteelSeries" }
        "3299" { "Unbekannt (3299)" }
        "0BDA" { "Realtek" }
        "0D8C" { "C-Media" }
        "058F" { "Alcor Micro" }
        "8087" { "Intel" }
        "1532" { "Razer" }
        "046D" { "Logitech" }
        "1E71" { "NZXT" }
        "2516" { "Cooler Master" }
        "1462" { "MSI" }
        default { "Unbekannt" }
    }
    Write-Host ""
    Write-Host "Hersteller: $vendorName (VID: $($vendor.Name))" -ForegroundColor Yellow
    foreach ($dev in $vendor.Group) {
        Write-Host "  - [$($dev.ProductID)] $($dev.Name)"
    }
}
