# GX-2 (client, `-Mode local` and `-Mode remote-mod`): what of the upgrade / world gaps a player sees.
#   A mob's jukebox backpack plays attached to the mob (the client attaches the record to it).

if ($env:ABP_E2E_HOST_DIR) { Wait-LogLine 'companion: server ready' 90 | Out-Null }

# ---- A jukebox mob's music follows the mob ------------------------------------------------------------
$mark = Get-LogMark
$r = Invoke-SelfTest 'wearable.mob_jukebox_follows'
if ($r.Fail -gt 0 -or $r.Pass -lt 1) { throw "wearable.mob_jukebox_follows: pass=$($r.Pass) fail=$($r.Fail)" }
$attached = (Get-LeviLog -Since $mark) | Where-Object { $_ -match 'jukebox: playing .* attached to entity' -and $_ -notmatch '\[host\]' }
if (-not $attached) { throw 'the client did not attach the mob jukebox sound to the mob' }
Write-Step "client: $($attached | Select-Object -First 1)"

Assert-NoErrors
