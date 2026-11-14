# P2-13 world systems with a real client (`-Mode local` and `-Mode remote-mod`):
#   - opening another player's backpack: a simulated player stands with its back to us, wearing a
#     backpack; a right click on its back opens it as the native screen;
#   - `/backpackadmin list` and `/backpackadmin give` typed in chat.
# Server-side rules are covered by the selftests (server harness `world`).

Wait-LogLine 'companion: server ready' 90 | Out-Null
Write-Step 'companion handshake done'

# Remote: a chat opened right after a screen closed can get lost. Retries until `pattern` is logged.
function Invoke-ChatUntil([string]$text, [string]$pattern, [int]$tries = 3) {
    for ($t = 0; $t -lt $tries; $t++) {
        Start-Sleep -Milliseconds 1000
        $m = Get-LogMark
        Invoke-Chat $text
        try { return (Wait-LogLine $pattern 8 -Since $m) } catch { Write-Step "no '$pattern' after '$text'; retrying"; Press-Key ESC; Start-Sleep -Milliseconds 600 } # e.g. the vanilla emote wheel (also B) took the input
    }
    throw "'$text' never logged '$pattern'"
}

# ---- setup -------------------------------------------------------------------------------------------
Invoke-Chat '/clear @s'
$mark = Get-LogMark
Invoke-Chat '/backpack companion setup'
$line = Wait-LogLine 'companion setup: gave netherite backpack ([0-9a-f-]{36})' 15 -Since $mark
if ($line -notmatch 'backpack ([0-9a-f-]{36})') { throw "no uuid: $line" }
$netheriteUuid = $Matches[1]

# ---- another player's backpack ----------------------------------------------------------------------
Press-Key '5' # empty hand
Invoke-ChatUntil '/backpack world target' 'world target: spawned abp_e2e_target wearing backpack' | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'target-back' | Out-Null
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine "access: .* -> abp_e2e_target's backpack: opened" 10 -Since $mark | Out-Null
Wait-LogLine 'companion: drawing the native backpack screen' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 900
Save-Screenshot 'other-player-backpack' | Out-Null
if ((Get-LeviLog -Since $mark) -match 'menu shown: ') { throw 'a fake chest was opened for a companion player' }
Press-Key 'E'
Start-Sleep -Milliseconds 800
Invoke-Chat '/backpack world untarget'

# ---- /backpackadmin ------------------------------------------------------------------------------------
Invoke-ChatUntil '/backpackadmin list' 'backpackadmin: list -> [1-9]\d* backpacks' | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'backpackadmin-list' | Out-Null # the chat lines show on the HUD for a few seconds

Invoke-ChatUntil "/backpackadmin give @s $netheriteUuid" "backpackadmin: gave backpack $netheriteUuid to 1 players" | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'backpackadmin-give' | Out-Null
Assert-NoErrors
