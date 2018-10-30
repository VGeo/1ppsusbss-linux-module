# 1PPSUSBSS linux module
## Synopsis

Linux drivers for 1PPSUSBSS device.

## How to build
### Requirements

- linux headers package
- make

To find needed linux headers package You have to know your linux version.
Call **uname -r**. This command prints actual linux version in your system.

For example: for Debian GNU/Linux 9 (30.10.2018) **uname -r** returns *4.9.0-8-amd64*.
And actual headers package is:

```
$ apt search linux-headers-4.9.0-8-amd64
Сортировка… Готово
Полнотекстовый поиск… Готово
linux-headers-4.9.0-8-amd64/stable,now 4.9.110-3+deb9u6 amd64 [установлен]
  Header files for Linux 4.9.0-8-amd64
```

### Building

Call **make** inside project directory. After successful building You'll get
*1ppsusbss-transmitter.ko* and *1ppsusbss-receiver.ko* linux modules.

## How to use
### Transmitter

Call **insmod 1ppsusbss-transmitter.ko** and connect the transmitter device.
Transmitter will start generate signal in four seconds.

### Receiver

You have to enable linux PPS subsystem at first. Call **modprobe pps-core**.
Now You are able to load the receiver module: **insmod 1ppsusbss-receiver.ko**
and connect the receiver device.

Now You are able to go to */sys/class/pps/ppsX* directory (where X is a number
like 0 or 1 or 2, etc) and read _assert_ and _clear_ files.
This files contain rising and falling signal timestamps.

You cat watch them with command like:

```
watch -n 0.1 cat assert
```

