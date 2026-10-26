-- Skyriding thrust impulses (Lift Off / Skyward Ascent / Surge Forward)
DELETE FROM `spell_script_names` WHERE `spell_id` IN (374763, 372610, 372608) AND `ScriptName` = 'spell_gen_skyriding_impulse';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(374763, 'spell_gen_skyriding_impulse'),
(372610, 'spell_gen_skyriding_impulse'),
(372608, 'spell_gen_skyriding_impulse');
