-- 321291 Death and Madness: Shadow Word: Death marks its target, a death while marked gives Insanity, a failed execute restores the charge
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_pri_death_and_madness','spell_pri_death_and_madness_debuff');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(32379,'spell_pri_death_and_madness'),
(322098,'spell_pri_death_and_madness_debuff');
