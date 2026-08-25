--- Purpose: Exercises the database as the read_only backend role.
---
--- Almost every test here is a negative one.  The writer suite proves
--- the functions work; this suite proves the reader cannot reach the
--- ones it should not, which is the half that fails silently -- a
--- missing REVOKE does not break anything, it just quietly grants
--- everyone EXECUTE and nobody notices until it matters.
---
--- Run as the READER.  Run with ON_ERROR_STOP=1.
---
--- Copyright: Ben Baker (UUSS) distributed under the MIT license.

\echo '=== reader tests ==='

--------------------------------------------------------------------------
---                       Direct table access                           ---
--------------------------------------------------------------------------

DO $$
BEGIN
    PERFORM 1 FROM users LIMIT 1;
    RAISE EXCEPTION 'FAIL: reader could SELECT from users';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied direct SELECT on users';
END $$;

DO $$
BEGIN
    PERFORM 1 FROM user_keys LIMIT 1;
    RAISE EXCEPTION 'FAIL: reader could SELECT from user_keys';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied direct SELECT on user_keys';
END $$;

--------------------------------------------------------------------------
---                     Functions the reader may call                   ---
--------------------------------------------------------------------------
--- Verifying a login is a read, so the reader gets the hash to check
--- against.  Recording that it happened is a write, and is denied below.

DO $$
BEGIN
    IF get_password_hash('alice') <> 'hash-alice-2' THEN
        RAISE EXCEPTION 'FAIL: reader cannot verify a password';
    END IF;
    IF get_user_permission('alice') <> 'read_write' THEN
        RAISE EXCEPTION 'FAIL: reader cannot read a permission';
    END IF;
    IF user_must_change_password('alice') THEN
        RAISE EXCEPTION 'FAIL: alice is flagged provisional';
    END IF;
    --- The reader decides what to render, so it needs to be able to ask
    --- who is an admin.  Asking is a read; acting is not.
    IF NOT user_is_admin('root') THEN
        RAISE EXCEPTION 'FAIL: reader cannot see that root is an admin';
    END IF;
    IF user_is_admin('alice') THEN
        RAISE EXCEPTION 'FAIL: alice reads as an admin';
    END IF;
    IF NOT user_has_permission('alice', 'read_write') THEN
        RAISE EXCEPTION 'FAIL: reader cannot evaluate a permission check';
    END IF;
    IF count_active_admins() < 1 THEN
        RAISE EXCEPTION 'FAIL: reader sees no admins';
    END IF;
    IF get_user_by_key('pubkey-alice-laptop') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: revoked key resolved for the reader';
    END IF;
    PERFORM count(*) FROM list_users();
    PERFORM count(*) FROM list_user_keys('alice');
    RAISE NOTICE 'ok: reader can call the read/verify functions';
END $$;

--------------------------------------------------------------------------
---                   Functions the reader may NOT call                 ---
--------------------------------------------------------------------------

--- Both the ungated function and its admin_ wrapper, because they fail
--- for different reasons and only one of them is a grant.  The reader
--- holding EXECUTE on an admin_ function would be a real hole even
--- though every call would still need an administrator as actor.
DO $$
BEGIN
    PERFORM add_user('intruder', 'hash-intruder');
    RAISE EXCEPTION 'FAIL: reader could add a user';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied add_user';
END $$;

DO $$
BEGIN
    PERFORM admin_add_user('root', 'intruder', 'hash-intruder');
    RAISE EXCEPTION 'FAIL: reader could call admin_add_user';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied admin_add_user';
END $$;

DO $$
BEGIN
    PERFORM remove_user('alice');
    RAISE EXCEPTION 'FAIL: reader could remove a user';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied remove_user';
END $$;

DO $$
BEGIN
    PERFORM admin_remove_user('root', 'alice');
    RAISE EXCEPTION 'FAIL: reader could call admin_remove_user';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied admin_remove_user';
END $$;

