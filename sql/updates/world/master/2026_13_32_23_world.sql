-- 1262034 Gift of the Wild: Mark of the Wild 1126 is more effective on the druid who casts it (a second script on 1126, beside spell_dru_mark_of_the_wild)
DELETE FROM `spell_script_names` WHERE `spell_id`=1126 AND `ScriptName`='spell_dru_gift_of_the_wild_mark';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1126,'spell_dru_gift_of_the_wild_mark');
