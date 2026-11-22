Просто добавить fork2join и оставить последовательный partition, тоже дает почти x3 ([коммит](https://github.com/stewkk/itmo-parallel-algos-quicksort/tree/c0b34416733c0edc7cddc27f1ad213cc6b690c94)):
``` 
[ RUN      ] QuicksortBenchTest.Main
Run 1: sequential 14352.6 ms, parallel 5896.59 ms
Run 2: sequential 14316.4 ms, parallel 5024.02 ms
Run 3: sequential 14278.9 ms, parallel 5092.24 ms
Run 4: sequential 14238.2 ms, parallel 4681.55 ms
Run 5: sequential 14255.3 ms, parallel 4818.24 ms
Average: sequential 14288.3 ms, parallel 5102.53 ms
[       OK ] QuicksortBenchTest.Main (100985 ms)
```

Версия с master:
``` 
[ RUN      ] QuicksortBenchTest.Main
Run 1: sequential 11237.1 ms, parallel 4062.01 ms
Run 2: sequential 11031.7 ms, parallel 4089.56 ms
Run 3: sequential 10876.7 ms, parallel 3890.3 ms
Run 4: sequential 10896 ms, parallel 4028.72 ms
Run 5: sequential 10944.6 ms, parallel 3861.83 ms
Average: sequential 10997.2 ms, parallel 3986.49 ms
``` 
