-- Flash Freeze 379993 (Frost Mage): "Casting Glacial Spike has a chance to grant you Fingers of Frost". With no row the DUMMY effect cast
-- Fingers of Frost 44544 on every harmful spell event. Glacial Spike casts (199786, 1236209, 1262860) are the only spells with word 3 0x20.
-- Glacial Spike is a DUMMY-only spell, which the core treats as positive, so the proc is on CAST_SUCCESSFUL (ProcFlags2 0x4), not on
-- DEAL_HARMFUL_SPELL. Chance 0 = the spell's own, raised to 100 by its effect 0 spell mod.
DELETE FROM `spell_proc` WHERE `SpellId`=379993;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(379993,0,3,0x0,0x0,0x0,0x20,0x0,0x4,0x0,0x0,0x0,0x0,0x1,0,0,0,0);
