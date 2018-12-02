/*!
 * \file galileo_e1_pcps_ambiguous_acquisition.cc
 * \brief Adapts a PCPS acquisition block to an AcquisitionInterface for
 *  Galileo E1 Signals
 * \author Luis Esteve, 2012. luis(at)epsilon-formacion.com
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2018  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <https://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */

#include "galileo_e1_pcps_ambiguous_acquisition.h"
#include "configuration_interface.h"
#include "galileo_e1_signal_processing.h"
#include "Galileo_E1.h"
#include "gnss_sdr_flags.h"
#include "acq_conf.h"
#include <boost/math/distributions/exponential.hpp>
#include <glog/logging.h>


using google::LogMessage;


GalileoE1PcpsAmbiguousAcquisition::GalileoE1PcpsAmbiguousAcquisition(
    ConfigurationInterface* configuration,
    const std::string& role,
    unsigned int in_streams,
    unsigned int out_streams) : role_(role),
                                in_streams_(in_streams),
                                out_streams_(out_streams)
{
    Acq_Conf acq_parameters;
    configuration_ = configuration;
    std::string default_item_type = "gr_complex";
    std::string default_dump_filename = "./acquisition.mat";

    DLOG(INFO) << "role " << role;

    item_type_ = configuration_->property(role + ".item_type", default_item_type);

    int64_t fs_in_deprecated = configuration_->property("GNSS-SDR.internal_fs_hz", 4000000);
    fs_in_ = configuration_->property("GNSS-SDR.internal_fs_sps", fs_in_deprecated);
    acq_parameters.fs_in = fs_in_;
    acq_parameters.samples_per_chip = static_cast<unsigned int>(ceil((1.0 / Galileo_E1_CODE_CHIP_RATE_HZ) * static_cast<float>(acq_parameters.fs_in)));
    doppler_max_ = configuration_->property(role + ".doppler_max", 5000);
    if (FLAGS_doppler_max != 0) doppler_max_ = FLAGS_doppler_max;
    acq_parameters.doppler_max = doppler_max_;
    acq_parameters.ms_per_code = 4;
    sampled_ms_ = configuration_->property(role + ".coherent_integration_time_ms", acq_parameters.ms_per_code);
    acq_parameters.sampled_ms = sampled_ms_;
    if ((acq_parameters.sampled_ms % acq_parameters.ms_per_code) != 0)
        {
            LOG(WARNING) << "Parameter coherent_integration_time_ms should be a multiple of 4. Setting it to 4";
            acq_parameters.sampled_ms = acq_parameters.ms_per_code;
        }
    bit_transition_flag_ = configuration_->property(role + ".bit_transition_flag", false);
    acq_parameters.bit_transition_flag = bit_transition_flag_;
    use_CFAR_algorithm_flag_ = configuration_->property(role + ".use_CFAR_algorithm", true);  //will be false in future versions
    acq_parameters.use_CFAR_algorithm_flag = use_CFAR_algorithm_flag_;
    acquire_pilot_ = configuration_->property(role + ".acquire_pilot", false);  //will be true in future versions
    max_dwells_ = configuration_->property(role + ".max_dwells", 1);
    acq_parameters.max_dwells = max_dwells_;
    dump_ = configuration_->property(role + ".dump", false);
    acq_parameters.dump = dump_;
    acq_parameters.dump_channel = configuration_->property(role + ".dump_channel", 0);
    blocking_ = configuration_->property(role + ".blocking", true);
    acq_parameters.blocking = blocking_;
    dump_filename_ = configuration_->property(role + ".dump_filename", default_dump_filename);
    acq_parameters.dump_filename = dump_filename_;
    //--- Find number of samples per spreading code (4 ms)  -----------------
    code_length_ = static_cast<unsigned int>(std::floor(static_cast<double>(fs_in_) / (Galileo_E1_CODE_CHIP_RATE_HZ / Galileo_E1_B_CODE_LENGTH_CHIPS)));

    float samples_per_ms = static_cast<float>(fs_in_) * 0.001;
    acq_parameters.samples_per_ms = samples_per_ms;
    acq_parameters.samples_per_code = acq_parameters.samples_per_ms * static_cast<float>(Galileo_E1_CODE_PERIOD_MS);
    vector_length_ = sampled_ms_ * samples_per_ms;

    if (bit_transition_flag_)
        {
            vector_length_ *= 2;
        }

    code_ = new gr_complex[vector_length_];

    if (item_type_.compare("cshort") == 0)
        {
            item_size_ = sizeof(lv_16sc_t);
        }
    else
        {
            item_size_ = sizeof(gr_complex);
        }
    acq_parameters.it_size = item_size_;
    acq_parameters.num_doppler_bins_step2 = configuration_->property(role + ".second_nbins", 4);
    acq_parameters.doppler_step2 = configuration_->property(role + ".second_doppler_step", 125.0);
    acq_parameters.make_2_steps = configuration_->property(role + ".make_two_steps", false);
    acq_parameters.blocking_on_standby = configuration_->property(role + ".blocking_on_standby", false);
    acquisition_ = pcps_make_acquisition(acq_parameters);
    DLOG(INFO) << "acquisition(" << acquisition_->unique_id() << ")";

    if (item_type_.compare("cbyte") == 0)
        {
            cbyte_to_float_x2_ = make_complex_byte_to_float_x2();
            float_to_complex_ = gr::blocks::float_to_complex::make();
        }

    channel_ = 0;
    threshold_ = 0.0;
    doppler_step_ = 0;
    gnss_synchro_ = nullptr;
    if (in_streams_ > 1)
        {
            LOG(ERROR) << "This implementation only supports one input stream";
        }
    if (out_streams_ > 0)
        {
            LOG(ERROR) << "This implementation does not provide an output stream";
        }
}


