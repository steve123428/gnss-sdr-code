import h5py
import numpy as np
import pandas as pd
import os, sys

import h5py
import numpy as np
import pandas as pd
import os

L1_LAMBDA_M = 0.190293672798365  # [m/cycle] GPS L1 carrier wavelength

def analyze_and_align_signals(file_path):
    # 1. 파일 로드
    if not os.path.exists(file_path):
        print(f"Error: File '{file_path}' not found.")
        return

    print(f"--- Processing: {file_path} ---")
    
    with h5py.File(file_path, 'r') as f:
        # 데이터셋 읽기 (Shape: Epochs x Channels)
        # 키 이름은 실제 파일 구조에 맞게 조정 (대소문자 주의)
        keys_map = {
            'utc_millis': 'utc_millis', # 또는 'RX_time', 'TOW...' 확인 필요
            'PRN': 'PRN',
            'Pseudorange': 'Pseudorange_m',
            'Doppler': 'Carrier_Doppler_hz',
            'Phase': 'Carrier_phase_cycles',
            'TOW': 'TOW_at_current_symbol_s'
        }
        
        # 데이터 추출 (Flatten하여 1차원 배열로 변환)
        # 예: (27000, 20) -> (540000,)
        try:
            raw_data = {}
            for k, v in keys_map.items():
                if v in f:
                    # h5py는 Transpose되어 읽힐 수 있으므로, .T 없이 읽거나 확인 필요.
                    # 여기서는 Flatten하므로 순서만 맞으면 됨.
                    # (Epoch가 행, Channel이 열이라고 가정)
                    raw_data[k] = np.array(f[v]).flatten()
                else:
                    print(f"Warning: Key '{v}' not found in mat file.")
                    return
            
            # Channel Index 생성 (0~19, 0~19, ...)
            num_samples = len(raw_data['PRN'])
            # Assuming original shape was (N, M), but flattened structure preserves pairing
            # We can use the flatten index as a unique ID for the row initially
            
        except Exception as e:
            print(f"Error reading file structure: {e}")
            return

    # DataFrame 생성
    df = pd.DataFrame(raw_data)
    
    # ---------------------------------------------------------
    # Step 1: PRN이 0인 데이터 제거
    # ---------------------------------------------------------
    original_len = len(df)
    df = df[df['PRN'] > 0]
    print(f"Step 1: Removed PRN 0. ({original_len} -> {len(df)} rows)")

    # ---------------------------------------------------------
    # Step 2: PRN이 중복되지 않는 데이터 제거 (Pair가 없는 경우)
    # ---------------------------------------------------------
    # 같은 시간(utc_millis)에 같은 PRN이 2개 이상 있어야 함
    # group size 계산
    counts = df.groupby(['utc_millis', 'PRN'])['PRN'].transform('count')
    df_pairs = df[counts >= 2].copy()
    
    print(f"Step 2: Removed non-duplicate PRNs. ({len(df)} -> {len(df_pairs)} rows)")
    
    if df_pairs.empty:
        print("No paired signals found.")
        return

    # ---------------------------------------------------------
    # Step 3 & 4: 각 row 별 저장 및 utc_millis 기준 정렬
    # ---------------------------------------------------------
    # 전략: 각 PRN별로 데이터를 추출한 뒤,
    # 같은 PRN 내에서 Channel ID(혹은 등장 순서)를 기준으로 두 개의 스트림으로 분리
    
    # PRN 목록 확인
    unique_prns = np.sort(df_pairs['PRN'].unique())
    print(f"Detected PRNs with interference: {unique_prns}")
    
    for prn in unique_prns:
        print(f"\n--- Analyzing PRN {int(prn)} ---")
        
        # 해당 PRN 데이터만 추출
        sub_df = df_pairs[df_pairs['PRN'] == prn].copy()
        
        # 같은 시간에 두 개의 데이터가 있을 때, 
        # 하나는 'Signal A', 하나는 'Signal B'로 구분해야 함.
        # 구분 기준: 위도/경도 정보가 없다면, 보통 Channel Index나 값의 크기 등으로 구분
        # 여기서는 단순히 '그룹 내에서 첫 번째로 잡힌 것'과 '두 번째로 잡힌 것'으로 나눕니다.
        # (채널 번호가 낮은 것이 항상 1번, 높은 것이 2번이라고 가정)
        
        # 각 시간별로 랭킹(1, 2, ...)을 매김
        sub_df.loc[:, 'rank'] = sub_df.groupby('utc_millis').cumcount() + 1
        
        # Signal 1 (첫 번째 채널)과 Signal 2 (두 번째 채널) 분리
        sig1 = sub_df[sub_df['rank'] == 1].set_index('utc_millis').sort_index()
        sig2 = sub_df[sub_df['rank'] == 2].set_index('utc_millis').sort_index()
        
        # Inner Join으로 시간축 정렬 (교집합만 남김 - 범위 밖 삭제)
        # suffixes=('_1', '_2')로 컬럼명 구분
        merged = sig1.join(sig2, lsuffix='_1', rsuffix='_2', how='inner')
        
        print(f"  Aligned Samples: {len(merged)}")
        
        if len(merged) > 0:
            # 분석 예시: Pseudorange 차이 계산
            diff_psr = merged['Pseudorange_1'] - merged['Pseudorange_2']
            diff_tow = merged['TOW_1'] - merged['TOW_2']
            diff_doppler = merged['Doppler_1'] - merged['Doppler_2']

            
            # 통계 출력
            print(f"  [Doppler Diff] Mean: {diff_doppler.mean():.4f} Hz, Std: {diff_doppler.std():.4f} Hz")
            
            print(f"  [Pseudorange Diff] Mean: {diff_psr.mean():.4f} m, Std: {diff_psr.std():.4f} m")
            
            # 1ms 정수 오차 확인 (빛의 속도 c approx 3e8)
            c = 299792458.0
            diff_ms = (diff_psr / c) * 1000.0
            print(f"  [Pseudo to Time Diff] Mean: {diff_ms.mean():.6f} ms")

            print(f"  [TOW Diff] Mean: {diff_tow.mean():.6f} s, Std: {diff_tow.std():.6f} s")

            # --- CMC computation ---
            merged["cmc_1"] = merged["Pseudorange_1"] + merged["Phase_1"] * L1_LAMBDA_M
            merged["cmc_2"] = merged["Pseudorange_2"] + merged["Phase_2"] * L1_LAMBDA_M

            # --- CMC slope per PRN (using UTC millis as time axis) ---
            # Use elapsed seconds to avoid numerical issues with large UTC millis values.
            t_sec = (merged.index.to_numpy(dtype=np.float64) - float(merged.index[0])) / 1000.0

            if len(t_sec) >= 2 and np.nanstd(t_sec) > 0:
                slope1, intercept1 = np.polyfit(t_sec, merged["cmc_1"].to_numpy(dtype=np.float64), 1)
                slope2, intercept2 = np.polyfit(t_sec, merged["cmc_2"].to_numpy(dtype=np.float64), 1)
                slope_diff = slope1 - slope2

                print(f"  [CMC Slope] cmc_1: {slope1:.6e} m/s, cmc_2: {slope2:.6e} m/s")
                print(f"  [CMC Slope Diff] (cmc_1 - cmc_2): {slope_diff:.6e} m/s")
            else:
                print("  [CMC Slope] Not enough samples or degenerate time axis to fit slope.")

            


if __name__ == "__main__":
    analyze_and_align_signals(sys.argv[1])