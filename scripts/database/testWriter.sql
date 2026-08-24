--- Purpose: Exercises the database as the read_write backend role.
---
--- Run with ON_ERROR_STOP=1; any failed expectation raises and the run
--- stops there.  Reaching the end means everything passed.
---
--- Run as the WRITER, not as a superuser.  A superuser bypasses every
--- privilege check, so the negative tests below -- the ones asserting
--- that even the writer cannot read a password hash -- would pass
--- against a database whose grants were completely wrong.
---
--- A note on transactions, because it shapes the file.  NOW() is the
--- transaction timestamp: it does not advance inside a statement, so a
--- DO block that inserts a row, sleeps, and then checks that time has
--- passed will always fail.  psql runs each statement in its own
--- transaction, so anything testing the passage of time is deliberately
--- split across separate top-level statements.  Do not merge them.
---
--- Copyright: Ben Baker (UUSS) distributed under the MIT license.

\echo '=== writer tests ==='

--------------------------------------------------------------------------
---                       Direct table access                           ---
--------------------------------------------------------------------------
--- The headline property: the backend that writes everything still
--- cannot read a password hash.

DO $$
BEGIN
    PERFORM 1 FROM users LIMIT 1;
    RAISE EXCEPTION 'FAIL: writer could SELECT from users';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: writer denied direct SELECT on users';
END $$;

DO $$
BEGIN
    PERFORM 1 FROM user_keys LIMIT 1;
    RAISE EXCEPTION 'FAIL: writer could SELECT from user_keys';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: writer denied direct SELECT on user_keys';
END $$;

DO $$
BEGIN
    UPDATE users SET password_hash = 'x' WHERE TRUE;
    RAISE EXCEPTION 'FAIL: writer could UPDATE users';
EXCEPTION WHEN insufficient_privilege THEN
    RAISE NOTICE 'ok: writer denied direct UPDATE on users';
END $$;

--------------------------------------------------------------------------
---                          Creating users                             ---
--------------------------------------------------------------------------

DO $$
BEGIN
    IF NOT add_user('alice', 'hash-alice', 'read_write') THEN
        RAISE EXCEPTION 'FAIL: add_user(alice) returned FALSE';
    END IF;
    RAISE NOTICE 'ok: add_user creates a user';

    IF add_user('alice', 'hash-again') THEN
        RAISE EXCEPTION 'FAIL: duplicate add_user(alice) returned TRUE';
    END IF;
    RAISE NOTICE 'ok: duplicate user rejected';

    --- Names are folded, so ALICE is the same person as alice.
    IF add_user('ALICE', 'hash-again') THEN
        RAISE EXCEPTION 'FAIL: add_user(ALICE) created a second alice';
    END IF;
    RAISE NOTICE 'ok: user names are case-folded';

    IF add_user('', 'hash') THEN
        RAISE EXCEPTION 'FAIL: add_user with empty name returned TRUE';
    END IF;
    IF add_user('bob', '') THEN
        RAISE EXCEPTION 'FAIL: add_user with empty hash returned TRUE';
    END IF;
    RAISE NOTICE 'ok: empty name and empty hash rejected';

    --- Trips the CHECK; must come back as FALSE, not as an exception
    --- the backend has to know how to catch.
    IF add_user('mallory', 'hash-m', 'superuser') THEN
        RAISE EXCEPTION 'FAIL: add_user accepted an unknown permission';
    END IF;
    RAISE NOTICE 'ok: unknown permission level rejected';
END $$;

DO $$
BEGIN
    IF get_password_hash('alice') <> 'hash-alice' THEN
        RAISE EXCEPTION 'FAIL: get_password_hash(alice) wrong';
    END IF;
    --- Whitespace and case are normalised on the way in.
    IF get_password_hash('  ALICE  ') <> 'hash-alice' THEN
        RAISE EXCEPTION 'FAIL: get_password_hash does not normalise input';
    END IF;
    IF get_password_hash('nobody') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: get_password_hash(nobody) not NULL';
    END IF;
    RAISE NOTICE 'ok: get_password_hash';

    IF get_user_permission('alice') <> 'read_write' THEN
        RAISE EXCEPTION 'FAIL: alice has the wrong permission';
    END IF;
    IF get_user_permission('nobody') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: get_user_permission(nobody) not NULL';
    END IF;
    RAISE NOTICE 'ok: get_user_permission';
END $$;

