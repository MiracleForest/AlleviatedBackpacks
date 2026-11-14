# Admin unit with a real client: the selftests in a local world, then the /backpackadmin commands and the
# /backpack admin extras typed in chat by the (operator) player. The screens are covered by
# `world.ps1` (native screens; no chest UI).
$r = Invoke-SelfTest 'admin.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 18) { throw "admin selftests: pass=$($r.Pass) fail=$($r.Fail)" }

Invoke-Chat '/clear @s'
$mark = Get-LogMark
Invoke-Chat '/backpackadmin list'
Wait-LogLine 'backpackadmin: list -> \d+ backpacks' 10 -Since $mark | Out-Null
Press-Key 'T'
Start-Sleep -Milliseconds 800
Save-Screenshot 'backpackadmin-list' | Out-Null
Press-Key ESC

$mark = Get-LogMark
Invoke-Chat '/backpack stats'
Wait-LogLine 'admin: stats stored=' 10 -Since $mark | Out-Null

$mark = Get-LogMark
Invoke-Chat '/backpackadmin removeNonPlayer true'
Wait-LogLine 'backpackadmin: removeNonPlayer true -> \d+' 10 -Since $mark | Out-Null
Assert-NoErrors
