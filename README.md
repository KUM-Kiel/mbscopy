# mbscopy

This little tool should be able to extract data from MBS flashcards, even when the filesystem has been corrupted by buggy Send software or firmware.
It searches for typical MBS data patterns on the device and retrieves them.

## Usage

If the flashcard is available as device `/dev/sdb`, one can run *mbscopy* as follows.

```text
$ mbscopy /dev/sdb
```

To find out the device name, one can use the following command on Linux.

```text
$ tail -f /var/log/syslog
```

Or on systems with *systemd* installed, like Fedora or ArchLinux:

```text
$ dmesg -w
```

When the device is plugged in, a message with the device path should appear.

On Mac OS X it is possible to use the `Disk Utility.app` (in `/Applications/Utilities`).
The device path should look like `/dev/disk2`.

It is advisable not to mount any MBS cards and especially not try to repair or check the filesystem.
This will make things worse and might destroy data.

## Building

If you have a C compiler installed you can easily build `mbscopy` with the following steps.

```text
$ cd mbscopy/
$ make
```

If you do not have a compiler, you can install one on Ubuntu, Debian or Linux Mint with the following command. 

```text
$ sudo apt-get install build-essential
```

If you are using Mac OS X, use this command instead.

```text
$ xcode-select --install
```

## License

GPLv3
