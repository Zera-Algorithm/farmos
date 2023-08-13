./looper 20 ./multi.sh 1 | ./busybox grep -o "COUNT|[[:digit:]]\+|" | ./busybox grep -o "[[:digit:]]\+" | ./busybox awk '{print "Unixbench SHELL1 test(lpm): "$0}'
./looper 20 ./multi.sh 8 | ./busybox grep -o "COUNT|[[:digit:]]\+|" | ./busybox grep -o "[[:digit:]]\+" | ./busybox awk '{print "Unixbench SHELL8 test(lpm): "$0}'
./looper 20 ./multi.sh 16 | ./busybox grep -o "COUNT|[[:digit:]]\+|" | ./busybox grep -o "[[:digit:]]\+" | ./busybox awk '{print "Unixbench SHELL16 test(lpm): "$0}'
