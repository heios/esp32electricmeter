# esp32electricmeter
Collect energy consumption from electeric meter by photoresistor.

## Board
Developed with board ESP-WROOM-32. In Arduino studio worked with "ESP32-WROOM-DA".

## Test UDP protocol:

```
$ echo "status" | nc -4u -w1 192.168.0.123 4567
loops: 8274, logs: 8271, photoresistor pin: 34, polling freq: 32, start time: 2023-05-07T20:04:07.280136, last time: 2023-05-07T20:08:23.655476, last luminocity: 1756
```

```
$ echo | nc -4u -w1 192.168.0.123 4567 | hexdump
0000000 a690 4316 fb1f 0005 06d0 1fa8 4317 fb1f
0000010 0005 06d2 98c0 4317 fb1f 0005 06d3 11d8
0000020 4318 fb1f 0005 06d2 8af0 4318 fb1f 0005
0000030 06d5 0408 4319 fb1f 0005 06d0 7d20 4319
...
```