GalileoE1PcpsAmbiguousAcquisition::~GalileoE1PcpsAmbiguousAcquisition()
{
    delete[] code_;
}


void GalileoE1PcpsAmbiguousAcquisition::stop_acquisition()
{
}


void GalileoE1PcpsAmbiguousAcquisition::set_channel(unsigned int channel)
{
    channel_ = channel;
    acquisition_->set_channel(channel_);
}


void GalileoE1PcpsAmbiguousAcquisition::set_threshold(float threshold)
{
    float pfa = configuration_->property(role_ + std::to_string(channel_) + ".pfa", 0.0);

    if (pfa == 0.0) pfa = configuration_->property(role_ + ".pfa", 0.0);

    if (pfa == 0.0)
        {
            threshold_ = threshold;
        }
    else
        {
            threshold_ = calculate_threshold(pfa);
        }

    DLOG(INFO) << "Channel " << channel_ << " Threshold = " << threshold_;

    acquisition_->set_threshold(threshold_);
}


void GalileoE1PcpsAmbiguousAcquisition::set_doppler_max(unsigned int doppler_max)
{
    doppler_max_ = doppler_max;

    acquisition_->set_doppler_max(doppler_max_);
}


void GalileoE1PcpsAmbiguousAcquisition::set_doppler_step(unsigned int doppler_step)
{
    doppler_step_ = doppler_step;

    acquisition_->set_doppler_step(doppler_step_);
}


void GalileoE1PcpsAmbiguousAcquisition::set_gnss_synchro(Gnss_Synchro* gnss_synchro)
{
    gnss_synchro_ = gnss_synchro;

    acquisition_->set_gnss_synchro(gnss_synchro_);
}


signed int GalileoE1PcpsAmbiguousAcquisition::mag()
{
    return acquisition_->mag();
}


void GalileoE1PcpsAmbiguousAcquisition::init()
{
    acquisition_->init();
    //set_local_code();
}


