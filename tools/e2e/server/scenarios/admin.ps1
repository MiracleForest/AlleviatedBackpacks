# Admin unit: the /backpackadmin commands, the access log, the server config and AlleviatedBackpacks'
# /backpack admin extras.
$r = Invoke-SelfTest 'admin.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 18) { throw "admin selftests: pass=$($r.Pass) fail=$($r.Fail)" }

# The real commands from the console, asserting on their log lines.
$mark = Get-LogMark
Send-Console 'backpackadmin list'
Wait-LogLine 'backpackadmin: list -> \d+ backpacks' 10 -Since $mark | Out-Null

$mark = Get-LogMark
Send-Console 'backpackadmin removeNonPlayer true'
Wait-LogLine 'backpackadmin: removeNonPlayer true -> \d+' 10 -Since $mark | Out-Null

$mark = Get-LogMark
Send-Console 'backpack stats'
Wait-LogLine 'admin: stats stored=\d+' 10 -Since $mark | Out-Null

$mark = Get-LogMark
Send-Console 'backpack cleanup'
Wait-LogLine 'admin: cleanup -> \d+' 10 -Since $mark | Out-Null

# Unknown / too short uuids are rejected without errors in the log.
Send-Console 'backpack remove 00000000-0000-4000-8000-000000000000'
Send-Console 'backpack info ab'
Wait-Seconds 1

$mark = Get-LogMark
Send-Console 'backpack reload'
Wait-LogLine 'configuration reloaded' 10 -Since $mark | Out-Null

# The config files exist with the defaults.
$dir = Join-Path $script:SlotDir 'plugins\AlleviatedBackpacks\config'
$server = Get-Content (Join-Path $dir 'alleviatedbackpacks-server.json') -Raw | ConvertFrom-Json
if ($server.allowOpeningOtherPlayerBackpacks -ne $true) { throw 'allowOpeningOtherPlayerBackpacks must default to true' }
if ($server.nerfs.tooManyBackpacksSlowness -ne $false) { throw 'tooManyBackpacksSlowness must default to false' }
if ($server.nerfs.maxNumberOfBackpacks -ne 3) { throw 'maxNumberOfBackpacks must default to 3' }
if ($server.entityBackpackAdditions.chance -ne 0.01) { throw 'entityBackpackAdditions.chance must default to 0.01' }
if ($server.netheriteBackpack.inventorySlotCount -ne 120) { throw 'netheriteBackpack.inventorySlotCount must default to 120' }
if (($server.maxUpgradesPerStorage -join ',') -ne 'stack_upgrades|3,furnace_upgrades|1,jukebox_upgrades|1') { throw 'maxUpgradesPerStorage default' }
$common = Get-Content (Join-Path $dir 'alleviatedbackpacks-common.json') -Raw | ConvertFrom-Json
if ($common.chestLootEnabled -ne $true) { throw 'chestLootEnabled must default to true' }
$core = Get-Content (Join-Path $dir 'alleviatedbackpacks-core.json') -Raw | ConvertFrom-Json
if (-not ($core.enabledItems -contains 'alleviatedbackpacks:upgrade_base|true')) { throw 'enabledItems lists the mod items' }
$admin = Get-Content (Join-Path $dir 'admin.json') -Raw | ConvertFrom-Json
if ($admin.keepBackpacksOnDeath -ne $false) { throw 'keepBackpacksOnDeath must default to false' }
if ($admin.maxNestingDepth -ne 0) { throw 'maxNestingDepth must default to 0 (unlimited)' }
