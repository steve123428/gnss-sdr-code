# -*- coding: utf-8 -*-
import argparse
import numpy as np
import h5py
import csv
import pandas as pd
import matplotlib.pyplot as plt
import math

Fs = 4_000_000  # 4 MHz
samples_per_chip = Fs / 1.023e6  # GPS C/A

# ------------------ KEY 목록 ----------------------
OPTIONAL_DATA = {
    "utc": ["utc_millis"],
    "state": ["tracking_state"],
    "C/N0": ["CN0_SNV_dB_Hz"],
    "code_phase_in_samples": ["aux1"],
    "total_samples": ["aux2"],
    "Doppler": ["carrier_doppler_hz"],
    "Carrier_phase": ["acc_carrier_phase_rad"],
    "rem_code_phase_chips": ["rem_code_phase_chips"],
    "full_code_phase_reconstructed": ["__dummy__"],
}

# ------------------ Unwrap 설정 ---------------------
UNWRAP_LABELS = {
    "code_phase_in_samples": {
        "diff": {"wrap_thresh": 0.5, "wrap_offset": 1.0, "accumulate": False},
    },
    "rem_code_phase_chips": {
        "diff": {"wrap_thresh": 0.1, "wrap_offset": 0.25575, "accumulate": False},
    },
}

# ------------------ 데이터 검색 ---------------------
def find_first_existing(f, key_list):
    for k in key_list:
        if k in f:
            return np.array(f[k]).squeeze()
    return None

# ------------------ HDF5 데이터 추출 ---------------------
def extract_data_hdf5(path, key_map):
    with h5py.File(path, "r") as f:
        data = {}
        for label, key_list in key_map.items():
            val = find_first_existing(f, key_list)
            if val is not None:
                data[label] = val
    return data

# ------------------ 통계 계산 ------------------------
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

# ------------------ unwrap ---------------------
def unwrap_value(arr, wrap_thresh, wrap_offset, accumulate):
    arr = np.asarray(arr, dtype=float)
    if arr.size == 0:
        return arr.copy()

    out = np.zeros_like(arr)
    out[0] = arr[0]
    persistent = 0.0

    for i in range(1, len(arr)):
        diff = arr[i] - arr[i - 1]
        if diff < -wrap_thresh:
            persistent += wrap_offset
        elif diff > wrap_thresh:
            persistent -= wrap_offset

        out[i] = arr[i] + (persistent if accumulate else 0.0)

    return out

# ------------------ UTC 정렬 ---------------------
def _align_on_utc(utc1_ms, v1, utc2_ms, v2):
    t1 = pd.to_datetime(
        np.asarray(utc1_ms, dtype=np.int64),
        unit="ms", utc=True, errors="coerce"
    )
    t2 = pd.to_datetime(
        np.asarray(utc2_ms, dtype=np.int64),
        unit="ms", utc=True, errors="coerce"
    )

    s1 = pd.Series(np.asarray(v1), index=t1)
    s2 = pd.Series(np.asarray(v2), index=t2)

    # 🔑 핵심: 중복 UTC 제거 (평균)
    s1 = s1.groupby(level=0).mean()
    s2 = s2.groupby(level=0).mean()

    df = pd.DataFrame({"v1": s1, "v2": s2}).dropna()

    return df.index, df["v1"].to_numpy(), df["v2"].to_numpy()


# ------------------ Plot ----------------------------
def plot_all(data_pairs, x_pairs=None, x_mode="index", cut_start=1, cut_end=None):
    N = len(data_pairs)
    fig, axs = plt.subplots(N, 2, figsize=(15, 4 * N))
    if N == 1:
        axs = np.array([axs])

    cut_label = f"{cut_start} ~ {cut_end if cut_end else 'end'}"
    fig.suptitle(
        f"Receiver Comparison ({'UTC' if x_mode=='utc' else 'Index'})  [cut: {cut_label}]",
        fontsize=16
    )

    for i, (label, (v1, v2, diff)) in enumerate(data_pairs.items()):
        if x_mode == "utc" and x_pairs and label in x_pairs:
            x = x_pairs[label]
        else:
            x = np.arange(1, len(v1) + 1)

        axs[i][0].plot(x, v1, label="Rec1")
        axs[i][0].plot(x, v2, label="Rec2")
        axs[i][0].set_title(label)
        axs[i][0].legend()
        axs[i][0].grid()

        axs[i][1].plot(x, diff, color="gray")
        axs[i][1].set_title(f"{label} Diff")
        axs[i][1].grid()

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.show()

