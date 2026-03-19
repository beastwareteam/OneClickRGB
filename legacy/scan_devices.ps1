# Real Hardware Scanner - All USB HID devices with VID/PID
Write-Host "===== ALL USB HID DEVICES =====" -ForegroundColor Cyan

Get-WmiObject Win32_PnPEntity | Where-Object {
    $_.DeviceID -like "HID\VID_*" -or $_.DeviceID -like "USB\VID_*"
} | ForEach-Object {
    $name = $_.Name
    $deviceId = $_.DeviceID

    # Extract VID and PID
    if ($deviceId -match "VID_([0-9A-F]{4})") {
        $vid = $matches[1]
    }
    if ($deviceId -match "PID_([0-9A-F]{4})") {
        $pid = $matches[1]
    }

    Write-Host ""
    Write-Host "Device: $name" -ForegroundColor Yellow
    Write-Host "  VID: $vid  PID: $pid"
    Write-Host "  DeviceID: $deviceId"
}

Write-Host ""
Write-Host "===== ASUS SPECIFIC DEVICES =====" -ForegroundColor Cyan
Get-WmiObject Win32_PnPEntity | Where-Object {
    $_.DeviceID -like "*0B05*" -or $_.Name -like "*ASUS*" -or $_.Name -like "*Aura*" -or $_.Name -like "*ROG*"
} | ForEach-Object {
    Write-Host ""
    Write-Host "Device: $($_.Name)" -ForegroundColor Green
    Write-Host "  DeviceID: $($_.DeviceID)"
    Write-Host "  Status: $($_.Status)"
}
