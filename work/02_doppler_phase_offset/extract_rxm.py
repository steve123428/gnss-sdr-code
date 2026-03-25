import csv
import os
import argparse
import math
from pyubx2 import UBXReader

# 데이터 분석 및 시각화를 위한 라이브러리 (cmc 옵션일 때만 사용)
try:
    import pandas as pd
    import matplotlib.pyplot as plt
except ImportError:
    pd = None
    plt = None

# L1 주파수 파장 (User defined constant)
WAVELENGTH_L1 = 0.190293672798365

def parse_ubx_to_csv(ubx_file_path, rawx_csv_path, measx_csv_path):
    """UBX 파일을 읽어 RAWX와 MEASX 메시지를 CSV로 저장"""
    
    # RXM-RAWX 헤더 정의
    rawx_headers = [
        'rcvTow', 'week', 'leapS', 'numMeas', 
        'gnssId', 'svId', 'prMes', 'cpMes', 'doMes', 'cno', 'locktime', 
        'prStd', 'cpStd', 'doStd', 
        'prValid', 'cpValid', 'halfCyc', 'subHalfCyc'
    ]

    # RXM-MEASX 헤더 정의
    measx_headers = [
        'towMS', 'numSv', 'msgId', 
        'gnssId', 'svId', 'cno', 'mpathIndic', 'dopplerMS', 'dopplerHz', 
        'prMes', 'prRMS', 'mesQI'
    ]

    print(f"[1/2] UBX 파싱 시작: {ubx_file_path}...")

    with open(rawx_csv_path, 'w', newline='') as f_rawx, \
         open(measx_csv_path, 'w', newline='') as f_measx, \
         open(ubx_file_path, 'rb') as f_ubx:

        writer_rawx = csv.DictWriter(f_rawx, fieldnames=rawx_headers)
        writer_measx = csv.DictWriter(f_measx, fieldnames=measx_headers)

        writer_rawx.writeheader()
        writer_measx.writeheader()

        ubr = UBXReader(f_ubx)
        
        count_rawx = 0
        count_measx = 0

        for (raw_data, parsed_data) in ubr:
            try:
                msg_id = parsed_data.identity
                
                if msg_id == 'RXM-RAWX':
                    common_data = {
                        'rcvTow': getattr(parsed_data, 'rcvTow', ''),
                        'week': getattr(parsed_data, 'week', ''),
                        'leapS': getattr(parsed_data, 'leapS', ''),
                        'numMeas': getattr(parsed_data, 'numMeas', 0)
                    }
                    
                    for i in range(1, common_data['numMeas'] + 1):
                        row = common_data.copy()
                        idx_str = f"{i:02d}"
                        
                        # 필드 매핑
                        for field in rawx_headers[4:]: # gnssId부터 끝까지
                            # CSV 헤더 이름과 pyubx2 속성 이름 매칭 (헤더 이름 + _xx)
                            attr_name = f"{field}_{idx_str}"
                            val = getattr(parsed_data, attr_name, '')
                            row[field] = val
                            
                        writer_rawx.writerow(row)
                    count_rawx += 1

                elif msg_id == 'RXM-MEASX':
                    common_data = {
                        'towMS': getattr(parsed_data, 'towMS', ''),
                        'numSv': getattr(parsed_data, 'numSv', 0),
                        'msgId': getattr(parsed_data, 'msgId', '')
                    }
                    
                    for i in range(1, common_data['numSv'] + 1):
                        row = common_data.copy()
                        idx_str = f"{i:02d}"

                        for field in measx_headers[3:]:
                             attr_name = f"{field}_{idx_str}"
                             val = getattr(parsed_data, attr_name, '')
                             row[field] = val

                        writer_measx.writerow(row)
                    count_measx += 1

            except Exception as e:
                continue

    print(f" -> 파싱 완료. (RAWX Epochs: {count_rawx}, MEASX Epochs: {count_measx})")
    print(f" -> 파일 저장: {rawx_csv_path}, {measx_csv_path}")

