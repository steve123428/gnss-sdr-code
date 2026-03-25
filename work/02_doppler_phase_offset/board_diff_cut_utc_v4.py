# -*- coding: utf-8 -*-
import argparse
import numpy as np
import h5py
import pandas as pd
import matplotlib.pyplot as plt
import os
import glob
from matplotlib.ticker import FuncFormatter

Fs = 4_000_000  # 4 MHz
samples_per_chip = Fs / 1.023e6  # GPS C/A
samples_per_usec = Fs / 1_000_000  # samples per microsecond

# ------------------ KEY 목록 ----------------------
OPTIONAL_DATA = {
    "utc": ["utc_millis", "RX_time", "gps_time"],
    "PRN": ["PRN", "prn"],
    "state": ["tracking_state", "state"],
    "SNR (dB-Hz)": ["CN0_SNV_dB_Hz", "cn0_db_hz", "snr"],
    "code_phase_in_samples": ["aux1", "Code_Phase_samples"],
    "total_samples": ["aux2", "Total_Samples"],
    "Doppler Frequency (Hz)": ["carrier_doppler_hz", "Carrier_Doppler_hz", "doppler_hz"],
    "Accumulated Carrier Phase (rad)": ["acc_carrier_phase_rad", "Carrier_Phase_rad"],
    "Code Phase (usec)": ["__dummy__"],
    "code_freq_chips" : ["code_freq_chips", "Code_Freq_chips"],
    "code_freq_rate_chips" : ["code_freq_rate_chips"],
    "code_phase_step_chips" : ["code_phase_step_chips"],
    "code_error_filt_chips" : ["code_error_filt_chips"],
    "abs_code_phase" : ["abs_code_phase"],
    "full_code_phase_samples" : ["full_code_phase_samples"],
}

PLOTTING_DATA = [
    "SNR (dB-Hz)",
    "Doppler Frequency (Hz)",
    "Accumulated Carrier Phase (rad)",
    "state",
    "Code Phase (usec)",
    "PRN",
    "code_freq_chips",
    "code_phase_step_chips",
    "code_error_filt_chips",
    "code_phase_in_samples",
]

def find_first_existing(f, key_list):
    file_keys = list(f.keys())
    for k in key_list:
        if k in f: return np.array(f[k]).squeeze()
        for fk in file_keys:
            if fk.lower() == k.lower(): return np.array(f[fk]).squeeze()
    return None

def extract_data_hdf5(path, key_map):
    if not os.path.exists(path):
        print(f"Error: File not found {path}")
        return {}
    with h5py.File(path, "r") as f:
        data = {}
        for label, key_list in key_map.items():
            val = find_first_existing(f, key_list)
            if val is not None: data[label] = val
    return data

def compute_stats(v1, v2, diff):
    def stat(x):
        return {
            "count": int(len(x)),
            "mean": float(np.mean(x)) if len(x) else float("nan"),
            "std": float(np.std(x)) if len(x) else float("nan"),
            "min": float(np.min(x)) if len(x) else float("nan"),
            "max": float(np.max(x)) if len(x) else float("nan"),
            "var": float(np.var(x)) if len(x) else float("nan"),
        }
    return stat(v1), stat(v2), stat(diff)

def _align_on_utc(utc1_ms, v1, utc2_ms, v2):
    t1 = pd.to_datetime(np.asarray(utc1_ms, dtype=np.int64), unit="ms", utc=True, errors="coerce")
    t2 = pd.to_datetime(np.asarray(utc2_ms, dtype=np.int64), unit="ms", utc=True, errors="coerce")
    s1 = pd.Series(np.asarray(v1), index=t1)
    s2 = pd.Series(np.asarray(v2), index=t2)
    s1 = s1.groupby(level=0).mean()
    s2 = s2.groupby(level=0).mean()
    df = pd.DataFrame({"v1": s1, "v2": s2}).dropna()
    return df.index, df["v1"].to_numpy(), df["v2"].to_numpy()

