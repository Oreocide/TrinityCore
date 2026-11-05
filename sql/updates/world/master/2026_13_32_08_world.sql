-- Skyriding: Skyriding periodic (Thrill of the Skies / Ground Skimming), Whirling Surge, Launch Boost, Aerial Halt
DELETE FROM `spell_script_names` WHERE `spell_id` IN (406095, 361584, 392752, 403092) AND `ScriptName` IN ('spell_gen_skyriding', 'spell_gen_skyriding_whirling_surge', 'spell_gen_skyriding_launch_boost', 'spell_gen_skyriding_aerial_halt');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(406095, 'spell_gen_skyriding'),
(361584, 'spell_gen_skyriding_whirling_surge'),
(392752, 'spell_gen_skyriding_launch_boost'),
(403092, 'spell_gen_skyriding_aerial_halt');