--------------------------------------------------------------------------
---                            Permissions                              ---
--------------------------------------------------------------------------

DO $$
BEGIN
    IF NOT add_user('carol', 'hash-carol') THEN
        RAISE EXCEPTION 'FAIL: add_user(carol) returned FALSE';
    END IF;
    --- The default matters: a user created without a stated level must
    --- come out read_only, never the more permissive one.
    IF get_user_permission('carol') <> 'read_only' THEN
        RAISE EXCEPTION 'FAIL: default permission is not read_only';
    END IF;
    RAISE NOTICE 'ok: permission defaults to read_only';

    IF NOT set_user_permission('carol', 'read_write') THEN
        RAISE EXCEPTION 'FAIL: set_user_permission returned FALSE';
    END IF;
    IF get_user_permission('carol') <> 'read_write' THEN
        RAISE EXCEPTION 'FAIL: set_user_permission did not stick';
    END IF;
    RAISE NOTICE 'ok: set_user_permission';

    IF set_user_permission('carol', 'root') THEN
        RAISE EXCEPTION 'FAIL: set_user_permission accepted a bad level';
    END IF;
    IF get_user_permission('carol') <> 'read_write' THEN
        RAISE EXCEPTION 'FAIL: rejected set_user_permission still changed it';
    END IF;
    IF set_user_permission('nobody', 'read_only') THEN
        RAISE EXCEPTION 'FAIL: set_user_permission(nobody) returned TRUE';
    END IF;
    RAISE NOTICE 'ok: bad permission level and unknown user rejected';
END $$;

--------------------------------------------------------------------------
---                              Logins                                 ---
--------------------------------------------------------------------------

DO $$
DECLARE v_last_login TIMESTAMPTZ;
BEGIN
    SELECT last_login INTO v_last_login FROM list_users() WHERE name = 'alice';
    IF v_last_login IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: a new user already has a last_login';
    END IF;
    IF NOT record_login('alice') THEN
        RAISE EXCEPTION 'FAIL: record_login(alice) returned FALSE';
    END IF;
    SELECT last_login INTO v_last_login FROM list_users() WHERE name = 'alice';
    IF v_last_login IS NULL THEN
        RAISE EXCEPTION 'FAIL: record_login did not set last_login';
    END IF;
    IF record_login('nobody') THEN
        RAISE EXCEPTION 'FAIL: record_login(nobody) returned TRUE';
    END IF;
    RAISE NOTICE 'ok: record_login';
END $$;

--------------------------------------------------------------------------
---                           Password change                           ---
--------------------------------------------------------------------------

DO $$
BEGIN
    IF NOT update_user_password('alice', 'hash-alice-2') THEN
        RAISE EXCEPTION 'FAIL: update_user_password returned FALSE';
    END IF;
    IF get_password_hash('alice') <> 'hash-alice-2' THEN
        RAISE EXCEPTION 'FAIL: password did not change';
    END IF;
    IF update_user_password('alice', '') THEN
        RAISE EXCEPTION 'FAIL: empty hash accepted';
    END IF;
    IF get_password_hash('alice') <> 'hash-alice-2' THEN
        RAISE EXCEPTION 'FAIL: rejected update still changed the password';
    END IF;
    IF update_user_password('nobody', 'hash') THEN
        RAISE EXCEPTION 'FAIL: update_user_password(nobody) returned TRUE';
    END IF;
    RAISE NOTICE 'ok: update_user_password';
END $$;

--------------------------------------------------------------------------
---                              Keys                                   ---
--------------------------------------------------------------------------

