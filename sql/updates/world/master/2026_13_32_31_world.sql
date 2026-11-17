-- Rime 59057 (Frost Death Knight): "Frost Strike and Glacial Advance have a chance"; the old row (word 0 0x8080) matched Obliterate's hits
-- and Frostscythe. Frost Strike 49143 / 213793 = word 1 0x4, Glacial Advance 194913 = word 0 0x2, Frostbane 1228433 = word 2 0x2000000;
-- spell_dk_rime keeps only those casts (their hits share the bits)
UPDATE `spell_proc` SET `SpellFamilyMask0`=0x2, `SpellFamilyMask1`=0x4, `SpellFamilyMask2`=0x2000000 WHERE `SpellId`=59057;
