/*!
 * \file four_bit_cpx_file_signal_source.cc
 * \brief Implementation of a class that reads signals samples from a 2 bit complex sampler front-end file
 * and adapts it to a SignalSourceInterface.
 * \author Javier Arribas, 2015 jarribas(at)cttc.es
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2021  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */

#include "four_bit_cpx_file_signal_source.h"
#include "configuration_interface.h"
#include "gnss_sdr_string_literals.h"
#include <glog/logging.h>

using namespace std::string_literals;


FourBitCpxFileSignalSource::FourBitCpxFileSignalSource(
    const ConfigurationInterface* configuration,
    const std::string& role,
    unsigned int in_streams, unsigned int out_streams,
    Concurrent_Queue<pmt::pmt_t>* queue)
    : FileSourceBase(configuration, role, "Four_Bit_Cpx_File_Signal_Source"s, queue, "byte"s)
{
    sample_type_ = configuration->property(role + ".sample_type", "iq"s);
    // the complex-ness of the input is inferred from the output type
    if (sample_type_ == "iq")
        {
            reverse_interleaving_ = false;
        }
    else if (sample_type_ == "qi")
        {
            reverse_interleaving_ = true;
        }
    else
        {
            LOG(WARNING) << sample_type_ << " unrecognized sample type. Assuming: ";
        }


    if (in_streams > 0)
        {
            LOG(ERROR) << "A signal source does not have an input stream";
        }
    if (out_streams > 1)
        {
            LOG(ERROR) << "This implementation only supports one output stream";
        }
}


std::tuple<size_t, bool> FourBitCpxFileSignalSource::itemTypeToSize()
{
    auto is_complex = false;
    auto item_size = size_t(sizeof(char));  // default

    if (item_type() == "byte")
        {
            item_size = sizeof(char);
        }
    else
        {
            LOG(WARNING) << item_type() << " unrecognized item type. Using byte.";
        }

    return std::make_tuple(item_size, is_complex);
}

// 1 byte -> 1 complex samples
double FourBitCpxFileSignalSource::packetsPerSample() const { return 1.0; }
gnss_shared_ptr<gr::block> FourBitCpxFileSignalSource::source() const { return inter_shorts_to_cpx_; }


void FourBitCpxFileSignalSource::create_file_source_hook()
{
    unpack_byte_ = make_unpack_byte_4bit_samples();
    DLOG(INFO) << "unpack_byte_2bit_cpx_samples(" << unpack_byte_->unique_id() << ")";
    inter_shorts_to_cpx_ = gr::blocks::interleaved_short_to_complex::make(false, reverse_interleaving_);  // I/Q swap enabled
    DLOG(INFO) << "interleaved_short_to_complex(" << inter_shorts_to_cpx_->unique_id() << ")";
}

void FourBitCpxFileSignalSource::pre_connect_hook(gr::top_block_sptr top_block)
{
    top_block->connect(file_source(), 0, unpack_byte_, 0);
    top_block->connect(unpack_byte_, 0, inter_shorts_to_cpx_, 0);
    DLOG(INFO) << "connected file_source to unpacker";
}

void FourBitCpxFileSignalSource::pre_disconnect_hook(gr::top_block_sptr top_block)
{
    top_block->disconnect(file_source(), 0, unpack_byte_, 0);
    top_block->disconnect(unpack_byte_, 0, inter_shorts_to_cpx_, 0);
    DLOG(INFO) << "disconnected file_source from unpacker";
}
