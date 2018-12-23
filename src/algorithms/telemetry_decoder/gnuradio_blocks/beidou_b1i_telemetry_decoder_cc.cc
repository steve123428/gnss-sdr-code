/*!
 * \file beidou_b1i_telemetry_decoder_cc.cc
 * \brief Implementation of an adapter of a BEIDOU BI1 DNAV data decoder block
 * to a TelemetryDecoderInterface
 * \note Code added as part of GSoC 2018 program
 * \author Damian Miralles, 2018. dmiralles2009(at)gmail.com
 * \author Sergi Segura, 2018. sergi.segura.munoz(at)gmail.es
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2015  (see AUTHORS file for a list of contributors)
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
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */


#include "beidou_b1i_telemetry_decoder_cc.h"
#include "control_message_factory.h"
#include "convolutional.h"
#include "display.h"
#include "gnss_synchro.h"
#include <boost/lexical_cast.hpp>
#include <gnuradio/io_signature.h>
#include <glog/logging.h>
#include <volk_gnsssdr/volk_gnsssdr.h>
#include <iostream>

#define CRC_ERROR_LIMIT 8

using google::LogMessage;


beidou_b1i_telemetry_decoder_cc_sptr
beidou_b1i_make_telemetry_decoder_cc(const Gnss_Satellite &satellite, bool dump)
{
    return beidou_b1i_telemetry_decoder_cc_sptr(new beidou_b1i_telemetry_decoder_cc(satellite, dump));
}


beidou_b1i_telemetry_decoder_cc::beidou_b1i_telemetry_decoder_cc(
    const Gnss_Satellite &satellite,
    bool dump) : gr::block("beidou_b1i_telemetry_decoder_cc",
    		     gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)),
                 gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)))
{
    // Ephemeris data port out
    this->message_port_register_out(pmt::mp("telemetry"));
    // initialize internal vars
    d_dump = dump;
    d_satellite = Gnss_Satellite(satellite.get_system(), satellite.get_PRN());
    LOG(INFO) << "Initializing BeiDou B2a Telemetry Decoding";
    // Define the number of sampes per symbol. Notice that BEIDOU has 2 rates, !!!Change
    //one for the navigation data and the other for the preamble information
    d_samples_per_symbol = (BEIDOU_B1I_CODE_RATE_HZ / BEIDOU_B1I_CODE_LENGTH_CHIPS) / BEIDOU_B1I_SYMBOL_RATE_SPS;
    d_symbols_per_preamble = BEIDOU_DNAV_PREAMBLE_LENGTH_SYMBOLS;

    // set the preamble
    d_samples_per_preamble = BEIDOU_DNAV_PREAMBLE_LENGTH_SYMBOLS * d_samples_per_symbol;

    // preamble symbols to samples
    d_secondary_code_symbols = static_cast<int32_t *>(volk_gnsssdr_malloc(BEIDOU_B1I_SECONDARY_CODE_LENGTH * sizeof(int32_t), volk_gnsssdr_get_alignment()));
    d_preamble_samples = static_cast<int32_t *>(volk_gnsssdr_malloc(d_samples_per_preamble * sizeof(int32_t), volk_gnsssdr_get_alignment()));
    d_preamble_period_samples = BEIDOU_DNAV_PREAMBLE_PERIOD_SYMBOLS*d_samples_per_symbol;
    d_subframe_length_symbols = BEIDOU_DNAV_PREAMBLE_PERIOD_SYMBOLS;

    // Setting samples of secondary code
    for (int32_t i = 0; i < BEIDOU_B1I_SECONDARY_CODE_LENGTH; i++)
		{
			if (BEIDOU_B1I_SECONDARY_CODE.at(i) == '1')
				{
					d_secondary_code_symbols[i] = 1;
				}
			else
				{
					d_secondary_code_symbols[i] = -1;
				}
		}

    // Setting samples of preamble code
    int32_t n = 0;
    for (int32_t i = 0; i < d_symbols_per_preamble; i++)
        {
    		int32_t m = 0;
            if (BEIDOU_DNAV_PREAMBLE.at(i) == '1')
                {
                    for (uint32_t j = 0; j < d_samples_per_symbol; j++)
                        {
                            d_preamble_samples[n] = d_secondary_code_symbols[m];
                            n++;
                            m++;
                            m = m % BEIDOU_B1I_SECONDARY_CODE_LENGTH;
                        }
                }
            else
                {
                    for (uint32_t j = 0; j < d_samples_per_symbol; j++)
                        {
                            d_preamble_samples[n] = -d_secondary_code_symbols[m];
                            n++;
                            m++;
                            m = m % BEIDOU_B1I_SECONDARY_CODE_LENGTH;
                        }
                }
        }

    d_subframe_symbols = static_cast<double *>(volk_gnsssdr_malloc(d_subframe_length_symbols * sizeof(double), volk_gnsssdr_get_alignment()));
    d_required_symbols = BEIDOU_DNAV_SUBFRAME_SYMBOLS*d_samples_per_symbol + d_samples_per_preamble;
    d_sample_counter = 0;
    d_stat = 0;
    d_preamble_index = 0;

    d_flag_frame_sync = false;

    d_TOW_at_current_symbol_ms = 0;
    Flag_valid_word = false;
    d_CRC_error_counter = 0;
    d_flag_preamble = false;
    d_channel = 0;
    flag_SOW_set = false;

}


