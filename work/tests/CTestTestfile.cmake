# CMake generated Testfile for 
# Source directory: /home/suhjw/gnss-sdr/tests
# Build directory: /home/suhjw/gnss-sdr/work/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[flowgraph_test]=] "flowgraph_test")
set_tests_properties([=[flowgraph_test]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1048;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
add_test([=[gnss_block_test]=] "gnss_block_test")
set_tests_properties([=[gnss_block_test]=] PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1110;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
add_test([=[gnuradio_block_test]=] "gnuradio_block_test")
set_tests_properties([=[gnuradio_block_test]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1154;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
add_test([=[matio_test]=] "matio_test")
set_tests_properties([=[matio_test]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1196;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
add_test([=[acq_test]=] "acq_test")
set_tests_properties([=[acq_test]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1252;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
add_test([=[trk_test]=] "trk_test")
set_tests_properties([=[trk_test]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1355;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
add_test([=[control_thread_test]=] "control_thread_test")
set_tests_properties([=[control_thread_test]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/suhjw/gnss-sdr/tests/CMakeLists.txt;1393;add_test;/home/suhjw/gnss-sdr/tests/CMakeLists.txt;0;")
subdirs("unit-tests/signal-processing-blocks/libs")
subdirs("system-tests/libs")
