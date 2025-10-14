#!/bin/bash
# start_synced.sh

echo "=== Synchronizing to PPS ==="

# GPS 시간 가져오기
GPS_TIME=$(uhd_usrp_probe --args="addr=192.168.40.2" --sensor="gps_time" 2>/dev/null | grep "GPS epoch time" | awk '{print $4}')

# 2초 후로 설정
NEXT_TIME=$((GPS_TIME + 2))

echo "GPS time: $GPS_TIME"
echo "Setting time to $NEXT_TIME at next PPS..."

# UHD를 사용해 시간 설정 (C++ 프로그램 필요)
# 아래 C++ 프로그램 컴파일 후 실행
./sync_usrp_time 192.168.40.2 $NEXT_TIME

echo ""
echo "=== Starting GNSS-SDR ==="
gnss-sdr -c x310.conf