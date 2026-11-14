-- Slice and Dice: Recuperator heal
DELETE FROM `spell_script_names` WHERE `spell_id`=315496 AND `ScriptName`='spell_rog_slice_and_dice';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(315496, 'spell_rog_slice_and_dice');
