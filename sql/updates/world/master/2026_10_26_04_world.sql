-- Founder's Point laborers that walk a path must not carry the static work auras 1233777 Shoveling / 1265374 Hold Hammer / 1234001 Hold Mining Tool:
-- the auras drive the work animation and tool, and retail removes/applies them as the NPC stops and starts. Carry auras stay (legitimately played while walking).
UPDATE `creature_addon` a JOIN `creature` c ON c.`guid`=a.`guid` SET a.`auras`=NULLIF(TRIM(REPLACE(REPLACE(REPLACE(CONCAT(' ',a.`auras`,' '),' 1233777 ',' '),' 1265374 ',' '),' 1234001 ',' ')),'')
WHERE c.`map`=2735 AND c.`id` IN (246336,244850) AND c.`MovementType`=2 AND a.`auras` REGEXP '1233777|1265374|1234001';
