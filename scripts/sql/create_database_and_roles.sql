/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

DROP DATABASE IF EXISTS erp_processing_context;
DROP SCHEMA IF EXISTS erp CASCADE;

-- remove old roles
DROP ROLE IF EXISTS role_proc_admin;
DROP ROLE IF EXISTS role_proc_user;
DROP ROLE IF EXISTS role_proc_maintenance;
DROP ROLE IF EXISTS role_proc_readonly;


-- These roles are assigned to the users based on the requirements. In this context, admin refers to the ability to
-- perform DDL statements, not superuser privileges.
--
-- Postgres doesn't differentiate between users and roles, this uses the prefix role_ to
-- denote such roles that should not have a login.
--
-- abbreviations:
-- proc: processing context
-- svc: service

-- Privileges: Alter, Create, Drop for database objects. This role should own all objects. To prevent
-- objects from being owned by users in this role the role is set to NOINHERIT
CREATE ROLE role_proc_admin WITH NOLOGIN NOINHERIT;

-- Privileges: Delete, Insert, Update, Select (may be restricted, mainly for the application user)
CREATE ROLE role_proc_user WITH NOLOGIN INHERIT;

-- Privileges: Manages Data retention, can have additional delete / truncate privileges
CREATE ROLE role_proc_maintenance WITH NOLOGIN INHERIT;

-- Privileges: SELECT on tables, USAGE on sequences, for use with read replica or for non service
-- users
CREATE ROLE role_proc_readonly WITH NOLOGIN INHERIT;


-- Database for ERP processing context
CREATE DATABASE erp_processing_context OWNER role_proc_admin ENCODING 'UTF-8';
GRANT CONNECT ON DATABASE erp_processing_context TO role_proc_user;
GRANT CONNECT ON DATABASE erp_processing_context TO role_proc_maintenance;
GRANT CONNECT ON DATABASE erp_processing_context TO role_proc_readonly;


-- Python3 is required for generating sortable UUID
CREATE EXTENSION IF NOT EXISTS plpython3u WITH SCHEMA pg_catalog;
