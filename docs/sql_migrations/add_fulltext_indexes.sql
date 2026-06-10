-- H3 : Amélioration des performances de recherche
-- À exécuter une seule fois sur la base quarm locale (MariaDB/MySQL).
-- Ces index remplacent les full-table-scans LIKE '%term%' par des recherches
-- FULLTEXT en mode BOOLEAN, passant de O(n) à O(log n) sur la colonne Name.
--
-- Usage :
--   mysql -u root quarm < docs/sql_migrations/add_fulltext_indexes.sql
--
-- Vérification post-migration :
--   SHOW INDEX FROM items WHERE Key_name = 'ft_name';
--   SHOW INDEX FROM npc_types WHERE Key_name = 'ft_name';

ALTER TABLE items
    ADD FULLTEXT INDEX ft_name (Name);

ALTER TABLE npc_types
    ADD FULLTEXT INDEX ft_name (name);
