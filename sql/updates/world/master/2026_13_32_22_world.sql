-- Templar (Paladin hero talents): Light's Guidance only from Wake of Ashes / Divine Toll, Hammer of Light calls down Empyrean Hammers,
-- Light's Deliverance and Light's Judicator stacks
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_pal_empyrean_hammer','spell_pal_hammer_of_light','spell_pal_hammer_of_light_area','spell_pal_lights_deliverance','spell_pal_lights_guidance','spell_pal_lights_judicator');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(431398,'spell_pal_empyrean_hammer'),
(427453,'spell_pal_hammer_of_light'),
(429826,'spell_pal_hammer_of_light_area'),
(425518,'spell_pal_lights_deliverance'),
(427445,'spell_pal_lights_guidance'),
(1261525,'spell_pal_lights_judicator');
