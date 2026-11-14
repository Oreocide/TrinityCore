-- Fists of Fury: damage ticks
DELETE FROM `spell_script_names` WHERE `spell_id`=113656 AND `ScriptName`='spell_monk_fists_of_fury';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(113656, 'spell_monk_fists_of_fury');
