-- Glacial Spike 199786 (Frost Mage): a missile with one DUMMY effect and no script, so it dealt no damage and applied no slow.
-- spell_mage_glacial_spike casts its damage spell 228600 and Chilled 205708 on each target hit.
DELETE FROM `spell_script_names` WHERE `spell_id`=199786 AND `ScriptName`='spell_mage_glacial_spike';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES (199786,'spell_mage_glacial_spike');
