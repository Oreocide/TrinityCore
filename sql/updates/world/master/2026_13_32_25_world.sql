-- 215479 Shuffle (Brewmaster Monk): Keg Smash, Blackout Kick and Spinning Crane Kick grant it
-- each DELETE is keyed on its own spell and name: Blackout Kick 205523 keeps spell_gen_trigger_exclude_caster_aura_spell
DELETE FROM `spell_script_names` WHERE (`spell_id`=121253 AND `ScriptName`='spell_monk_keg_smash_shuffle') OR (`spell_id`=205523 AND `ScriptName`='spell_monk_blackout_kick_shuffle') OR (`spell_id`=107270 AND `ScriptName`='spell_monk_spinning_crane_kick_shuffle');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(121253,'spell_monk_keg_smash_shuffle'),
(205523,'spell_monk_blackout_kick_shuffle'),
(107270,'spell_monk_spinning_crane_kick_shuffle');
