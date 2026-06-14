# Установка chan_sccp

Инструкция по сборке и установке канального драйвера chan_sccp для Asterisk из репозитория [chan-sccp/chan-sccp](https://github.com/chan-sccp/chan-sccp).

## Требования

- **ОС:** Debian 11/12 или совместимая (Ubuntu и т.п.)
- **Asterisk:** 16+ (рекомендуется 21–23), установленный с пакетами разработки
- **Модуль chan_skinny** должен быть отключён в Asterisk (конфликт с chan_sccp)

## Шаг 1. Зависимости

```bash
apt-get update
apt-get install -y build-essential gettext libssl-dev asterisk-devel
```

Для **libxml2-dev** и **libxslt1-dev**:
- На Debian/Ubuntu, если версии из основных репозиториев подходят:
  ```bash
  apt-get install -y libxml2-dev libxslt1-dev
  ```
- Если возникает конфликт версий (например, система на trixie, репозитории bookworm), добавьте репозиторий с нужной версией и установите:
  ```bash
  echo 'deb http://deb.debian.org/debian/ trixie main' > /etc/apt/sources.list.d/trixie.list
  apt-get update
  apt-get install -y -t trixie libxml2-dev libxslt1-dev
  ```

Для **Asterisk 23** (Sangoma/FreePBX):
```bash
apt-get install -y asterisk23-devel
```

## Шаг 2. Отключение chan_skinny

В `/etc/asterisk/modules.conf` в секции `[modules]` добавьте или проверьте строку:

```ini
noload = chan_skinny.so
```

Перезагрузите Asterisk:
```bash
asterisk -rx "module reload"
```

## Шаг 3. Клонирование и сборка

```bash
cd /usr/src
git clone --depth 1 https://github.com/chan-sccp/chan-sccp.git
cd chan-sccp
```

Конфигурация с рекомендуемыми опциями (конференции, расширенные функции, распределённый devicestate, видео):

```bash
./configure \
  --enable-conference \
  --enable-advanced-functions \
  --enable-distributed-devicestate \
  --enable-video
```

Сборка и установка:

```bash
make -j$(nproc)
make install
make reload
```

## Шаг 4. Проверка

```bash
asterisk -rx "module show like chan_sccp"
```

Должна быть строка с `chan_sccp.so` и статусом «Loaded».

Конфигурация: `/etc/asterisk/sccp.conf`. Документация: `/var/lib/asterisk/documentation/thirdparty/`.

## Шаблоны TFTP для телефонов

Для provisioning Cisco SCCP-телефонов скопируйте шаблоны в каталог TFTP:

```bash
mkdir -p /tftpboot/templates
cp /usr/src/chan-sccp/conf/tftp/*.xml* /tftpboot/templates/
chown -R asterisk:asterisk /tftpboot
```

## Откат

Удаление модуля:
```bash
rm -f /lib/x86_64-linux-gnu/asterisk/modules/chan_sccp.so
# или путь из вывода configure: "Module Directory"
asterisk -rx "module reload"
```

Включите снова `chan_skinny.so` в `modules.conf`, если нужен штатный Skinny.
