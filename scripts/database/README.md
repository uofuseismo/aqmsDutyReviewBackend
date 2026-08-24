# AQMS DRP Backend database

A PostgreSQL database holding frontend users, their public keys, and an
event store.  Everything lives in the default `public` schema; there are
no others.  Target is PostgreSQL 18, floor is 14.

## Layout

| File | What it does |
| --- | --- |
| `create.sh` | The installer.  Everything site-specific is in its CONFIGURATION block. |
| `createDatabase.sql` | Cluster roles and the database.  Runs against the maintenance database. |
| `createTables.sql` | Every table and function. |
| `grantPrivileges.sql` | Privileges.  Runs last, because it names objects from the step before. |
| `test.sh` | Builds a throwaway database and runs the suites against it. |
| `testWriter.sql` | 39 checks as the read_write role. |
| `testReader.sql` | 18 checks as the read_only role. |

## One database per system

There are no regions and no per-region schemas; the systems are the only
dimension, so each gets its own database:

    sudo -u postgres AQMSDB_RW_PASSWORD=... AQMSDB_RO_PASSWORD=... \
        ./create.sh aqmsdb_test

    sudo -u postgres AQMSDB_RW_PASSWORD=... AQMSDB_RO_PASSWORD=... \
        ./create.sh aqmsdb_prod

Users do not cross databases: adding someone to test does not add them
to prod, and they register keys in each separately.  Roles *are*
cluster-wide, so both systems on one server share `aqmsdb_writer` and
`aqmsdb_reader`.

Note the password assignments come after `sudo`, not before it.  `sudo`
scrubs the environment, so setting them in your own shell first lands
you in the placeholder abort.

`postgres` also has to be able to read these files, since `\i` runs as
that user.  Under `/home/yourname/` it usually cannot; somewhere like
`/usr/local/share/aqmsdb/` works.

**Set `DROP_EXISTING=0` before prod goes live.**  It defaults to 1, and
a stray run takes the database with it.

## Testing

    sudo -u postgres ./test.sh          # build, test, drop
    sudo -u postgres ./test.sh --keep   # leave it up to poke at

The suites run as the backend roles, never as a superuser: a superuser
bypasses every privilege check, so the negative tests -- the ones
asserting that even the writer cannot read a password hash -- would pass
against a database with no grants at all.

The test roles connect over `--host=localhost`, so `pg_hba.conf` needs a
`scram-sha-256` line covering `127.0.0.1/32`.  A unix-socket connection
lands on peer auth, which maps to your OS user and cannot authenticate
as a test role whatever password it is given.

`test.sh` drops the database it is pointed at.  `aqmsdb_test` and
`aqmsdb_prod` are in its refuse-list; add any other deployment names.

## How the pieces work

### Users are unreachable except through functions

Neither backend role holds table privileges on `users` or `user_keys`.
`SELECT * FROM users` is denied even to the writer -- that is the design,
not a misconfiguration.  The functions are `SECURITY DEFINER` with
`search_path` pinned, so a compromised backend cannot read a password
hash even though it can create and delete users.

The `SECURITY DEFINER` list in `grantPrivileges.sql` is written out by
name rather than swept up with a catalog query.  A function added later
is not covered until it is added there, which is deliberate: in a single
schema, a catalog loop would also flip the event trigger function.

Plaintext passwords never enter the database.  The backend hashes with
something like libsodium's `crypto_pwhash_str` and stores the result.

### Provisioning

There is no registration workflow.  An operator's script creates the
account with a dummy password, hands it over out of band, and the
account deletes itself if it is never turned into a real one:

    SELECT add_provisional_user('jsmith', '$argon2id$...', INTERVAL '3 days');

`provisional_until` carries both facts: non-NULL means the account still
has the password it was issued, and names the instant it dies.
Changing the password clears it, so activation is not a separate call
the frontend can forget.  `password_updated` cannot answer this -- it is
set at creation too, so "never changed it" and "changed it immediately"
look identical.

A timer runs the sweep:

    SELECT delete_expired_provisional_users();

The schedule is not load-bearing.  `get_password_hash` and
`get_user_by_key` both refuse past-deadline accounts, so an expired user
cannot log in whether or not the sweep has reached them; without that,
the cron interval quietly becomes the real deadline.  The sweep only
ever sees rows with a non-NULL deadline, so it cannot delete a real user
however often it runs.

Two things the calling script owns:

* **Generate a distinct random dummy password per user** and hash it
  like any other.  It is a live credential for as long as it lives.  A
  shared `changeme` across fifteen accounts is the failure this design
  otherwise invites: unwatched accounts with a known password.
* **Restrict the provisional session.**  After a successful login, call
  `user_must_change_password(name)`; when it returns TRUE, allow nothing
  but the password change.  Whoever is on the other end has proved only
  that they received an email.

### Events

Unlike the auth tables, `events` is reached directly -- there is nothing
to hide behind a function -- so the roles hold ordinary table grants.

`event_identifier` is a `BIGINT` primary key supplied by the writer.  No
identity column, no sequence: the event already has an identity upstream
and this table records it rather than inventing a second one.

`created` is stamped on insert and pinned by the trigger; `last_update`
advances on every update.  The pinning matters because an upsert that
lists every column would otherwise reset `created` on each re-write, and
it would silently come to mean "last written" instead of "first seen".

## Not done yet

* No provisioning script -- `add_provisional_user` has no caller.
* The sweep is inert until something calls it on a timer.
* `createProcedures.sql`, referenced by the original installer, never
  surfaced.  The functions currently live in `createTables.sql`.
* Backups are not addressed anywhere here.
