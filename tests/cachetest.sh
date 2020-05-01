#!/bin/bash
echo Cache test with multiple clients to rigctld
echo Need two rigctld running
echo rigctld -t 4532 --set-conf=cache_timeout=0
echo rigctld -t 4533
rigctlOK4532=`ps ax | grep rigctld | grep 4532`
rigctlOK4533=`ps ax | grep rigctld | grep 4533`
echo 4532 $rigctlOK4532
echo 4533 $rigctlOK4533
if [ -z "$rigctlOK4532"  ];then
echo  rigctld port 4532  is not running
exit 1
fi
if [ -z "$rigctlOK4533"  ];then
echo  rigctld port 4533  is not running
exit 1
fi
for cachetime in 0 500;do
echo ===============================================
echo rigctl cachetimeout=$cachetime
for port in 4532 4533;do
if [ $port == "4532" ];then
echo rigctld cachetimeout=0
cache="no cache"
fi
if [ $port == "4533" ];then
echo ===============================================
echo rigctld cachetimeout=500
cache="500ms cache"
fi
echo With one process, $cache
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2 >&1|egrep "Elapsed"
echo With two processes, $cache
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2 >&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"

echo With ten processes, $cache
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"&
./cachetest 2 127.0.0.1:$port 19200 12 $cachetime 2>&1|egrep "Elapsed"
wait

done
done
