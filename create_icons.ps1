Add-Type -AssemblyName System.Drawing

function Create-GetyBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    # Blue circle or rounded rect background
    $rect = New-Object System.Drawing.Rectangle 1, 1, ($size - 2), ($size - 2)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, 
        ([System.Drawing.Color]::FromArgb(30, 130, 230)), 
        ([System.Drawing.Color]::FromArgb(10, 50, 140)), 
        45.0
    $g.FillEllipse($brush, $rect)

    # Dark blue border
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(5, 30, 90)), 1.5
    $g.DrawEllipse($pen, $rect)

    # Golden lightning bolt / arrow
    $scale = $size / 32.0
    $pts = @(
        (New-Object System.Drawing.PointF (18 * $scale), (4 * $scale)),
        (New-Object System.Drawing.PointF (10 * $scale), (16 * $scale)),
        (New-Object System.Drawing.PointF (15 * $scale), (16 * $scale)),
        (New-Object System.Drawing.PointF (12 * $scale), (28 * $scale)),
        (New-Object System.Drawing.PointF (22 * $scale), (14 * $scale)),
        (New-Object System.Drawing.PointF (17 * $scale), (14 * $scale))
    )
    $boltBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect,
        ([System.Drawing.Color]::FromArgb(255, 230, 50)),
        ([System.Drawing.Color]::FromArgb(255, 150, 0)),
        90.0
    $g.FillPolygon($boltBrush, $pts)
    $boltPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(180, 100, 0)), 1.0
    $g.DrawPolygon($boltPen, $pts)

    $g.Dispose()
    return $bmp
}

$bmp16 = Create-GetyBitmap 16
$bmp32 = Create-GetyBitmap 32
$bmp48 = Create-GetyBitmap 48

# Save 32x32 as PNG
$bmp32.Save("resources/icons/app.png", [System.Drawing.Imaging.ImageFormat]::Png)

# Save as .ico using icon handle
$hIcon = $bmp32.GetHicon()
$icon = [System.Drawing.Icon]::FromHandle($hIcon)
$fs = New-Object System.IO.FileStream "resources/icons/app.ico", ([System.IO.FileMode]::Create)
$icon.Save($fs)
$fs.Close()
$icon.Dispose()

Write-Output "Icons created successfully!"
