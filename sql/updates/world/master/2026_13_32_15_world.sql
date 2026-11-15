-- Mass Disintegrate and Bombardments
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_evo_mass_disintegrate_empower', 'spell_evo_bombardments_disintegrate', 'spell_evo_bombardments_mark');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(357208, 'spell_evo_mass_disintegrate_empower'),
(382266, 'spell_evo_mass_disintegrate_empower'),
(359073, 'spell_evo_mass_disintegrate_empower'),
(382411, 'spell_evo_mass_disintegrate_empower'),
(356995, 'spell_evo_bombardments_disintegrate'),
(434473, 'spell_evo_bombardments_mark');

DELETE FROM `spell_proc` WHERE `SpellId` IN (436336, 434473);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(436336,0x00,224,0x00000040,0x00000000,0x00000000,0x00000000,0x0,0x4,0x0,0x0,0x0,0x18,0x0,0,100,0,0), -- Mass Disintegrate
(434473,0x00,0,0x00000000,0x00000000,0x00000000,0x00000000,0xA22A8,0x0,0x7,0x0,0x0,0x0,0x0,0,30,2000,0); -- Bombardments

DELETE FROM `spell_custom_attr` WHERE `entry`=434481;
INSERT INTO `spell_custom_attr` (`entry`, `attributes`) VALUES
(434481, 8); -- SPELL_ATTR0_CU_SHARE_DAMAGE

DELETE FROM `areatrigger_template` WHERE `Id`=34697 AND `IsCustom`=0;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(34697, 0, 0, 0, 0, 70575);

DELETE FROM `areatrigger_create_properties` WHERE `Id` IN (31941, 31946) AND `IsCustom`=0;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`, `ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `MaxSummons`, `MaxUnits`, `PositionalSoundKitId`, `TimeToTargetScale`, `Speed`, `SpeedIsTime`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`, `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `TargetRoll`, `TargetPitch`, `TargetYaw`, `ScriptName`, `VerifiedBuild`) VALUES
(31941, 0, 34697, 0, 0, 73149, 0, 0, 0, -1, 0, 0, NULL, NULL, NULL, 0, 2000, 33.9964, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 'at_evo_bombardments', 70575), -- Spell: 434301 (Bombardments)
(31946, 0, 34697, 0, 0, 0, 0, 0, 0, -1, 0, 0, NULL, NULL, NULL, 0, 2000, 34.0041, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 'at_evo_bombardments', 70575); -- Spell: 434319 (Bombardments)

DELETE FROM `areatrigger_create_properties_spline_point` WHERE `AreaTriggerCreatePropertiesId` IN (31941, 31946) AND `IsCustom`=0;
INSERT INTO `areatrigger_create_properties_spline_point` (`AreaTriggerCreatePropertiesId`, `IsCustom`, `Idx`, `X`, `Y`, `Z`, `VerifiedBuild`) VALUES
(31941, 0, 0, -11.0487, -22.0973, -0.699219, 70575),
(31941, 0, 1, -11.0487, -22.0973, -0.699219, 70575),
(31941, 0, 2, 23.9514, 47.9028, 1.51578, 70575),
(31941, 0, 3, 23.9514, 47.9028, 1.51578, 70575),
(31946, 0, 0, -6.37294, 10.9252, -0.193024, 70575),
(31946, 0, 1, -6.37294, 10.9252, -0.193024, 70575),
(31946, 0, 2, 28.6271, -49.075, 0.867058, 70575),
(31946, 0, 3, 28.6271, -49.075, 0.867058, 70575);
