# chan-sccp

Этот репозиторий содержит рабочую сборку драйвера `chan-sccp` для телефонов Cisco SCCP в Asterisk.

**Рабочий драйвер:** [timspb/chan-sccp](https://github.com/timspb/chan-sccp)

Это короткий README для форка: только сборка, установка и базовое использование.

[English](README.md) | Русский

Проверенный диапазон для этого форка:

- FreePBX 16 или 17
- Asterisk 21 / 22 / 23

## Для чего этот проект

- Поддержка телефонов Cisco SCCP/Skinny в Asterisk
- Регистрация устройств и обработка вызовов
- Подготовка телефонов SCCP к работе

## Что нужно

- Сборка Asterisk с заголовками и файлами разработки
- Инструменты сборки: `gcc`, `make`, `autoconf`, `libtool`
- Библиотеки для XML и XSLT
- Библиотеки разработки OpenSSL
- `gettext`

Если в системе есть пакетный менеджер, сначала установите обычные dev-пакеты. Точные названия зависят от дистрибутива.

Пример для Debian / Ubuntu:

```bash
sudo apt update && sudo apt install -y build-essential git autoconf libtool pkg-config gettext \
  libxml2-dev libxslt1-dev libssl-dev libjansson-dev libsqlite3-dev
```

## Сборка из исходников

```bash
git clone https://github.com/timspb/chan-sccp.git
cd chan-sccp
./tools/bootstrap.sh
./configure
make -j2
make install
make reload
```

Если вы обновляете уже существующую копию:

```bash
git pull
./tools/bootstrap.sh
./configure
make -j2
make install
make reload
```

## Примечания

- Этот форк рассчитан на практическое использование и активную разработку.
- README специально короткий, чтобы пользователь быстро дошёл до сборки и установки.
- Оригинальный upstream-проект остаётся каноническим источником `chan-sccp`.

## Если что-то не работает

- Если `./configure` падает, проверьте установку dev-пакетов.
- Если сборка не находит заголовки Asterisk, установите matching Asterisk dev package для вашей версии.
- Если драйвер не загружается, сначала смотрите логи Asterisk и проверку модулей.