def analyze_cmc(rawx_csv_path):
    """RAWX CSV 데이터를 읽어 위성별 CMC 계산 및 시각화"""
    if pd is None or plt is None:
        print("오류: pandas와 matplotlib가 설치되지 않아 CMC 분석을 수행할 수 없습니다.")
        return

    print(f"\n[2/2] CMC 분석 시작: {rawx_csv_path}...")
    
    # CSV 읽기
    df = pd.read_csv(rawx_csv_path)

    # 데이터 유효성 검사 (prMes, cpMes가 0이거나 비어있는 경우 제외, 필요 시 valid 플래그 확인 추가 가능)
    df = df[(df['prMes'] > 0) & (df['cpMes'] != 0)].copy()

    # 1. & 2. CMC 계산 (CMC = prMes - cpMes * lambda)
    df['cmc'] = df['prMes'] - (df['cpMes'] * WAVELENGTH_L1)

    # 위성 식별자 생성 (예: GPS_01) - gnssId는 숫자일 수도 있고 문자일 수도 있음
    # pyubx2는 gnssId를 문자로 주기도 하므로 문자열 변환
    df['sat_id'] = df['gnssId'].astype(str) + "_" + df['svId'].astype(str)
    
    # 3. 위성별 통계 계산
    unique_sats = df['sat_id'].unique()
    unique_sats.sort()
    
    print("\n" + "="*50)
    print(f"{'Satellite':<15} | {'Mean (m)':<15} | {'Variance':<15} | {'Std Dev (m)':<15}")
    print("-" * 66)

    stats_list = []
    
    for sat in unique_sats:
        sat_data = df[df['sat_id'] == sat]['cmc']
        
        # 통계 계산
        mean_val = sat_data.mean()
        var_val = sat_data.var()
        std_val = sat_data.std()
        
        print(f"{sat:<15} | {mean_val:<15.4f} | {var_val:<15.4f} | {std_val:<15.4f}")
        
        stats_list.append({
            'sat': sat,
            'data': df[df['sat_id'] == sat],
            'mean': mean_val
        })

    print("="*50)

    # 4. Subplot 그리기
    num_sats = len(unique_sats)
    if num_sats == 0:
        print("유효한 위성 데이터가 없습니다.")
        return

    # Subplot 격자 크기 계산 (예: 12개면 4x3)
    cols = 4
    rows = math.ceil(num_sats / cols)
    
    fig, axes = plt.subplots(rows, cols, figsize=(16, 3 * rows), constrained_layout=True)
    fig.suptitle(f'CMC (Code Minus Carrier) per Satellite\nLambda = {WAVELENGTH_L1}', fontsize=16)
    
    # axes가 1차원 배열이거나 단일 객체일 경우 처리
    if num_sats == 1:
        axes = [axes]
    elif rows > 1:
        axes = axes.flatten()

    for i, sat_info in enumerate(stats_list):
        ax = axes[i]
        sat_name = sat_info['sat']
        data = sat_info['data']
        
        # CMC 그래프 (시간에 따른 변화를 보기 위해 rcvTow 사용, 상대 시간으로 변경)
        # rcvTow가 크므로 첫 번째 시간 기준 0초부터 시작하도록 조정
        rel_time = data['rcvTow'] - data['rcvTow'].iloc[0]
        
        # 보기 편하게 평균을 뺀 값(Centered CMC)을 그릴 수도 있지만, 
        # 요청사항은 "cmc 값" 출력이므로 원본(또는 평균 중심) 중 선택. 
        # 여기서는 변동성을 보기 위해 (CMC - Mean)을 그리는 것이 일반적이나
        # 요청대로 순수 CMC 값을 그리면 오프셋이 커서 보기가 힘들 수 있습니다.
        # 일단 그대로 그립니다.
        ax.plot(rel_time, data['cmc'], label='CMC', linewidth=0.8)
        
        ax.set_title(f"Sat: {sat_name}")
        ax.set_xlabel("Time (sec)")
        ax.set_ylabel("CMC (m)")
        ax.grid(True, linestyle='--', alpha=0.6)
        
        # 통계 텍스트 추가 (그래프 구석)
        stats_text = f"Std: {sat_info['data']['cmc'].std():.3f}m"
        ax.text(0.05, 0.9, stats_text, transform=ax.transAxes, 
                bbox=dict(facecolor='white', alpha=0.8))

    # 남은 빈 subplot 끄기
    for j in range(i + 1, len(axes)):
        axes[j].axis('off')

    plt.show()

# --- 실행부 ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="UBX Parser & CMC Analyzer")
    parser.add_argument("input_file", help="Input .ubx file path")
    parser.add_argument("-cmc", "--calculate_cmc", action="store_true", help="Calculate and plot CMC statistics")
    
    args = parser.parse_args()

    input_file = args.input_file
    base_name = os.path.splitext(input_file)[0]
    output_rawx = f"{base_name}_rawx.csv"
    output_measx = f"{base_name}_measx.csv"
    
    if os.path.exists(input_file):
        # 1. 파싱 수행
        parse_ubx_to_csv(input_file, output_rawx, output_measx)
        
        # 2. CMC 옵션이 켜져있으면 분석 수행
        if args.calculate_cmc:
            analyze_cmc(output_rawx)
    else:
        print(f"오류: '{input_file}' 파일을 찾을 수 없습니다.")