def plot_all(data_pairs, x_pairs=None, x_mode="index", cut_start=1, cut_end=None, start_time=None, file1=None, file2=None):
    N = len(data_pairs)
    fig, axs = plt.subplots(N, 2, figsize=(15, 4 * N), sharex=True)
    if N == 1: axs = np.array([axs])

    cut_label = f"{cut_start} ~ {cut_end if cut_end else 'end'}"
    dir_name = file1.split("/")[-2] if file1 else "Directory"
    file1_name = os.path.basename(file1).replace(".mat","") if file1 else "File 1"
    file2_name = os.path.basename(file2).replace(".mat","") if file2 else "File 2"

    fig.suptitle(f"{dir_name}: {file1_name} vs {file2_name} ({cut_label})", fontsize=16)

    def time_formatter(x, pos):
        if start_time is not None:
            return (start_time + pd.Timedelta(seconds=x)).strftime("%H:%M:%S")
        else:
            return f"{int(x)}"
    formatter = FuncFormatter(time_formatter)
    
    csv_frames = []

    for i, (label, (v1, v2, diff)) in enumerate(data_pairs.items()):
        is_bottom = (i == N - 1)
        if x_mode == "utc" and x_pairs and label in x_pairs:
            x = x_pairs[label]
        else:
            x = np.arange(1, len(v1) + 1)

        axs[i][0].plot(x, v1, label="Authentic")
        axs[i][0].plot(x, v2, label="Spoofing")
        axs[i][0].set_title(label)
        axs[i][0].legend(loc='upper right')
        axs[i][0].grid()

        axs[i][1].plot(x, diff, color="gray")
        axs[i][1].set_title(f"Difference of {label}")
        axs[i][1].grid()

        if x_mode == "utc":
            for ax in axs[i]:
                ax.xaxis.set_major_formatter(formatter)
                if not is_bottom: ax.tick_params(labelbottom=False)
                else: 
                    if start_time: ax.set_xlabel(f"Time (start: {start_time.strftime('%H:%M:%S')} UTC)")
                    else: ax.set_xlabel("Elapsed time (sec)")
        else:
            if not is_bottom:
                for ax in axs[i]: ax.tick_params(labelbottom=False)
            else:
                axs[i][0].set_xlabel("Index")
                axs[i][1].set_xlabel("Index")

        df_one = pd.DataFrame({"metric": label, "x": x, "authentic": v1, "mixed": v2, "diff": diff})
        if x_mode == "utc" and start_time:
            df_one["utc_time"] = (start_time + pd.to_timedelta(df_one["x"], unit="s")).dt.strftime("%H:%M:%S.%f")
        csv_frames.append(df_one)

    plt.subplots_adjust(left=0.04, bottom=0.05, right=0.99, top=0.90, hspace=0.25, wspace=0.15)
    
    if csv_frames:
        os.makedirs("./csv", exist_ok=True)
        csv_path = f"./csv/{dir_name}_{file1_name}_{file2_name}.csv"
        pd.concat(csv_frames, ignore_index=True).to_csv(csv_path, index=False)

    os.makedirs("./fig", exist_ok=True)
    plt.savefig(f"./fig/{dir_name}_{file1_name}_{file2_name}.png")
    plt.show()

def scan_and_plot_states(target_dir):
    files = sorted(glob.glob(os.path.join(target_dir, "*.mat")))
    if not files: return
    print(f"Found {len(files)} mat files. Extracting states...")
    prn_groups = {} 
    req_keys = {"PRN": ["PRN"], "state": ["tracking_state"]}

    for fpath in files:
        fname = os.path.basename(fpath)
        try: data = extract_data_hdf5(fpath, req_keys)
        except: continue
        if "PRN" not in data or "state" not in data: continue
        
        prn_arr = np.array(data["PRN"]).flatten()
        state_arr = np.array(data["state"]).flatten()
        if len(prn_arr) == 0: continue
        prn_val = int(prn_arr[prn_arr > 0][0]) if np.any(prn_arr > 0) else 0
        x_data = np.arange(len(state_arr))

        if prn_val not in prn_groups: prn_groups[prn_val] = []
        prn_groups[prn_val].append({"name": fname.replace("tracking_", "").replace(".mat", ""), "x": x_data, "y": state_arr})

    sorted_prns = sorted(prn_groups.keys())
    if not sorted_prns: return
    N = len(sorted_prns)
    fig, axs = plt.subplots(N, 1, figsize=(12, 3 * N), sharex=False)
    if N == 1: axs = [axs]
    fig.suptitle(f"Tracking States by PRN ({target_dir})", fontsize=16)

    for i, prn in enumerate(sorted_prns):
        ax = axs[i]
        group_list = prn_groups[prn]
        if len(group_list) >= 2:
            min_len = min(len(item["y"]) for item in group_list)
            y_matrix = np.array([item["y"][:min_len] for item in group_list])
            match_mask = np.all(y_matrix == 4, axis=0)
            ax.fill_between(group_list[0]["x"][:min_len], -1, 10, where=match_mask, color='yellow', alpha=0.3, step='post')

        for item in group_list:
            ax.step(item["x"], item["y"], where='post', label=item["name"], alpha=0.8)
        
        ax.set_ylabel(f"PRN {prn}")
        ax.grid(True, linestyle='--', alpha=0.6)
        ax.legend(loc='upper right', fontsize='small')
        ax.set_ylim(1.5, 4.5)
        ax.set_xlabel("Index")
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.subplots_adjust(hspace=0.4)
    plt.savefig(f"./state/{os.path.basename(target_dir)}.png")
    plt.show()