# ================================================================
# =================== 메인 비교 로직 ============================
# ================================================================
def compare_files(file1, file2, cut_start=None, cut_end=None, use_utc=False):
    data1 = extract_data_hdf5(file1, OPTIONAL_DATA)
    data2 = extract_data_hdf5(file2, OPTIONAL_DATA)

    # -------- cut 범위 처리 (1-based → 0-based slicing) --------
    s0 = 0 if cut_start is None else max(0, int(cut_start) - 1)
    e0 = None if cut_end is None else int(cut_end)

    results = {}
    data_pairs = {}
    x_pairs = {}

    # -------- total_samples 0 기준 정규화 --------
    for d in (data1, data2):
        if "total_samples" in d:
            d["total_samples"] = d["total_samples"].astype(float) - float(d["total_samples"][0])

    # -------- full_code_phase 재구성 --------
    for d in (data1, data2):
        if "total_samples" in d and "code_phase_in_samples" in d:
            d["full_code_phase_reconstructed"] = (
                d["total_samples"].astype(float)
                + d["code_phase_in_samples"].astype(float)
            )

    utc1 = data1.get("utc")
    utc2 = data2.get("utc")

    for label in OPTIONAL_DATA:
        if label not in data1 or label not in data2 or label == "utc":
            results[label] = None
            continue

        v1_full = np.asarray(data1[label])
        v2_full = np.asarray(data2[label])

        # -------- alignment --------
        if use_utc:
            if utc1 is None or utc2 is None:
                continue
            t, v1a, v2a = _align_on_utc(utc1, v1_full, utc2, v2_full)
            v1, v2 = v1a[s0:e0], v2a[s0:e0]
            x_pairs[label] = t[s0:e0]
        else:
            N = min(len(v1_full), len(v2_full))
            v1, v2 = v1_full[s0:e0], v2_full[s0:e0]

        if len(v1) == 0:
            continue

        m = min(len(v1), len(v2))
        if m == 0:
            results[label] = None
            continue

        v1 = v1[:m]
        v2 = v2[:m]

        # -------- unwrap --------
        cfg = UNWRAP_LABELS.get(label)
        if cfg and "diff" in cfg:
            diff_raw = v1 - v2
            diff = unwrap_value(diff_raw, **cfg["diff"])
        else:
            diff = v1 - v2

        # -------- carrier / sample offset 제거 --------
        if label.lower() == "carrier_phase" or "total_samples" in label:
            v1 -= v1[0]
            v2 -= v2[0]
            diff = v1 - v2

        # -------- full_code_phase chip 정규화 --------
        if label == "full_code_phase_reconstructed":
            v1 = (v1 / samples_per_chip) % 1023 / 1023
            v2 = (v2 / samples_per_chip) % 1023 / 1023
            diff = v1 - v2

        results[label] = compute_stats(v1, v2, diff)
        data_pairs[label] = (v1, v2, diff)

    if data_pairs:
        plot_all(
            data_pairs,
            x_pairs=x_pairs if use_utc else None,
            x_mode="utc" if use_utc else "index",
            cut_start=cut_start or 1,
            cut_end=cut_end
        )

    return results

# ================================================================
# ======================== CLI =================================
# ================================================================
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("file1")
    parser.add_argument("file2")
    parser.add_argument("--cut_start", type=int, default=None)
    parser.add_argument("--cut_end", type=int, default=None)
    parser.add_argument("--utc", action="store_true")
    parser.add_argument("--out", type=str, default=None)

    args = parser.parse_args()

    results = compare_files(
        args.file1,
        args.file2,
        cut_start=args.cut_start,
        cut_end=args.cut_end,
        use_utc=args.utc,
    )

