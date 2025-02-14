set -xe

source_list="main.cpp"
library_list="-lntdll -lws2_32"

CPPC=i686-w64-mingw32-c++
$CPPC -g -static -shared -o memory_goober.asi $source_list $library_list
$CPPC -g -static -o goober_tester.exe tester.cpp $library_list

CPPC=x86_64-w64-mingw32-c++
$CPPC -g -static -shared -o memory_goober64.asi $source_list $library_list
$CPPC -g -static -o goober_tester64.exe tester.cpp $library_list