void GalileoE1PcpsAmbiguousAcquisition::set_local_code()
{
    bool cboc = configuration_->property(
        "Acquisition" + std::to_string(channel_) + ".cboc", false);

    std::complex<float>* code = new std::complex<float>[code_length_];

    if (acquire_pilot_ == true)
        {
            //set local signal generator to Galileo E1 pilot component (1C)
            char pilot_signal[3] = "1C";
            galileo_e1_code_gen_complex_sampled(code, pilot_signal,
                cboc, gnss_synchro_->PRN, fs_in_, 0, false);
        }
    else
        {
            galileo_e1_code_gen_complex_sampled(code, gnss_synchro_->Signal,
                cboc, gnss_synchro_->PRN, fs_in_, 0, false);
        }


    for (unsigned int i = 0; i < sampled_ms_ / 4; i++)
        {
            memcpy(&(code_[i * code_length_]), code, sizeof(gr_complex) * code_length_);
        }

    acquisition_->set_local_code(code_);
    delete[] code;
}


void GalileoE1PcpsAmbiguousAcquisition::reset()
{
    acquisition_->set_active(true);
}


void GalileoE1PcpsAmbiguousAcquisition::set_state(int state)
{
    acquisition_->set_state(state);
}


float GalileoE1PcpsAmbiguousAcquisition::calculate_threshold(float pfa)
{
    unsigned int frequency_bins = 0;
    for (int doppler = static_cast<int>(-doppler_max_); doppler <= static_cast<int>(doppler_max_); doppler += doppler_step_)
        {
            frequency_bins++;
        }

    DLOG(INFO) << "Channel " << channel_ << "  Pfa = " << pfa;

    unsigned int ncells = vector_length_ * frequency_bins;
    double exponent = 1 / static_cast<double>(ncells);
    double val = pow(1.0 - pfa, exponent);
    double lambda = double(vector_length_);
    boost::math::exponential_distribution<double> mydist(lambda);
    float threshold = static_cast<float>(quantile(mydist, val));

    return threshold;
}


void GalileoE1PcpsAmbiguousAcquisition::connect(gr::top_block_sptr top_block)
{
    if (item_type_ == "gr_complex")
        {
            // nothing to connect
        }
    else if (item_type_ == "cshort")
        {
            // nothing to connect
        }
    else if (item_type_ == "cbyte")
        {
            // Since a byte-based acq implementation is not available,
            // we just convert cshorts to gr_complex
            top_block->connect(cbyte_to_float_x2_, 0, float_to_complex_, 0);
            top_block->connect(cbyte_to_float_x2_, 1, float_to_complex_, 1);
            top_block->connect(float_to_complex_, 0, acquisition_, 0);
        }
    else
        {
            LOG(WARNING) << item_type_ << " unknown acquisition item type";
        }
}


void GalileoE1PcpsAmbiguousAcquisition::disconnect(gr::top_block_sptr top_block)
{
    if (item_type_ == "gr_complex")
        {
            // nothing to disconnect
        }
    else if (item_type_ == "cshort")
        {
            // nothing to disconnect
        }
    else if (item_type_ == "cbyte")
        {
            top_block->disconnect(cbyte_to_float_x2_, 0, float_to_complex_, 0);
            top_block->disconnect(cbyte_to_float_x2_, 1, float_to_complex_, 1);
            top_block->disconnect(float_to_complex_, 0, acquisition_, 0);
        }
    else
        {
            LOG(WARNING) << item_type_ << " unknown acquisition item type";
        }
}


gr::basic_block_sptr GalileoE1PcpsAmbiguousAcquisition::get_left_block()
{
    if (item_type_ == "gr_complex")
        {
            return acquisition_;
        }
    else if (item_type_ == "cshort")
        {
            return acquisition_;
        }
    else if (item_type_ == "cbyte")
        {
            return cbyte_to_float_x2_;
        }
    else
        {
            LOG(WARNING) << item_type_ << " unknown acquisition item type";
            return nullptr;
        }
}


gr::basic_block_sptr GalileoE1PcpsAmbiguousAcquisition::get_right_block()
{
    return acquisition_;
}
