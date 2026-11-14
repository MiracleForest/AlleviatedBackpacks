# P2-13 world systems: uuid dedup, nerfs, opening another player's backpack, mob backpacks and
# chest loot (the rules), the /backpackadmin commands and the config.
$r = Invoke-SelfTest 'dedup.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 4) { throw "dedup selftests: pass=$($r.Pass) fail=$($r.Fail)" }
$r = Invoke-SelfTest 'nerf.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 2) { throw "nerf selftests: pass=$($r.Pass) fail=$($r.Fail)" }
$r = Invoke-SelfTest 'access.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 1) { throw "access selftests: pass=$($r.Pass) fail=$($r.Fail)" }
$r = Invoke-SelfTest 'wearable.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 12) { throw "wearable selftests: pass=$($r.Pass) fail=$($r.Fail)" }

. (Join-Path $PSScriptRoot 'admin.ps1')
