-- Lyssabel Dawnpetal intro (quest 91863): retail removes phase 27429 and adds 27184 when the player picks "Let's go!" (sniff 12.1.5.70575
-- 2026-10-23_14-37-06, phase shift #53745 -> #66751), so the original NPC 233063 disappears while the summoned guardian 249850 follows the player;
-- 27429 returns later in the sequence (#73019). Return condition = house acquired (objective 463610) is inferred, not proven.
UPDATE `creature` SET `PhaseId`=27429 WHERE `guid`=99001057 AND `id`=233063;
DELETE FROM `phase_area` WHERE `AreaId`=16588 AND `PhaseId`=27429;
INSERT INTO `phase_area` (`AreaId`,`PhaseId`,`Comment`) VALUES (16588,27429,'Founder''s Point: original Lyssabel Dawnpetal (hidden while the steward guardian follows the player)');
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup`=27429 AND `SourceEntry`=16588;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(26,27429,16588,0,0,47,0,91863,65,0,'',0,0,0,'','Lyssabel phase: My First Home not taken or rewarded'),
(26,27429,16588,0,1,47,0,91863,10,0,'',0,0,0,'','Lyssabel phase: quest active, steward not yet asked to join'),
(26,27429,16588,0,1,48,0,463609,0,0,'',0,0,0,'','Lyssabel phase: objective Ask the steward to join you not done'),
(26,27429,16588,0,2,47,0,91863,10,0,'',0,0,0,'','Lyssabel phase: quest active, house acquired'),
(26,27429,16588,0,2,48,0,463610,0,1,'',0,0,0,'','Lyssabel phase: objective Acquire a house done');
-- the summoned guardian follows its summoner (retail: continuous steering splines after the summon)
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid`=249850 AND `id`=4;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(249850,0,4,0,'',54,0,100,0,0,0,0,0,0,'',29,0,0,0,0,0,0,0,'',23,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal (guardian) - On Just Summoned - Follow Owner');