DO $$
BEGIN
    PERFORM admin_set_user_permission('root', 'alice', 'admin');
    RAISE EXCEPTION 'FAIL: reader could call admin_set_user_permission';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied admin_set_user_permission';
END $$;

DO $$
BEGIN
    PERFORM admin_reset_user_password('root', 'alice', 'hash-x',
                                      INTERVAL '1 day');
    RAISE EXCEPTION 'FAIL: reader could call admin_reset_user_password';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied admin_reset_user_password';
END $$;

DO $$
BEGIN
    PERFORM admin_add_provisional_user('root', 'intruder', 'hash',
                                       INTERVAL '1 hour');
    RAISE EXCEPTION 'FAIL: reader could call admin_add_provisional_user';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied admin_add_provisional_user';
END $$;

DO $$
BEGIN
    PERFORM update_user_password('alice', 'hash-hijacked');
    RAISE EXCEPTION 'FAIL: reader could change a password';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied update_user_password';
END $$;

DO $$
BEGIN
    PERFORM set_user_permission('alice', 'read_only');
    RAISE EXCEPTION 'FAIL: reader could change a permission';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied set_user_permission';
END $$;

DO $$
BEGIN
    PERFORM record_login('alice');
    RAISE EXCEPTION 'FAIL: reader could record a login';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied record_login';
END $$;

DO $$
BEGIN
    PERFORM add_provisional_user('intruder', 'hash', INTERVAL '1 hour');
    RAISE EXCEPTION 'FAIL: reader could provision a user';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied add_provisional_user';
END $$;

--- The sweep deletes rows.  A reader holding this could empty the
--- pending-user list.
DO $$
BEGIN
    PERFORM delete_expired_provisional_users();
    RAISE EXCEPTION 'FAIL: reader could run the sweep';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied delete_expired_provisional_users';
END $$;

DO $$
BEGIN
    PERFORM add_user_key('alice', 'intruder-key', 'pubkey-intruder');
    RAISE EXCEPTION 'FAIL: reader could register a key';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied add_user_key';
END $$;

DO $$
BEGIN
    PERFORM revoke_user_key('alice', 'expired');
    RAISE EXCEPTION 'FAIL: reader could revoke a key';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied revoke_user_key';
END $$;

DO $$
BEGIN
    PERFORM record_key_use('pubkey-alice-expired');
    RAISE EXCEPTION 'FAIL: reader could record a key use';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied record_key_use';
END $$;

--------------------------------------------------------------------------
---                              Events                                 ---
--------------------------------------------------------------------------

DO $$
BEGIN
    PERFORM count(*) FROM events;
    RAISE NOTICE 'ok: reader can SELECT events';
END $$;

DO $$
BEGIN
    INSERT INTO events (event_identifier, data)
        VALUES (2001, '{"magnitude": 1.0}'::jsonb);
    RAISE EXCEPTION 'FAIL: reader could INSERT an event';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied INSERT on events';
END $$;

DO $$
BEGIN
    UPDATE events SET data = '{}'::jsonb WHERE TRUE;
    RAISE EXCEPTION 'FAIL: reader could UPDATE an event';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied UPDATE on events';
END $$;

DO $$
BEGIN
    DELETE FROM events WHERE TRUE;
    RAISE EXCEPTION 'FAIL: reader could DELETE an event';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied DELETE on events';
END $$;

--------------------------------------------------------------------------
---                        Creating objects                             ---
--------------------------------------------------------------------------
--- On PostgreSQL 14 and older, PUBLIC holds CREATE on the public schema
--- by default.  createDatabase.sql revokes it; this checks that it took,
--- because on 15+ the REVOKE is a no-op and the test would pass for the
--- wrong reason on the version where it matters least.

DO $$
BEGIN
    EXECUTE 'CREATE TABLE reader_should_not_have_this (x INTEGER)';
    EXECUTE 'DROP TABLE reader_should_not_have_this';
    RAISE EXCEPTION 'FAIL: reader could create a table in public';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: reader denied CREATE in public';
END $$;

\echo '=== reader tests passed ==='
