-- Runs once on first MySQL container start (empty data volume).
-- Creates the DB user TrinityCore's worldserver/bnetserver connect as.
-- The auth/world/characters/hotfixes databases themselves are created
-- by worldserver's built-in Updater (Updates.AutoSetup = 1), not here.
CREATE USER IF NOT EXISTS 'trinity'@'%' IDENTIFIED BY 'trinity';
GRANT ALL PRIVILEGES ON *.* TO 'trinity'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