def compare_files(file1, file2, cut_start=None, cut_end=None, use_utc=False, automatic=False):
    data1 = extract_data_hdf5(file1, OPTIONAL_DATA)
    data2 = extract_data_hdf5(file2, OPTIONAL_DATA)

    # -------- 데이터 전처리 --------
    for d in (data1, data2):
        if "total_samples" in d and "code_phase_in_samples" in d:
            d["Code Phase (usec)"] = d["total_samples"].astype(float) + d["code_phase_in_samples"].astype(float)

    results, data_pairs, x_pairs = {}, {}, {}
    start_time = None
    utc1, utc2 = data1.get("utc"), data2.get("utc")

    # -------- [Mode 1: Automatic Mode] --------
    # 전략: 전체 데이터 UTC 정렬 -> 공통 State 4 구간 탐색 -> 해당 시간대로 다른 데이터 Slice
    if automatic:
        if not use_utc:
            print("Warning: Automatic mode forces UTC alignment enabled.")
            use_utc = True
        
        if utc1 is None or utc2 is None or "state" not in data1 or "state" not in data2:
            print("Error: Automatic mode requires UTC and State data.")
            return {}

        # 1. State를 먼저 UTC로 정렬
        t_aligned, st1_aligned, st2_aligned = _align_on_utc(utc1, data1["state"], utc2, data2["state"])
        
        if len(t_aligned) == 0:
            print("Error: No overlapping UTC timestamps found for Automatic mode.")
            return {}

        # 2. 둘 다 State 4인 구간 찾기
        mask = (st1_aligned == 4) & (st2_aligned == 4)
        if not np.any(mask):
            print("Warning: No overlapping State 4 region found in aligned data.")
            valid_start_time, valid_end_time = t_aligned[0], t_aligned[-1] # Fallback: Full overlap
        else:
            # 가장 긴 연속 구간 찾기
            padded = np.concatenate(([False], mask, [False]))
            diff = np.diff(padded.astype(int))
            starts, ends = np.where(diff == 1)[0], np.where(diff == -1)[0]
            idx = np.argmax(ends - starts)
            
            # 인덱스를 사용하여 Time Range 추출
            valid_start_time = t_aligned[starts[idx]]
            valid_end_time = t_aligned[ends[idx]-1] # end는 exclusive하므로 -1
            print(f"[Automatic] Stable Region (UTC): {valid_start_time.time()} ~ {valid_end_time.time()} (samples: {ends[idx]-starts[idx]})")

        # 3. 각 메트릭에 대해 처리
        for label in OPTIONAL_DATA:
            if label not in data1 or label not in data2 or label == "utc":
                results[label] = None
                continue
            
            # 전체 데이터 Align
            t, v1, v2 = _align_on_utc(utc1, data1[label], utc2, data2[label])
            
            # 위에서 구한 시간 범위로 Slice
            # (시간 비교를 위해 boolean mask 생성)
            time_mask = (t >= valid_start_time) & (t <= valid_end_time)
            v1_valid = v1[time_mask]
            v2_valid = v2[time_mask]
            t_valid = t[time_mask]
            
            if len(v1_valid) == 0: continue

            # 결과 저장
            t_elapsed = (t_valid - t_valid[0]).total_seconds()
            x_pairs[label] = t_elapsed
            if start_time is None: start_time = t_valid[0]
            
            diff = v1_valid - v2_valid
            
            # Post-processing
            if label.lower() == "accumulated carrier phase (rad)" or "total_samplesss" in label:
                v1_valid -= v1_valid[0]; v2_valid -= v2_valid[0]; diff = v1_valid - v2_valid
            if label == "Code Phase (usec)":
                v1_valid = (v1_valid / samples_per_usec) % 1000.0
                v2_valid = (v2_valid / samples_per_usec) % 1000.0
                diff = v1_valid - v2_valid
                diff[diff > 500] -= 1000.0; diff[diff < -500] += 1000.0

            results[label] = compute_stats(v1_valid, v2_valid, diff)
            if label in PLOTTING_DATA: data_pairs[label] = (v1_valid, v2_valid, diff)

        # Plotting (Automatic)
        if data_pairs:
            plot_all(data_pairs, x_pairs=x_pairs, x_mode="utc", 
                     cut_start="Auto", cut_end="Auto", start_time=start_time, file1=file1, file2=file2)
        return results

    # -------- [Mode 2: Manual Mode] --------
    # 전략: Raw Index로 먼저 자르기 -> (옵션) 잘린 조각들 UTC 정렬
    else:
        s0 = 0 if cut_start is None else max(0, int(cut_start) - 1)
        e0 = None if cut_end is None else int(cut_end)
        
        for label in OPTIONAL_DATA:
            if label not in data1 or label not in data2 or label == "utc":
                results[label] = None
                continue
            
            v1_full, v2_full = np.asarray(data1[label]), np.asarray(data2[label])
            
            # 1. Raw Index Slicing (User requested this FIRST)
            N = min(len(v1_full), len(v2_full))
            curr_e0 = e0 if e0 is not None else N
            v1_cut = v1_full[s0:curr_e0]
            v2_cut = v2_full[s0:curr_e0]
            
            if len(v1_cut) == 0: continue

            # 2. (Optional) UTC Alignment on slices
            if use_utc and utc1 is not None and utc2 is not None:
                u1_cut = utc1[s0:curr_e0]
                u2_cut = utc2[s0:curr_e0]
                t, v1, v2 = _align_on_utc(u1_cut, v1_cut, u2_cut, v2_cut)
                
                if len(t) == 0: continue
                t_elapsed = (t - t[0]).total_seconds()
                x_pairs[label] = t_elapsed
                if start_time is None: start_time = t[0]
            else:
                # No UTC alignment
                m = min(len(v1_cut), len(v2_cut))
                v1, v2 = v1_cut[:m], v2_cut[:m]

            # 3. Post-processing & Save
            diff = v1 - v2
            if label.lower() == "accumulated carrier phase (rad)" or "total_samples" in label:
                v1 -= v1[0]; v2 -= v2[0]; diff = v1 - v2
            if label == "Code Phase (usec)":
                v1 = (v1 / samples_per_usec) % 1000.0
                v2 = (v2 / samples_per_usec) % 1000.0
                diff = v1 - v2
                diff[diff > 500] -= 1000.0; diff[diff < -500] += 1000.0

            results[label] = compute_stats(v1, v2, diff)
            if label in PLOTTING_DATA: data_pairs[label] = (v1, v2, diff)

        # Plotting (Manual)
        if data_pairs:
            plot_all(data_pairs, x_pairs=x_pairs if use_utc else None, x_mode="utc" if use_utc else "index",
                     cut_start=s0+1, cut_end=curr_e0, start_time=start_time, file1=file1, file2=file2)
        return results

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("dir")
    parser.add_argument("ch1", nargs='?', default=None)
    parser.add_argument("ch2", nargs='?', default=None)
    parser.add_argument("--cut", "-c", type=int, default=None)
    parser.add_argument("--end", "-e", type=int, default=None)
    parser.add_argument("--utc", "-u", "-t", action="store_true")
    parser.add_argument("--state-only", "-s", action="store_true")
    parser.add_argument("--automatic", "-a", action="store_true", help="Auto-detect stable region")

    args = parser.parse_args()

    if args.state_only or args.ch1 is None:
        scan_and_plot_states(args.dir)
    elif args.ch1 is not None and args.ch2 is not None:
        file1 = f"{args.dir}/tracking_ch{args.ch1}.mat"
        file2 = f"{args.dir}/tracking_ch{args.ch2}.mat"
        results = compare_files(file1, file2, cut_start=args.cut, cut_end=args.end, 
                                use_utc=args.utc, automatic=args.automatic)

        print("Comparison Results:")
        for label in ["Doppler Frequency (Hz)", "Code Phase (usec)" , "code_freq_chips", "code_phase_step_chips", "code_error_filt_chips"]:
            stats = results.get(label)
            if stats is None:
                print(f"  {label}: No data")
                continue
            print(f"  {label}:")
            for name in ["mean", "std", "min", "max"]:
                print(f"    {name}: Auth={stats[0][name]:.6f}, Mixed={stats[1][name]:.6f}, Diff={stats[2][name]:.6f}")
    else:
        print("Usage: python script.py <dir> <ch1> <ch2> [-a] [-u]")