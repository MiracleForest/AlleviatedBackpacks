# P2-2: the backpack settings (main / item display / no sort / memory, templates persistence) and
# settings template packs.
$r = Invoke-SelfTest 'storagesettings.*'
if ($r.Pass -lt 5 -or $r.Fail -gt 0) { throw "storagesettings selftests: pass=$($r.Pass) fail=$($r.Fail)" }
$r = Invoke-SelfTest 'templates.*'
if ($r.Pass -lt 1 -or $r.Fail -gt 0) { throw "templates selftests: pass=$($r.Pass) fail=$($r.Fail)" }
# The phase-1 GUI and the companion screen actions still pass with the settings.
$r = Invoke-SelfTest 'gui.*'
if ($r.Fail -gt 0) { throw "gui selftests failed" }
$r = Invoke-SelfTest 'companion.*'
if ($r.Fail -gt 0) { throw "companion selftests failed" }