DO $$
DECLARE n INTEGER;
BEGIN
    IF NOT add_user_key('alice', 'laptop', 'pubkey-alice-laptop') THEN
        RAISE EXCEPTION 'FAIL: add_user_key returned FALSE';
    END IF;
    RAISE NOTICE 'ok: add_user_key';

    IF add_user_key('alice', 'laptop', 'pubkey-different') THEN
        RAISE EXCEPTION 'FAIL: duplicate key NAME accepted for one user';
    END IF;
    --- Globally unique: one key must not identify two people.
    IF add_user_key('carol', 'carol-laptop', 'pubkey-alice-laptop') THEN
        RAISE EXCEPTION 'FAIL: one public key registered to two users';
    END IF;
    IF add_user_key('nobody', 'k', 'pubkey-nobody') THEN
        RAISE EXCEPTION 'FAIL: add_user_key(nobody) returned TRUE';
    END IF;
    IF add_user_key('alice', '', 'pubkey-empty-name') THEN
        RAISE EXCEPTION 'FAIL: empty key name accepted';
    END IF;
    RAISE NOTICE 'ok: duplicate/unknown/empty key registrations rejected';

    IF get_user_by_key('pubkey-alice-laptop') <> 'alice' THEN
        RAISE EXCEPTION 'FAIL: get_user_by_key did not resolve to alice';
    END IF;
    IF get_user_by_key('pubkey-nonexistent') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: get_user_by_key resolved an unknown key';
    END IF;
    RAISE NOTICE 'ok: get_user_by_key';

    IF NOT record_key_use('pubkey-alice-laptop') THEN
        RAISE EXCEPTION 'FAIL: record_key_use returned FALSE';
    END IF;
    SELECT count(*) INTO n FROM list_user_keys('alice')
     WHERE key_name = 'laptop' AND last_used IS NOT NULL;
    IF n <> 1 THEN
        RAISE EXCEPTION 'FAIL: record_key_use did not stamp last_used';
    END IF;
    RAISE NOTICE 'ok: record_key_use';

    --- An already-expired key must not authenticate even though it is
    --- freshly registered and not revoked.
    IF NOT add_user_key('alice', 'expired', 'pubkey-alice-expired',
                        'ed25519', NOW() - INTERVAL '1 hour') THEN
        RAISE EXCEPTION 'FAIL: could not register an expired key';
    END IF;
    IF get_user_by_key('pubkey-alice-expired') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: an expired key still authenticates';
    END IF;
    RAISE NOTICE 'ok: expired keys do not authenticate';
END $$;

DO $$
DECLARE n INTEGER;
BEGIN
    IF NOT revoke_user_key('alice', 'laptop') THEN
        RAISE EXCEPTION 'FAIL: revoke_user_key returned FALSE';
    END IF;
    IF get_user_by_key('pubkey-alice-laptop') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: a revoked key still authenticates';
    END IF;
    --- Revoked, not deleted: the row is the audit trail.
    SELECT count(*) INTO n FROM list_user_keys('alice')
     WHERE key_name = 'laptop' AND revoked IS NOT NULL;
    IF n <> 1 THEN
        RAISE EXCEPTION 'FAIL: revoked key row did not survive for audit';
    END IF;
    IF revoke_user_key('alice', 'laptop') THEN
        RAISE EXCEPTION 'FAIL: re-revoking an inactive key returned TRUE';
    END IF;
    IF record_key_use('pubkey-alice-laptop') THEN
        RAISE EXCEPTION 'FAIL: record_key_use accepted a revoked key';
    END IF;
    RAISE NOTICE 'ok: revoke_user_key';
END $$;

--------------------------------------------------------------------------
---                       Provisioning users                            ---
--------------------------------------------------------------------------

DO $$
DECLARE v_deadline TIMESTAMPTZ;
BEGIN
    IF NOT add_provisional_user('dave', 'hash-dummy', INTERVAL '1 hour') THEN
        RAISE EXCEPTION 'FAIL: add_provisional_user returned FALSE';
    END IF;
    SELECT provisional_until INTO v_deadline
      FROM list_users() WHERE name = 'dave';
    IF v_deadline IS NULL THEN
        RAISE EXCEPTION 'FAIL: provisional user has no deadline';
    END IF;
    IF NOT user_must_change_password('dave') THEN
        RAISE EXCEPTION 'FAIL: provisional user not flagged';
    END IF;
    --- The dummy password has to actually work, or they cannot get in
    --- to replace it.
    IF get_password_hash('dave') <> 'hash-dummy' THEN
        RAISE EXCEPTION 'FAIL: provisional user cannot log in';
    END IF;
    RAISE NOTICE 'ok: add_provisional_user';

    IF add_provisional_user('eve', 'hash', INTERVAL '0') THEN
        RAISE EXCEPTION 'FAIL: zero interval accepted';
    END IF;
    IF add_provisional_user('eve', 'hash', INTERVAL '-1 hour') THEN
        RAISE EXCEPTION 'FAIL: negative interval accepted';
    END IF;
    IF add_provisional_user('eve', 'hash', NULL) THEN
        RAISE EXCEPTION 'FAIL: NULL interval accepted';
    END IF;
    IF add_provisional_user('alice', 'hash', INTERVAL '1 hour') THEN
        RAISE EXCEPTION 'FAIL: provisioned over an existing user';
    END IF;
    RAISE NOTICE 'ok: bad intervals and duplicate names rejected';

    --- An ordinary user must never look provisional.
    IF user_must_change_password('alice') THEN
        RAISE EXCEPTION 'FAIL: an activated user is flagged provisional';
    END IF;
    IF user_must_change_password('nobody') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: user_must_change_password(nobody) not NULL';
    END IF;
    RAISE NOTICE 'ok: user_must_change_password';
