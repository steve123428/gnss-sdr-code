#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
GNSS-SDR Config Generator and Runner

기능:
    1. RF ID와 PRN을 지정하여 CHANNELS CONFIG 블록 생성/교체
    2. Tracking dump_filename 경로 자동 업데이트
    3. GNSS-SDR 실행

Usage:
    python3 gnss_sdr_runner.py --config my.conf --rf 0 1 --prn 3 5 7 10
    python3 gnss_sdr_runner.py --config my.conf --rf 0 1 2 --prn 3 5 --run
"""

import datetime
import os
import subprocess
import argparse
import re
import signal
import sys
import time


# 전역 변수
gnss_process = None
shutdown_requested = False


def signal_handler(sig, frame):
    """Ctrl+C 시그널 핸들러"""
    global gnss_process, shutdown_requested
    
    if shutdown_requested:
        # 두 번째 Ctrl+C: 강제 종료
        print("\nForce killing GNSS-SDR...")
        if gnss_process:
            gnss_process.kill()
        sys.exit(1)
    
    shutdown_requested = True
    
    if gnss_process is not None:
        print("\n\nShutting down GNSS-SDR gracefully...")
        print("(Press Ctrl+C again to force kill)\n")
        
        # 방법 1: 'q' + Enter 전송 시도
        try:
            gnss_process.stdin.write(b'q\n')
            gnss_process.stdin.flush()
            print("Sent 'q' command to GNSS-SDR")
        except:
            pass
        
        # 잠시 대기 후 아직 실행 중이면 SIGTERM 전송
        time.sleep(600)
        if gnss_process.poll() is None:
            print("Sending SIGTERM...")
            gnss_process.terminate()
        
        # 추가 대기 후에도 실행 중이면 SIGINT 전송
        time.sleep(30)
        if gnss_process.poll() is None:
            print("Sending SIGINT...")
            gnss_process.send_signal(signal.SIGINT)
    else:
        sys.exit(0)


def generate_tracking_directory():
    """현재 시간으로 디렉토리 이름 생성"""
    current_time = datetime.datetime.now()
    directory_name = current_time.strftime("tracking_%y%m%d_%H%M")
    return directory_name


def generate_log_paths():
    """로그 파일 경로 생성"""
    current_time = datetime.datetime.now()
    base_path = current_time.strftime("./log/%y%m%d_%H%M")
    
    if not os.path.exists(base_path):
        os.makedirs(base_path)

    log_paths = {
        # "acquisition": os.path.join(base_path, "acq", "acq_dump.dat"),
        #"tracking": os.path.join(base_path, "tracking", "tracking_ch"),
        "tracking": os.path.join(base_path, "tracking_ch"),
        "telemetry": os.path.join(base_path, "telemetry_1C"),
        "observables": os.path.join(base_path, "observables"),
        "pvt": os.path.join(base_path, "pvt")
    }
    
    return log_paths


def generate_channels_block(rf_ids, prns, signal="1C"):
    """CHANNELS CONFIG 블록 생성"""
    total_channels = len(prns) * len(rf_ids)

    out = []
    out.append("############################################################")
    out.append("# CHANNELS CONFIG")
    out.append("############################################################")
    out.append(f"Channels_{signal}.count={total_channels}")
    out.append("Channels.in_acquisition=1")
    out.append("")

    ch = 0
    for prn in prns:
        for rf_id in rf_ids:
            out.append(f"Channel{ch}.signal={signal}")
            out.append(f"Channel{ch}.satellite={prn}")
            out.append(f"Channel{ch}.RF_channel_ID={rf_id}")
            out.append("")
            ch += 1

    return "\n".join(out)


def update_tracking_dump_filename(content, log_paths, signal="1C"):
    """Tracking dump_filename 업데이트"""
    
    # 패턴: Tracking_1C.dump_filename=... (또는 다른 signal)
    pattern = rf'(Tracking_{signal}\.dump_filename\s*=\s*).*'
    replacement = rf'\g<1>{log_paths["tracking"]}'
    
    if re.search(pattern, content):
        content = re.sub(pattern, replacement, content)
        print(f"  - Tracking dump_filename updated: {log_paths['tracking']}")
    else:
        print(f"  - Warning: Tracking_{signal}.dump_filename not found in config")

    # 패턴: TelemetryDecoder_1C.dump_filename=... (또는 다른 signal)
    pattern = rf'(TelemetryDecoder_{signal}\.dump_filename\s*=\s*).*'
    replacement = rf'\g<1>{log_paths["telemetry"]}'
    
    if re.search(pattern, content):
        content = re.sub(pattern, replacement, content)
        print(f"  - Telemetry dump_filename updated: {log_paths['telemetry']}")
    else:
        print(f"  - Warning: TelemetryDecoder_{signal}.dump_filename not found in config")

    # 패턴: Observables_{signal}.dump_filename=... (또는 다른 signal)
    pattern = rf'(Observables\.dump_filename\s*=\s*).*'
    replacement = rf'\g<1>{log_paths["observables"]}'
    
    if re.search(pattern, content): 
        content = re.sub(pattern, replacement, content)
        print(f"  - Observables dump_filename updated: {log_paths['observables']}")
    else:
        print(f"  - Warning: Observables_{signal}.dump_filename not found in config")

    # 패턴: PVT.dump_filename=...
    pattern = r'(PVT\.dump_filename\s*=\s*).*'
    replacement = rf'\g<1>{log_paths["pvt"]}'

    if re.search(pattern, content):
        content = re.sub(pattern, replacement, content)
        print(f"  - PVT dump_filename updated: {log_paths['pvt']}")
    else:
        print(f"  - Warning: PVT.dump_filename not found in config")
    
    return content


def update_channels_block(content, rf_ids, prns, signal="1C"):
    """CHANNELS CONFIG 블록 교체 (없으면 추가)"""
    
    new_channels_block = generate_channels_block(rf_ids, prns, signal)
    
    # 기존 CHANNELS CONFIG 블록 찾기
    pattern = r'#{10,}\s*\n# CHANNELS CONFIG\s*\n#{10,}\s*\n.*?(?=#{10,}|\Z)'
    
    if re.search(pattern, content, re.DOTALL):
        content = re.sub(pattern, new_channels_block + "\n\n", content, flags=re.DOTALL)
        print("  - CHANNELS CONFIG block replaced")
    else:
        content = content.rstrip() + "\n\n" + new_channels_block + "\n"
        print("  - CHANNELS CONFIG block added")
    
    return content


def update_config_file(config_path, rf_ids, prns, signal="1C"):
    """Config 파일 업데이트 (CHANNELS 블록 + Tracking 경로)"""
    
    # 파일 읽기
    with open(config_path, 'r') as f:
        content = f.read()
    
    # 로그 경로 생성
    log_paths = generate_log_paths()
    
    print(f"Updating config: {config_path}")
    print(f"  - RF IDs: {rf_ids}")
    print(f"  - PRNs: {prns}")
    print(f"  - Total channels: {len(rf_ids) * len(prns)}")
    
    # 1. Tracking dump_filename 업데이트
    content = update_tracking_dump_filename(content, log_paths, signal)
    
    # 2. CHANNELS 블록 교체/추가
    content = update_channels_block(content, rf_ids, prns, signal)
    
    # 파일 쓰기
    with open(config_path, 'w') as f:
        f.write(content)
    
    print(f"  - Log directory: {os.path.dirname(log_paths['tracking'])}")
    
    return log_paths


def run_gnss_sdr(config_file):
    """GNSS-SDR 실행 (graceful shutdown 지원)"""
    global gnss_process, shutdown_requested
    
    shutdown_requested = False
    
    # 시그널 핸들러 등록
    original_sigint = signal.signal(signal.SIGINT, signal_handler)
    
    try:
        print(f"\nRunning GNSS-SDR with config: {config_file}")
        print("Press Ctrl+C to stop GNSS-SDR gracefully...")
        print("=" * 50 + "\n")
        
        # Popen으로 프로세스 시작 (stdin 파이프 연결)
        gnss_process = subprocess.Popen(
            ['gnss-sdr', f'--config_file={config_file}'],
            stdin=subprocess.PIPE,
            # 프로세스 그룹 분리하지 않음 (시그널 전달을 위해)
            preexec_fn=None
        )
        
        # 프로세스 종료 대기
        return_code = gnss_process.wait()
        
        print("\n" + "=" * 50)
        if return_code == 0:
            print("GNSS-SDR completed successfully.")
        elif return_code == -2:
            print("GNSS-SDR terminated by SIGINT.")
        elif return_code == -15:
            print("GNSS-SDR terminated by SIGTERM.")
        else:
            print(f"GNSS-SDR exited with code: {return_code}")
        
        # 종료 후 로그 변환 대기
        if shutdown_requested:
            print("Waiting for log conversion...")
            time.sleep(3)
            
    except FileNotFoundError:
        print("Error: gnss-sdr not found. Please make sure GNSS-SDR is installed.")
    finally:
        gnss_process = None
        signal.signal(signal.SIGINT, original_sigint)


def main():
    parser = argparse.ArgumentParser(
        description="GNSS-SDR Config Generator and Runner"
    )
    parser.add_argument(
        "--config",
        type=str,
        default="base.conf",
        help="Config file path (default: base.conf)"
    )
    parser.add_argument(
        "--rf",
        type=int,
        nargs="+",
        required=True,
        help="RF channel IDs (e.g., --rf 0 1 2)"
    )
    parser.add_argument(
        "--prn",
        type=int,
        nargs="+",
        required=True,
        help="PRN numbers (e.g., --prn 3 5 7 10)"
    )
    parser.add_argument(
        "--signal",
        type=str,
        default="1C",
        help="Signal type (default: 1C)"
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Run GNSS-SDR after updating config"
    )
    parser.add_argument(
        "--print-only",
        action="store_true",
        help="Only print the generated channels block"
    )

    args = parser.parse_args()

    # print-only 모드
    if args.print_only:
        print(generate_channels_block(args.rf, args.prn, args.signal))
        return

    # Config 파일 업데이트
    update_config_file(args.config, args.rf, args.prn, args.signal)

    # GNSS-SDR 실행
    if args.run:
        run_gnss_sdr(args.config)


if __name__ == "__main__":
    main()