beidou_b1i_telemetry_decoder_cc::~beidou_b1i_telemetry_decoder_cc()
{
	volk_gnsssdr_free(d_preamble_samples);
    volk_gnsssdr_free(d_secondary_code_symbols);
    volk_gnsssdr_free(d_subframe_symbols);

    if (d_dump_file.is_open() == true)
        {
            try
                {
                    d_dump_file.close();
                }
            catch (const std::exception &ex)
                {
                    LOG(WARNING) << "Exception in destructor closing the dump file " << ex.what();
                }
        }
}


void beidou_b1i_telemetry_decoder_cc::decode_bch15_11_01(int32_t *bits, int32_t *decbits)
{
    int bit, err, reg[4] = {1, 1, 1, 1};
    int errind[15] = {14, 13, 10, 12, 6, 9, 4, 11, 0, 5, 7, 8, 1, 3, 2};

    for (unsigned int i = 0; i < 15; i++)
        {
            decbits[i] = bits[i];
        }

    for (unsigned int i = 0; i < 15; i++)
        {
            bit = reg[3];
            reg[3] = reg[2];
            reg[2] = reg[1];
            reg[1] = reg[0];
            reg[0] = bits[i] * bit;
            reg[1] *= bit;
        }

    err = errind[reg[0] + reg[1]*2 + reg[2]*4 + reg[3]*8];

    if (err > 0)
        {
            decbits[err - 1] *= -1;
        }
}

void beidou_b1i_telemetry_decoder_cc::decode_word(
		int32_t word_counter,
		double* enc_word_symbols,
		int32_t* dec_word_symbols)
{
    int32_t bitsbch[30], first_branch[15], second_branch[15];

    if (word_counter == 1)
        {
            for (unsigned int j = 0; j < 30; j++)
	            {
            	    dec_word_symbols[j] = (int32_t)(enc_word_symbols[j] > 0) ? (1) : (-1);
	            }
         }
    else
        {
	        for (unsigned int r = 0; r < 2; r++)
	     		 {
	     		 	 for (unsigned int c = 0; c < 15; c++)
                        {
	     		 		 	 bitsbch[r*15 + c] = (int32_t)(enc_word_symbols[c*2 + r] > 0) ? (1) : (-1);
                        }
	     		 }

            decode_bch15_11_01(&bitsbch[0], first_branch);
            decode_bch15_11_01(&bitsbch[15], second_branch);

			for (unsigned int j = 0; j < 11; j++)
				{
				    dec_word_symbols[j] = first_branch[j];
				    dec_word_symbols[j + 11] = second_branch[j];
				}

			for (unsigned int j = 0; j < 4; j++)
				{
				    dec_word_symbols[j + 22] = first_branch[11 + j];
				    dec_word_symbols[j + 26] = second_branch[11 + j];
				}
        }


}