END $$;

--- Changing the password is the activation; there is no second call.
DO $$
BEGIN
    IF NOT update_user_password('dave', 'hash-dave-real') THEN
        RAISE EXCEPTION 'FAIL: provisional user could not set a password';
    END IF;
    IF user_must_change_password('dave') THEN
        RAISE EXCEPTION 'FAIL: password change did not activate the account';
    END IF;
    IF (SELECT provisional_until FROM list_users() WHERE name = 'dave')
       IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: deadline survived activation';
    END IF;
    RAISE NOTICE 'ok: changing the password activates the account';
END $$;

--------------------------------------------------------------------------
---                     Provisional expiry and sweep                    ---
--------------------------------------------------------------------------
--- Separate statements from here down: NOW() is frozen per transaction,
--- so a sleep inside one DO block would not move the clock.

DO $$
BEGIN
    IF NOT add_provisional_user('frank', 'hash-frank', INTERVAL '2 seconds') THEN
        RAISE EXCEPTION 'FAIL: could not provision frank';
    END IF;
    IF get_password_hash('frank') <> 'hash-frank' THEN
        RAISE EXCEPTION 'FAIL: frank cannot log in before his deadline';
    END IF;
    RAISE NOTICE 'ok: provisional user works before the deadline';
END $$;

SELECT pg_sleep(2.5);

--- The deadline binds on its own: frank is locked out here, before the
--- sweep has run.  If this passes only after the DELETE below, then the
--- cron interval is the real deadline and this property is missing.
DO $$
BEGIN
    IF get_password_hash('frank') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: expired user can still log in before the sweep';
    END IF;
    IF update_user_password('frank', 'hash-too-late') THEN
        RAISE EXCEPTION 'FAIL: expired user could activate after the deadline';
    END IF;
    RAISE NOTICE 'ok: expired user is locked out before the sweep runs';
END $$;

DO $$
DECLARE n INTEGER;
    v_before INTEGER;
    v_after INTEGER;
BEGIN
    SELECT count(*) INTO v_before FROM list_users();
    n := delete_expired_provisional_users();
    IF n <> 1 THEN
        RAISE EXCEPTION 'FAIL: sweep deleted % rows, expected 1', n;
    END IF;
    SELECT count(*) INTO v_after FROM list_users();
    IF v_after <> v_before - 1 THEN
        RAISE EXCEPTION 'FAIL: sweep removed % users, expected 1',
                        v_before - v_after;
    END IF;
    IF EXISTS (SELECT 1 FROM list_users() WHERE name = 'frank') THEN
        RAISE EXCEPTION 'FAIL: frank survived the sweep';
    END IF;
    RAISE NOTICE 'ok: sweep deletes exactly the expired account';

    --- The safety property: run it again and again, it must never
    --- touch an activated user.
    n := delete_expired_provisional_users();
    IF n <> 0 THEN
        RAISE EXCEPTION 'FAIL: second sweep deleted % rows', n;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM list_users() WHERE name = 'alice') THEN
        RAISE EXCEPTION 'FAIL: sweep deleted an activated user';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM list_users() WHERE name = 'dave') THEN
        RAISE EXCEPTION 'FAIL: sweep deleted a recently activated user';
    END IF;
    RAISE NOTICE 'ok: sweep leaves activated users alone';
END $$;

--------------------------------------------------------------------------
---                          Removing users                             ---
--------------------------------------------------------------------------

