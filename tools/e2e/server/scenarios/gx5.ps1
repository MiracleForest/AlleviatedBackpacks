# GX-5 parity gaps: the upgrade limits, piglins vs gold backpacks, dispensers placing backpacks,
# everlasting vs explosion knock-back, the config keys, tag namespaces.
foreach ($glob in @(
        'admin.upgrade_group_limits', 'admin.max_upgrades',
        'crafting.two_upgrades_independent', 'stonecutter.two_upgrades_independent',
        'cooking.info', 'cooking.one_per_backpack', 'stack.items_registered', 'jukebox.*',
        'mainscreen.error_overlay', 'companion.upgrade_slots',
        'wearable.piglin_neutral', 'placed.dispenser_*', 'everlasting.explosion_knockback',
        'filter.*', 'templates.*')) {
    Invoke-SelfTest $glob -TimeoutSeconds 600 | Out-Null
}