void beidou_b1i_telemetry_decoder_cc::decode_subframe(double *frame_symbols, int32_t frame_length)
{
	// 1. Transform from symbols to bits
    std::string data_bits;
    int32_t dec_word_bits[30];

    // Decode each word in subframe
    for(uint32_t ii = 0; ii < BEIDOU_DNAV_WORDS_SUBFRAME; ii++)
    {
    	// decode the word
    	decode_word((ii+1), &frame_symbols[ii*30], dec_word_bits);

    	// Save word to string format
        for (uint32_t jj = 0; jj < (BEIDOU_DNAV_WORD_LENGTH_BITS); jj++)
            {
        	    data_bits.push_back( (dec_word_bits[jj] > 0) ? ('1') : ('0') );
            }
    }

    d_nav.subframe_decoder(data_bits);

    // 3. Check operation executed correctly
    if (d_nav.flag_crc_test == true)
        {
    		LOG(INFO) << "BeiDou DNAV CRC correct in channel " << d_channel << " from satellite " << d_satellite;
        }
    else
        {
            LOG(INFO) << "BeiDou DNAV CRC error in channel " << d_channel << " from satellite " << d_satellite;
        }
    // 4. Push the new navigation data to the queues
    if (d_nav.have_new_ephemeris() == true)
        {
            // get object for this SV (mandatory)
            std::shared_ptr<Beidou_Dnav_Ephemeris> tmp_obj = std::make_shared<Beidou_Dnav_Ephemeris>(d_nav.get_ephemeris());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU DNAV Ephemeris have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << "New BEIDOU B1I DNAV message received in channel " << d_channel << ": ephemeris from satellite " << d_satellite << std::endl;
        }
    if (d_nav.have_new_utc_model() == true)
        {
            // get object for this SV (mandatory)
            std::shared_ptr<Beidou_Dnav_Utc_Model> tmp_obj = std::make_shared<Beidou_Dnav_Utc_Model>(d_nav.get_utc_model());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU DNAV UTC Model have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << "New BEIDOU B1I DNAV utc model message received in channel " << d_channel << ": UTC model parameters from satellite " << d_satellite << std::endl;
        }
    if (d_nav.have_new_iono() == true)
        {
            // get object for this SV (mandatory)
            std::shared_ptr<Beidou_Dnav_Iono> tmp_obj = std::make_shared<Beidou_Dnav_Iono>(d_nav.get_iono());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU DNAV Iono have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << "New BEIDOU B1I DNAV Iono message received in channel " << d_channel << ": UTC model parameters from satellite " << d_satellite << std::endl;
        }
    if (d_nav.have_new_almanac() == true)
        {
//            unsigned int slot_nbr = d_nav.i_alm_satellite_PRN;
//            std::shared_ptr<Beidou_Dnav_Almanac> tmp_obj = std::make_shared<Beidou_Dnav_Almanac>(d_nav.get_almanac(slot_nbr));
//            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "BEIDOU DNAV Almanac have been received in channel" << d_channel << " from satellite " << d_satellite << std::endl;
            std::cout << "New BEIDOU B1I DNAV almanac received in channel " << d_channel << " from satellite " << d_satellite << std::endl;
        }
}


void beidou_b1i_telemetry_decoder_cc::set_satellite(const Gnss_Satellite &satellite)
{
    d_satellite = Gnss_Satellite(satellite.get_system(), satellite.get_PRN());
    DLOG(INFO) << "Setting decoder Finite State Machine to satellite " << d_satellite;
    DLOG(INFO) << "Navigation Satellite set to " << d_satellite;
}


