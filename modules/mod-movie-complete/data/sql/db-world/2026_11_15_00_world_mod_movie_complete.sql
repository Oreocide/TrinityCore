-- mod-movie-complete (Oreocide/TrinityCore#211): the spell the player casts on itself when the client finishes a movie.
-- Each row is retail's server cast in the tick after CMSG_COMPLETE_MOVIE (captures read 2026-11-15). Routine auras are
-- left out (463542, 227034, 396612), and only the root spell is kept when one triggers another.
CREATE TABLE IF NOT EXISTS `movie_complete_spell` (
  `MovieId` int unsigned NOT NULL,
  `SpellId` int unsigned NOT NULL,
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`MovieId`, `SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-movie-complete: spell cast on the player at movie completion';

DELETE FROM `movie_complete_spell` WHERE `MovieId` IN (963, 979, 981, 1013, 1019, 1021, 1041, 1050, 1061, 1062, 1063);
INSERT INTO `movie_complete_spell` (`MovieId`, `SpellId`, `Comment`) VALUES
(963,  374167,  '[DNT] Kill Credit: 187561 (66191 objective 427820); seen in 2 of 2 completions'),
(979,  403091,  '[DNT] 10.1 - Ch 01 - IGC Ended: credit 202491 (72976 objective 434372); retail also cast dummy 411779 Despawn Clone (left out)'),
(981,  400994,  'Teleport [DNT]: spell_target_position 400994/0, force-casts 400996'),
(1013, 428649,  'Exit the Halls of Awakening: credit 217407 (78761 objective 446605) + teleport (spell_target_position 428649/1)'),
(1019, 464916,  'Play Scene: teleport (spell_target_position 464916/0)'),
(1021, 464397,  '[DNT] Play Scene: credit 219652 (79237 objective 447692)'),
(1041, 1240583, 'Teleport (DNT): spell_target_position 1240583/0; its effect 340 (conversation 20248 via 1240899) is EffectNULL in this core'),
(1050, 1266472, 'Finish: removes 1250477 and 1269656, triggers 1266474 after 1.5 s'),
(1061, 1283050, 'Kill Credit (DNT): credit 260236 (86536 objective 470194)'),
(1062, 1279525, 'Post Arator: conversation 32803'),
(1063, 1263712, 'Cutscene ZQI - Cleanup [DNT]: removes 1263713, triggers 81040, conversation 32703');
