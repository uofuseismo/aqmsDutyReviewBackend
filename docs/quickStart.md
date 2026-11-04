# Quick start

This gets the backend running on one Linux server: the container from quay,
a PostgreSQL database for users, and nginx or Apache in front of it.  It is
meant for trying the software out.  A production deployment wants the same
pieces with more care (see [Before production](#before-production)).

```
browser ──https──▶ nginx/apache ──/api/──▶ 127.0.0.1:8090 ──▶ container :8000
                        │                                        │
                        └── serves the frontend build             ├──▶ DRP user database
                                                                  └──▶ AQMS database
```

## What you need

* A server with `podman` (or `docker`) and PostgreSQL 14 or newer.
* Read access to an **existing AQMS PostgreSQL database**.  The backend
  reviews events in it.  This guide does not create one.
* `openssl`, and Python 3 with [PyNaCl](https://pypi.org/project/PyNaCl/),
  to make keys and the first password hash.
* nginx or Apache.

## 1. Create the user database

The backend keeps its users in a small database of its own, separate from
AQMS.  The installer is in [`scripts/database`](../scripts/database).
`postgres` has to be able to read the scripts, so copy them somewhere other
than your home directory first:

```bash
sudo install -d -m 755 /usr/local/share/aqmsdb
sudo cp scripts/database/*.sql scripts/database/*.sh /usr/local/share/aqmsdb/
cd /usr/local/share/aqmsdb

sudo -u postgres AQMSDB_RW_PASSWORD='pick-one' AQMSDB_RO_PASSWORD='pick-another' \
    ./create.sh aqmsdrp_demo
```

The passwords go after `sudo`, since `sudo` clears the environment.  This
creates the database `aqmsdrp_demo` and the login roles `aqmsdb_writer` and
`aqmsdb_reader`.  The backend connects as the writer.

### The first administrator

Every account-management call needs an existing administrator, so the
first one is created directly, as the superuser.  Hash a password the same
way the backend does (libsodium's `crypto_pwhash_str`):

```bash
python3 -c 'import getpass, nacl.pwhash; print(nacl.pwhash.argon2id.str(getpass.getpass().encode()).decode())'
```

Then insert it, with every `$` in the hash escaped so the shell leaves it
alone:

```bash
sudo -u postgres psql -d aqmsdrp_demo \
    -c "SELECT add_user('alice', '\$argon2id\$v=19\$m=65536,t=2,p=1\$...', 'admin');"
```

Every other user can be created from the app once you are logged in.  The
[database README](../scripts/database/README.md) covers permission levels
and provisional accounts.

### Let the container reach PostgreSQL

The container is not on the host's loopback, so PostgreSQL has to listen
somewhere it can reach and accept password logins from there.  In
`postgresql.conf`:

```
listen_addresses = 'localhost,10.0.0.5'   # this server's address
```

and in `pg_hba.conf`:

```
host  aqmsdrp_demo  aqmsdb_writer  10.0.0.0/24  scram-sha-256
```

Restart PostgreSQL afterwards.  If that is more than you want to change for
a trial, skip it and use `--network=host` in step 4 instead.

## 2. Make a signing key

Logins are JSON web tokens signed with an ed25519 key.  Make a pair for
this server; never reuse the ones under `testing/`:

```bash
mkdir -p ~/aqmsdrp/config && cd ~/aqmsdrp/config
openssl genpkey -algorithm ed25519 -out jwt-private.pem
openssl pkey -in jwt-private.pem -pubout -out jwt-public.pem
```

## 3. Write the ini file

Save this as `~/aqmsdrp/config/config.ini` and fill in the AQMS section.
Paths are as the container sees them, under `/config`.

```ini
[General]
verbosity=3
# How far back the catalog reaches.
catalogDurationInDays=7
# Optional: the frontend's map tiles.
#stadiaMapsAPIKey=

[Crow]
# Inside the container.  Which host port reaches it is decided in step 4.
address=0.0.0.0
port=8000
serverName=aqmsdrp-demo
numberOfThreads=2

[Authentication]
jwtTimeToLiveInMinutes=120
jwtSignatureAlgorithm=ed25519
jwtPublicKeyFile=/config/jwt-public.pem
jwtPrivateKeyFile=/config/jwt-private.pem

# The user database from step 1.  Must be writable.
[DRP]
user=aqmsdb_writer
password=pick-one
host=host.containers.internal
database=aqmsdrp_demo
port=5432
isReadOnly=false

# Your AQMS database.  Read-only is the safe choice for a trial; accepting
# or cancelling events needs a user that can write.
[AQMS]
alias=DEMO
user=
password=
host=
database=
port=5432
isReadOnly=true
```

Any `password=` can instead be `passwordFile=/config/<file>`, which keeps
the secret out of the ini.

The container runs as UID 10001, so it has to be able to read these files:

```bash
chmod 755 ~/aqmsdrp/config
chmod 644 ~/aqmsdrp/config/*
```

## 4. Run the container

Images are tagged by version; there is no `latest`.  Pick one from
[quay.io/uuss/aqmsdutyreviewbackend](https://quay.io/repository/uuss/aqmsdutyreviewbackend?tab=tags):

```bash
podman pull quay.io/uuss/aqmsdutyreviewbackend:0.5.4

podman run -d --name aqmsdrp \
    -v ~/aqmsdrp/config:/config:ro,Z \
    -p 127.0.0.1:8090:8000 \
    quay.io/uuss/aqmsdutyreviewbackend:0.5.4
```

The image reads `/config/config.ini` by default.  Publishing on
`127.0.0.1` means only the reverse proxy on this server can reach the
backend.

With Docker, drop the `,Z` and add `--add-host=host.docker.internal:host-gateway`,
and use `host.docker.internal` in the ini.

If you skipped the PostgreSQL changes in step 1, run with
`--network=host` instead of `-p`, and in the ini set `[Crow] address=127.0.0.1`,
`port=8090`, and `[DRP] host=localhost`.

Check it:

```bash
podman logs aqmsdrp
curl -i http://127.0.0.1:8090/                       # 200, empty body
curl -i -u alice http://127.0.0.1:8090/auth/login    # 200 with a token
```

A bad ini stops the container at startup, and `podman logs` names the
setting.

## 5. Put a reverse proxy in front

The frontend calls everything under `/api/`; the proxy strips that prefix
and forwards to the backend.  Build the
[frontend](https://github.com/uofuseismo/aqmsDutyReviewFrontend) and copy
its `dist/` to the web root:

```bash
git clone https://github.com/uofuseismo/aqmsDutyReviewFrontend.git
cd aqmsDutyReviewFrontend
npm ci && npm run build
sudo cp -r dist/. /var/www/aqmsdrp/
```

### nginx

```nginx
server {
    listen 443 ssl;
    server_name drp.example.org;

    ssl_certificate     /etc/ssl/certs/drp.example.org.crt;
    ssl_certificate_key /etc/ssl/private/drp.example.org.key;

    root /var/www/aqmsdrp;

    location / {
        try_files $uri /index.html;
    }

    # The trailing slash on proxy_pass is what strips /api.
    location /api/ {
        proxy_pass http://127.0.0.1:8090/;
        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        # The backend cannot cap request bodies itself; this is the cap.
        client_max_body_size 1m;
    }
}
```

### Apache

Needs `mod_proxy`, `mod_proxy_http`, `mod_ssl`, and `mod_rewrite`:

```apache
<VirtualHost *:443>
    ServerName drp.example.org
    SSLEngine on
    SSLCertificateFile    /etc/ssl/certs/drp.example.org.crt
    SSLCertificateKeyFile /etc/ssl/private/drp.example.org.key

    DocumentRoot /var/www/aqmsdrp

    ProxyPreserveHost On
    ProxyPass        /api/ http://127.0.0.1:8090/
    ProxyPassReverse /api/ http://127.0.0.1:8090/
    # The backend cannot cap request bodies itself; this is the cap.
    <Location /api/>
        LimitRequestBody 1048576
    </Location>

    # Send client-side routes to the app.
    <Directory /var/www/aqmsdrp>
        RewriteEngine On
        RewriteCond %{REQUEST_FILENAME} !-f
        RewriteRule ^ /index.html [L]
    </Directory>
</VirtualHost>
```

Browse to `https://drp.example.org` and log in as the administrator from
step 1.

## Before production

* Set `DROP_EXISTING=0` before running `create.sh` against anything real.
  It defaults to dropping the database it is pointed at.
* Put passwords in `passwordFile=` settings, not in the ini.
* Schedule `SELECT delete_expired_provisional_users();`.  Expired
  provisional accounts cannot log in either way, but nothing removes them
  until something calls it.
* Plan backups of the user database.
