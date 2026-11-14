-- Voidform
DELETE FROM `spell_script_names` WHERE `spell_id` IN (228260, 194249) AND `ScriptName` IN ('spell_pri_voidform', 'spell_pri_voidform_aura');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(228260, 'spell_pri_voidform'),
(194249, 'spell_pri_voidform_aura');

-- Void Volley and Voidform 1 last only as long as Voidform, which is not saved
DELETE FROM `spell_custom_attr` WHERE `entry` IN (1242171, 210199);
INSERT INTO `spell_custom_attr` (`entry`, `attributes`) VALUES
(1242171, 16777216), -- SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED
(210199, 16777216); -- SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED
