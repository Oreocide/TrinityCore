-- Empyrean Legacy 387170 (Retribution) and Tyr's Deliverance 1241275 (Holy): "Avenging Wrath empowers ..." / "Activating Avenging
-- Wrath releases ...". Both are PROC_TRIGGER_SPELL auras on CAST_SUCCESSFUL with no spell class mask and no spell_proc row, so every
-- successful cast procced them (and, with SPELL_ATTR3_CAN_PROC_FROM_PROCS, their own triggered casts). Restrict both to the Avenging
-- Wrath spells (family 10, mask word 1 0x100 | 0x2000: 31884, 454351, 1316624, 231895, 121163, 454373, 1319100). Avenging Crusader
-- and Sentinel are left out (not named by the talents).
DELETE FROM `spell_proc` WHERE `SpellId` IN (387170,1241275);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(387170,0,10,0x0,0x2100,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0x0,0,0,0,0),
(1241275,0,10,0x0,0x2100,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0x0,0,0,0,0);
