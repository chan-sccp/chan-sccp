# Installing chan_sccp

Instructions for building and installing the chan_sccp channel driver for Asterisk from the [chan-sccp/chan-sccp](https://github.com/chan-sccp/chan-sccp) repository.

## Requirements

- **OS:** Debian 11/12 or compatible (Ubuntu, etc.)
- **Asterisk:** 16+ (21–23 recommended), installed with development packages
- **chan_skinny** module must be disabled in Asterisk (conflicts with chan_sccp)

## Step 1. Dependencies

```bash
apt-get update
apt-get install -y build-essential gettext libssl-dev asterisk-devel
```

For **libxml2-dev** and **libxslt1-dev**:
- On Debian/Ubuntu, if versions from the main repositories are suitable:
  ```bash
  apt-get install -y libxml2-dev libxslt1-dev
  ```
- If you run into version conflicts (e.g. system on trixie, bookworm repositories), add the repository with the required version and install:
  ```bash
  echo 'deb http://deb.debian.org/debian/ trixie main' > /etc/apt/sources.list.d/trixie.list
  apt-get update
  apt-get install -y -t trixie libxml2-dev libxslt1-dev
  ```

For **Asterisk 23** (Sangoma/FreePBX):
```bash
apt-get install -y asterisk23-devel
```

## Step 2. Disable chan_skinny

In `/etc/asterisk/modules.conf`, in the `[modules]` section, add or verify:

```ini
noload = chan_skinny.so
```

Reload Asterisk:
```bash
asterisk -rx "module reload"
```

## Step 3. Clone and build

```bash
cd /usr/src
git clone --depth 1 https://github.com/chan-sccp/chan-sccp.git
cd chan-sccp
```

Configure with recommended options (conference, advanced functions, distributed devicestate, video):

```bash
./configure \
  --enable-conference \
  --enable-advanced-functions \
  --enable-distributed-devicestate \
  --enable-video
```

Build and install:

```bash
make -j$(nproc)
make install
make reload
```

## Step 4. Verify

```bash
asterisk -rx "module show like chan_sccp"
```

You should see a line with `chan_sccp.so` and status “Loaded”.

Configuration: `/etc/asterisk/sccp.conf`. Documentation: `/var/lib/asterisk/documentation/thirdparty/`.

## TFTP templates for phones

To provision Cisco SCCP phones, copy the templates to the TFTP directory:

```bash
mkdir -p /tftpboot/templates
cp /usr/src/chan-sccp/conf/tftp/*.xml* /tftpboot/templates/
chown -R asterisk:asterisk /tftpboot
```

## Rollback

To remove the module:
```bash
rm -f /lib/x86_64-linux-gnu/asterisk/modules/chan_sccp.so
# or the path from configure output: "Module Directory"
asterisk -rx "module reload"
```

Re-enable `chan_skinny.so` in `modules.conf` if you need the stock Skinny driver.
