Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Find and capture OneClickRGB window by process name
$proc = Get-Process -Name "OneClickRGB" -ErrorAction SilentlyContinue | Select-Object -First 1

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
}
public struct RECT {
    public int Left, Top, Right, Bottom;
}
"@

if ($proc -and $proc.MainWindowHandle -ne [IntPtr]::Zero) {
    $hwnd = $proc.MainWindowHandle
    Write-Host "Found OneClickRGB window (PID: $($proc.Id))"

    [Win32]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 500

    $rect = New-Object RECT
    [Win32]::GetWindowRect($hwnd, [ref]$rect)
    $bounds = New-Object System.Drawing.Rectangle($rect.Left, $rect.Top, ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top))
    Write-Host "Window bounds: $($bounds.X), $($bounds.Y), $($bounds.Width)x$($bounds.Height)"
} else {
    Write-Host "OneClickRGB process not found or has no visible window. Taking full screen."
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
}

$bitmap = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bounds.Size)
$bitmap.Save("$PSScriptRoot\02_main_window.png")
$graphics.Dispose()
$bitmap.Dispose()

Write-Host "Screenshot saved!"
