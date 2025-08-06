/*!
 * \file tlm_utils.cc
 * \brief Utilities for the telemetry decoder blocks.
 * \author Carles Fernandez, 2020. cfernandez(at)cttc.es
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */

#include "tlm_utils.h"
#include "gnss_sdr_filesystem.h"
#include <matio.h>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>


int save_tlm_matfile(const std::string &dumpfile)
{
    std::ifstream::pos_type size;
    const int32_t number_of_double_vars = 14;
    const int32_t number_of_int_vars = 2;
    const int32_t epoch_size_bytes = sizeof(uint64_t) + sizeof(double) * number_of_double_vars +
                                     sizeof(int32_t) * number_of_int_vars;
    std::ifstream dump_file;
    const std::string &dump_filename_(dumpfile);

    std::cout << "Generating .mat file for " << std::string(dump_filename_.begin(), dump_filename_.end() - 4) << '\n';
    dump_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
        {
            dump_file.open(dump_filename_.c_str(), std::ios::binary | std::ios::ate);
        }
    catch (const std::ifstream::failure &e)
        {
            std::cerr << "Problem opening dump file:" << e.what() << '\n';
            return 1;
        }
    // count number of epochs and rewind
    int64_t num_epoch = 0;
    if (dump_file.is_open())
        {
            size = dump_file.tellg();
            num_epoch = static_cast<int64_t>(size) / static_cast<int64_t>(epoch_size_bytes);
            if (num_epoch == 0LL)
                {
                    // empty file, exit
                    return 1;
                }
            dump_file.seekg(0, std::ios::beg);
        }
    else
        {
            return 1;
        }
    auto TOW_at_current_symbol_ms = std::vector<double>(num_epoch);
    auto tracking_sample_counter = std::vector<uint64_t>(num_epoch);
    auto TOW_at_Preamble_ms = std::vector<double>(num_epoch);
    auto nav_symbol = std::vector<int32_t>(num_epoch);
    auto prn = std::vector<int32_t>(num_epoch);
    auto carrier_lock_test = std::vector<double>(num_epoch);
    auto acc_carrier_phase_rad = std::vector<double>(num_epoch);
    auto carr_error_hz = std::vector<double>(num_epoch);
    auto carr_error_filt_hz = std::vector<double>(num_epoch);
    auto code_error_chips = std::vector<double>(num_epoch);
    auto code_error_filt_chips = std::vector<double>(num_epoch);
    auto CN0_SNV_dB_Hz = std::vector<double>(num_epoch);
    auto abs_VE = std::vector<double>(num_epoch);
    auto abs_E = std::vector<double>(num_epoch);
    auto abs_P = std::vector<double>(num_epoch);
    auto abs_L = std::vector<double>(num_epoch);
    auto abs_VL = std::vector<double>(num_epoch);

    try
        {
            if (dump_file.is_open())
                {
                    for (int64_t i = 0; i < num_epoch; i++)
                        {
                            dump_file.read(reinterpret_cast<char *>(&TOW_at_current_symbol_ms[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&tracking_sample_counter[i]), sizeof(uint64_t));
                            dump_file.read(reinterpret_cast<char *>(&TOW_at_Preamble_ms[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&nav_symbol[i]), sizeof(int32_t));
                            dump_file.read(reinterpret_cast<char *>(&prn[i]), sizeof(int32_t));
                            dump_file.read(reinterpret_cast<char *>(&carrier_lock_test[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&acc_carrier_phase_rad[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&carr_error_hz[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&carr_error_filt_hz[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&code_error_chips[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&code_error_filt_chips[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&CN0_SNV_dB_Hz[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&abs_VE[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&abs_E[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&abs_P[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&abs_L[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&abs_VL[i]), sizeof(double));
                        }
                }
            dump_file.close();
        }
    catch (const std::ifstream::failure &e)
        {
            std::cerr << "Problem reading dump file:" << e.what() << '\n';
            return 1;
        }

    // WRITE MAT FILE
    mat_t *matfp;
    matvar_t *matvar;
    std::string filename = dump_filename_;
    filename.erase(filename.length() - 4, 4);
    filename.append(".mat");
    try
        {
            matfp = Mat_CreateVer(filename.c_str(), nullptr, MAT_FT_MAT73);
            if (reinterpret_cast<int64_t *>(matfp) != nullptr)
                {
                    std::array<size_t, 2> dims{1, static_cast<size_t>(num_epoch)};
                    matvar = Mat_VarCreate("TOW_at_current_symbol_ms", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), TOW_at_current_symbol_ms.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("tracking_sample_counter", MAT_C_UINT64, MAT_T_UINT64, 2, dims.data(), tracking_sample_counter.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("TOW_at_Preamble_ms", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), TOW_at_Preamble_ms.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("nav_symbol", MAT_C_INT32, MAT_T_INT32, 2, dims.data(), nav_symbol.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("PRN", MAT_C_INT32, MAT_T_INT32, 2, dims.data(), prn.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("carrier_lock_test", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), carrier_lock_test.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("acc_carrier_phase_rad", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), acc_carrier_phase_rad.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("carr_error_hz", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), carr_error_hz.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("carr_error_filt_hz", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), carr_error_filt_hz.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("code_error_chips", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), code_error_chips.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("code_error_filt_chips", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), code_error_filt_chips.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("CN0_SNV_dB_Hz", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), CN0_SNV_dB_Hz.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("abs_VE", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), abs_VE.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("abs_E", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), abs_E.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("abs_P", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), abs_P.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("abs_L", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), abs_L.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);

                    matvar = Mat_VarCreate("abs_VL", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), abs_VL.data(), 0);
                    Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
                    Mat_VarFree(matvar);
                }
            Mat_Close(matfp);
        }
    catch (const std::exception &ex)
        {
            std::cerr << "Error saving the .mat file: " << ex.what();
        }
    return 0;
}


bool tlm_remove_file(const std::string &file_to_remove)
{
    errorlib::error_code ec;
    return fs::remove(fs::path(file_to_remove), ec);
}