DO $$
BEGIN
    IF NOT add_user('grace', 'hash-grace') THEN
        RAISE EXCEPTION 'FAIL: add_user(grace) returned FALSE';
    END IF;
    IF NOT add_user_key('grace', 'laptop', 'pubkey-grace') THEN
        RAISE EXCEPTION 'FAIL: could not give grace a key';
    END IF;
    IF NOT remove_user('grace') THEN
        RAISE EXCEPTION 'FAIL: remove_user(grace) returned FALSE';
    END IF;
    IF get_password_hash('grace') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: removed user still resolves';
    END IF;
    --- Keys must go with the user, or a deleted account keeps a
    --- working credential.
    IF get_user_by_key('pubkey-grace') IS NOT NULL THEN
        RAISE EXCEPTION 'FAIL: a deleted user''s key still authenticates';
    END IF;
    IF remove_user('grace') THEN
        RAISE EXCEPTION 'FAIL: removing a gone user returned TRUE';
    END IF;
    RAISE NOTICE 'ok: remove_user cascades to keys';
END $$;

--------------------------------------------------------------------------
---                             list_users                              ---
--------------------------------------------------------------------------

DO $$
DECLARE n INTEGER;
BEGIN
    SELECT count(*) INTO n FROM list_users();
    IF n < 3 THEN
        RAISE EXCEPTION 'FAIL: list_users returned only % rows', n;
    END IF;
    RAISE NOTICE 'ok: list_users returns % users', n;
END $$;

--- The column that must never reach a frontend is not in the result
--- type at all, so this reference cannot resolve.  A column added
--- carelessly later turns this from an error into a pass, which is the
--- point of asserting it.
DO $$
BEGIN
    PERFORM password_hash FROM list_users();
    RAISE EXCEPTION 'FAIL: list_users exposes password_hash';
EXCEPTION WHEN undefined_column THEN
    RAISE NOTICE 'ok: list_users has no password_hash column';
END $$;

--------------------------------------------------------------------------
---                              Events                                 ---
--------------------------------------------------------------------------

DO $$
BEGIN
    INSERT INTO events (event_identifier, data)
        VALUES (1001, '{"magnitude": 4.2}'::jsonb);
    RAISE NOTICE 'ok: writer can INSERT events';

    IF (SELECT data->>'magnitude' FROM events WHERE event_identifier = 1001)
       <> '4.2' THEN
        RAISE EXCEPTION 'FAIL: event data did not round-trip';
    END IF;
    RAISE NOTICE 'ok: writer can SELECT events';
END $$;

DO $$
BEGIN
    --- The identifier comes from upstream, so a repeat must collide
    --- rather than silently make a second row.
    BEGIN
        INSERT INTO events (event_identifier, data)
            VALUES (1001, '{"magnitude": 9.9}'::jsonb);
        RAISE EXCEPTION 'FAIL: duplicate event_identifier accepted';
    EXCEPTION WHEN unique_violation THEN
        RAISE NOTICE 'ok: event_identifier is unique';
    END;

    BEGIN
        INSERT INTO events (event_identifier, data) VALUES (1002, NULL);
        RAISE EXCEPTION 'FAIL: NULL data accepted';
    EXCEPTION WHEN not_null_violation THEN
        RAISE NOTICE 'ok: data is NOT NULL';
    END;
END $$;

--- Separate transaction so NOW() has moved on since the INSERT.
DO $$
DECLARE v_created TIMESTAMPTZ;
        v_last_update TIMESTAMPTZ;
        v_created_after TIMESTAMPTZ;
        v_last_update_after TIMESTAMPTZ;
BEGIN
    SELECT created, last_update INTO v_created, v_last_update
      FROM events WHERE event_identifier = 1001;

    --- Deliberately sets created, the way an upsert listing every
    --- column would.  The trigger must refuse to let it move.
    UPDATE events
       SET data = '{"magnitude": 4.5}'::jsonb,
           created = NOW()
     WHERE event_identifier = 1001;

    SELECT created, last_update INTO v_created_after, v_last_update_after
      FROM events WHERE event_identifier = 1001;

    IF v_created_after <> v_created THEN
        RAISE EXCEPTION 'FAIL: created moved on UPDATE';
    END IF;
    RAISE NOTICE 'ok: created is pinned against UPDATE';

    IF v_last_update_after <= v_last_update THEN
        RAISE EXCEPTION 'FAIL: last_update did not advance on UPDATE';
    END IF;
    RAISE NOTICE 'ok: last_update advances on UPDATE';
END $$;

DO $$
BEGIN
    DELETE FROM events WHERE event_identifier = 1001;
    IF EXISTS (SELECT 1 FROM events WHERE event_identifier = 1001) THEN
        RAISE EXCEPTION 'FAIL: event not deleted';
    END IF;
    RAISE NOTICE 'ok: writer can DELETE events';
END $$;

\echo '=== writer tests passed ==='
