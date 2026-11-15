-- 388511 Overflowing Mists: only Enveloping Mist applies 388513, each direct hit on its target casts the heal 388514, scaled by the talent
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_monk_overflowing_mists_trigger','spell_monk_overflowing_mists','spell_monk_overflowing_mists_heal');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(388512,'spell_monk_overflowing_mists_trigger'),
(388513,'spell_monk_overflowing_mists'),
(388514,'spell_monk_overflowing_mists_heal');
