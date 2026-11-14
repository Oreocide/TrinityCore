-- Ravager: weapon summon and damage ticks, Ravaged on each hit
DELETE FROM `spell_script_names` WHERE `spell_id` IN (228920, 156287) AND `ScriptName` IN ('spell_warr_ravager', 'spell_warr_ravager_damage');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(228920, 'spell_warr_ravager'),
(156287, 'spell_warr_ravager_damage');
