-- Augmentation Evoker: Ebon Might self buff, Upheaval damage on release, Mastery: Timewalker grants Shifting Sands on empower release
-- (a second script on Fire Breath beside the existing ones)
DELETE FROM `spell_script_names` WHERE (`spell_id`=395152 AND `ScriptName`='spell_evo_ebon_might')
 OR (`spell_id` IN (396286,408092) AND `ScriptName`='spell_evo_upheaval')
 OR (`spell_id` IN (357208,382266,396286,408092) AND `ScriptName`='spell_evo_mastery_timewalker')
 OR (`spell_id`=413984 AND `ScriptName`='spell_evo_shifting_sands');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(395152,'spell_evo_ebon_might'),
(396286,'spell_evo_upheaval'),
(408092,'spell_evo_upheaval'),
(357208,'spell_evo_mastery_timewalker'),
(382266,'spell_evo_mastery_timewalker'),
(396286,'spell_evo_mastery_timewalker'),
(408092,'spell_evo_mastery_timewalker'),
(413984,'spell_evo_shifting_sands');