void beidou_b1i_telemetry_decoder_cc::set_channel(int channel)
{
    d_channel = channel;
    LOG(INFO) << "Navigation channel set to " << channel;
    // ############# ENABLE DATA FILE LOG #################
    if (d_dump == true)
        {
            if (d_dump_file.is_open() == false)
                {
                    try
                        {
                            d_dump_filename = "telemetry";
                            d_dump_filename.append(boost::lexical_cast<std::string>(d_channel));
                            d_dump_filename.append(".dat");
                            d_dump_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                            d_dump_file.open(d_dump_filename.c_str(), std::ios::out | std::ios::binary);
                            LOG(INFO) << "Telemetry decoder dump enabled on channel " << d_channel << " Log file: " << d_dump_filename.c_str();
                        }
                    catch (const std::ifstream::failure &e)
                        {
                            LOG(WARNING) << "channel " << d_channel << ": exception opening Beidou TLM dump file. " << e.what();
                        }
                }
        }
}


int beidou_b1i_telemetry_decoder_cc::general_work(int noutput_items __attribute__((unused)), gr_vector_int &ninput_items __attribute__((unused)),
    gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	int32_t corr_value = 0;
	int32_t preamble_diff = 0;

    Gnss_Synchro **out = reinterpret_cast<Gnss_Synchro **>(&output_items[0]);            // Get the output buffer pointer
    const Gnss_Synchro **in = reinterpret_cast<const Gnss_Synchro **>(&input_items[0]);  // Get the input buffer pointer

    Gnss_Synchro current_symbol;  //structure to save the synchronization information and send the output object to the next block
    //1. Copy the current tracking output
    current_symbol = in[0][0];
    d_symbol_history.push_back(current_symbol.Prompt_I);  //add new symbol to the symbol queue
    d_sample_counter++;                          //count for the processed samples
    consume_each(1);

    d_flag_preamble = false;

    if (d_symbol_history.size() > d_required_symbols)
        {
            //******* preamble correlation ********
            for (int i = 0; i < d_samples_per_preamble; i++)
                {
                    if (d_symbol_history.at(i) < 0)  // symbols clipping
                        {
                            corr_value -= d_preamble_samples[i];
                        }
                    else
                        {
                            corr_value += d_preamble_samples[i];
                        }
                }
        }

    //******* frame sync ******************
    if (d_stat == 0)  //no preamble information
        {
            if (abs(corr_value) >= d_samples_per_preamble)
                {
                    // Record the preamble sample stamp
                    d_preamble_index = d_sample_counter;
                    LOG(INFO) << "Preamble detection for BEIDOU B1I SAT " << this->d_satellite;
                    // Enter into frame pre-detection status
                    d_stat = 1;
                }
        }
    else if (d_stat == 1)  // possible preamble lock
        {
            if (abs(corr_value) >= d_samples_per_preamble)
                {
                    //check preamble separation
                    preamble_diff = static_cast<int32_t>(d_sample_counter - d_preamble_index);
                    if (abs(preamble_diff - d_preamble_period_samples) == 0)
                        {
                            //try to decode frame
                            LOG(INFO) << "Starting BeiDou DNAV frame decoding for BeiDou B1I SAT " << this->d_satellite;
                            d_preamble_index = d_sample_counter;  //record the preamble sample stamp
                            d_stat = 2;
                        }
                    else
                        {
                            if (preamble_diff > d_preamble_period_samples)
                                {
                                    d_stat = 0;  // start again
                                }
                            DLOG(INFO) << "Failed BeiDou DNAV frame decoding for BeiDou B1I SAT " << this->d_satellite;
                        }
                }
        }
    else if (d_stat == 2) // preamble acquired
        {
           if (d_sample_counter == d_preamble_index + static_cast<uint64_t>(d_preamble_period_samples))
                {
                    //******* SAMPLES TO SYMBOLS *******
                    if (corr_value > 0)  //normal PLL lock
                        {
                            int k = 0;
                            for (uint32_t i = 0; i < d_subframe_length_symbols; i++)
                                {
                            		d_subframe_symbols[i] = 0;
                            		//integrate samples into symbols
                                    for (uint32_t m = 0; m < d_samples_per_symbol; m++)
                                        {
                                    	    // because last symbol of the preamble is just received now!
                                    		d_subframe_symbols[i] += static_cast<float>(d_secondary_code_symbols[k]) * d_symbol_history.at(i * d_samples_per_symbol + m);
                                            k++;
                                            k = k % BEIDOU_B1I_SECONDARY_CODE_LENGTH;
                                        }
                                }
                        }
                    else  //180 deg. inverted carrier phase PLL lock
                        {
                            int k = 0;
                            for (uint32_t i = 0; i < d_subframe_length_symbols; i++)
                                {
                            		d_subframe_symbols[i] = 0;
                            		//integrate samples into symbols
                                    for (uint32_t m = 0; m < d_samples_per_symbol; m++)
                                        {
                                    	    // because last symbol of the preamble is just received now!
                                    		d_subframe_symbols[i] -= static_cast<float>(d_secondary_code_symbols[k]) * d_symbol_history.at(i * d_samples_per_symbol + m);
                                            k++;
                                            k = k % BEIDOU_B1I_SECONDARY_CODE_LENGTH;
                                        }
                                }
                        }

                    //call the decoder
                    decode_subframe(d_subframe_symbols, d_subframe_length_symbols);
                    if (d_nav.flag_crc_test == true)
                        {
                            d_CRC_error_counter = 0;
                            d_flag_preamble = true;               //valid preamble indicator (initialized to false every work())
                            d_preamble_index = d_sample_counter;  //record the preamble sample stamp (t_P)
                            if (!d_flag_frame_sync)
                                {
                                    d_flag_frame_sync = true;
                                    DLOG(INFO) << "BeiDou DNAV frame sync found for SAT " << this->d_satellite;
                                }
                        }
                    else
                        {
                            d_CRC_error_counter++;
                            d_preamble_index = d_sample_counter;  //record the preamble sample stamp
                            if (d_CRC_error_counter > CRC_ERROR_LIMIT)
                                {
                                    LOG(INFO) << "BeiDou DNAV frame sync lost for SAT " << this->d_satellite;
                                    d_flag_frame_sync = false;
                                    d_stat = 0;
                                    flag_SOW_set = false;
                                }
                        }
                }
        }

    // UPDATE GNSS SYNCHRO DATA
    //2. Add the telemetry decoder information
    if (this->d_flag_preamble == true and d_nav.flag_new_SOW_available == true)
        //update TOW at the preamble instant
        {
    		// Reporting sow as gps time of week
    		d_TOW_at_Preamble_ms = static_cast<uint32_t>((d_nav.d_SOW + 14) * 1000.0);
    		d_TOW_at_current_symbol_ms = d_TOW_at_Preamble_ms + static_cast<uint32_t>((d_required_symbols + 1) * BEIDOU_B1I_CODE_PERIOD_MS);
    		flag_SOW_set = true;
    		d_nav.flag_new_SOW_available = false;
        }
    else  //if there is not a new preamble, we define the TOW of the current symbol
        {
        	d_TOW_at_current_symbol_ms += static_cast<uint32_t>(BEIDOU_B1I_CODE_PERIOD_MS);
        }


    if (d_flag_frame_sync == true and flag_SOW_set == true)
        {
            current_symbol.Flag_valid_word = true;
        }
    else
        {
            current_symbol.Flag_valid_word = false;
        }

    current_symbol.PRN = this->d_satellite.get_PRN();
    current_symbol.TOW_at_current_symbol_ms = d_TOW_at_current_symbol_ms;

    if (d_dump == true)
        {
            // MULTIPLEXED FILE RECORDING - Record results to file
            try
                {
                    double tmp_double;
                    unsigned long int tmp_ulong_int;
                    tmp_double = d_TOW_at_current_symbol_ms;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_double), sizeof(double));
                    tmp_ulong_int = current_symbol.Tracking_sample_counter;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_ulong_int), sizeof(unsigned long int));
                    tmp_double = 0;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_double), sizeof(double));
                }
            catch (const std::ifstream::failure &e)
                {
                    LOG(WARNING) << "Exception writing observables dump file " << e.what();
                }
        }

    // remove used symbols from history
    if (d_symbol_history.size() > d_required_symbols)
        {
            d_symbol_history.pop_front();
        }
    //3. Make the output (copy the object contents to the GNURadio reserved memory)
    *out[0] = current_symbol;

    return 1;
}
