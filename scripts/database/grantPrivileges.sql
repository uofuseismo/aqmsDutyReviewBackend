--- Purpose: Grants privileges.  Run after all objects exist.
---
--- Philosophy:
---   * The auth tables (users, user_keys) are reachable ONLY through
---     the auth functions.  Backend roles get EXECUTE, never direct
---     table access, so even the read_write backend cannot SELECT
---     password hashes.
---   * Those functions are therefore SECURITY DEFINER (they run with
---     the owner's privileges, not the caller's), with search_path
---     pinned, which is the standard hardening for definer functions.
---   * 'events' is different: it holds nothing the backend roles are
---     not allowed to see, so it gets ordinary table grants and the
---     backend writes ordinary SQL against it.
---   * PostgreSQL grants EXECUTE on new functions to PUBLIC by default.
---     Revoke wholesale, then re-grant deliberately.
---
--- Everything below is in 'public', so the auth functions and the event
--- objects are neighbours.  The separation between them is now carried
--- entirely by which grants are issued, not by schema membership --
--- which is why the SECURITY DEFINER list below is written out by name
--- rather than swept up with a catalog query.
---
--- Copyright: Ben Baker (UUSS) distributed under the MIT license.

--- Let the backend roles in the front door.
GRANT CONNECT ON DATABASE :"db_name" TO :"rw_role", :"ro_role";

--- USAGE lets a role reference objects in the schema; it does not by
--- itself allow reading any table.
GRANT USAGE ON SCHEMA public TO :"rw_role", :"ro_role";

--- Kill the EXECUTE-to-PUBLIC default on everything before re-granting.
REVOKE ALL ON ALL FUNCTIONS IN SCHEMA public FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA public FROM PUBLIC;

--------------------------------------------------------------------------
---                        Auth: SECURITY DEFINER                       ---
--------------------------------------------------------------------------
--- Named one at a time on purpose.  The schema-wide loop this replaces
--- would now also catch the event functions and silently run them as
--- the owner.  A function added later is NOT covered until it is added
--- here -- which is the point: it should be a decision, not a default.
---
--- The owner is whoever ran the installer (a superuser), so these
--- functions execute with that authority.  Their bodies touch only the
--- auth tables and take no dynamic SQL, which is what keeps that safe.

ALTER FUNCTION add_user(TEXT, TEXT, TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION remove_user(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION update_user_password(TEXT, TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION record_login(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION get_password_hash(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION set_user_permission(TEXT, TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION get_user_permission(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION add_provisional_user(TEXT, TEXT, INTERVAL, TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION user_must_change_password(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION delete_expired_provisional_users()
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION list_users()
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION add_user_key(TEXT, TEXT, TEXT, TEXT, TIMESTAMPTZ)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION revoke_user_key(TEXT, TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION record_key_use(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION get_user_by_key(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;
ALTER FUNCTION list_user_keys(TEXT)
    SECURITY DEFINER SET search_path = public, pg_temp;

--------------------------------------------------------------------------
---                          Auth: EXECUTE                              ---
--------------------------------------------------------------------------

--- Mutating functions: read_write backend only.
GRANT EXECUTE ON FUNCTION
    add_user(TEXT, TEXT, TEXT),
    remove_user(TEXT),
    update_user_password(TEXT, TEXT),
    record_login(TEXT),
    set_user_permission(TEXT, TEXT),
    add_provisional_user(TEXT, TEXT, INTERVAL, TEXT),
    delete_expired_provisional_users(),
    add_user_key(TEXT, TEXT, TEXT, TEXT, TIMESTAMPTZ),
    revoke_user_key(TEXT, TEXT),
    record_key_use(TEXT)
    TO :"rw_role";

--- Read/verify functions: both backends.  Note get_password_hash is
--- here because verifying a login IS a read; recording it is not.
--- list_users is safe for the reader because it cannot return a hash.
GRANT EXECUTE ON FUNCTION
    get_password_hash(TEXT),
    get_user_by_key(TEXT),
    get_user_permission(TEXT),
    user_must_change_password(TEXT),
    list_users(),
    list_user_keys(TEXT)
    TO :"rw_role", :"ro_role";

--- Deliberately NO table grants on users or user_keys, to either role.

--------------------------------------------------------------------------
---                              Events                                 ---
--------------------------------------------------------------------------

GRANT SELECT ON events TO :"rw_role", :"ro_role";
GRANT INSERT, UPDATE, DELETE ON events TO :"rw_role";

--- The trigger function is intentionally left SECURITY INVOKER: it runs
--- as whoever did the UPDATE, which is exactly the authority needed to
--- stamp a row that caller was already allowed to change.
