#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/stream.hpp>
#include <uhd/types/metadata.hpp>
#include <uhd/utils/thread.hpp>


#include <fstream>
#include <iostream>
#include <vector>
#include <complex>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>

static std::atomic<bool> stop_flag{false};

void sigint_handler(int) {
    stop_flag = true;
}

int main() {
    signal(SIGINT, sigint_handler);

    const std::string filename =
        "/home/suhjw/nvme_data/cleanStatic.bin";

    /* ===============================
     * 1. USRP CONFIG
     * =============================== */
    auto usrp = uhd::usrp::multi_usrp::make("type=x300");

    usrp->set_clock_source("external");
    usrp->set_time_source("external");

    double sample_rate = 25e6;
    double center_freq = 1.57542e9;

    usrp->set_tx_rate(sample_rate);
    usrp->set_tx_freq(center_freq);
    usrp->set_tx_gain(30);
    usrp->set_tx_bandwidth(sample_rate);
    usrp->set_tx_antenna("TX/RX");

    std::cout << "USRP configured\n"
              << "  Fs = " << usrp->get_tx_rate() << "\n"
              << "  Fc = " << usrp->get_tx_freq() << "\n";

    /* ===============================
     * 2. TX STREAM
     * =============================== */
    uhd::stream_args_t stream_args("sc16", "sc16");
    stream_args.args["send_buff_size"] = "33554432"; // 32 MB
    stream_args.args["num_send_frames"] = "64";

    auto tx_stream = usrp->get_tx_stream(stream_args);
    const size_t samps_per_buff = tx_stream->get_max_num_samps();

    /* ===============================
     * 3. RING BUFFER
     * =============================== */
    constexpr size_t RB_SIZE = 256;

    std::vector<std::vector<std::complex<int16_t>>> ring(RB_SIZE);
    for (auto& b : ring)
        b.resize(samps_per_buff);

    std::atomic<size_t> w_idx{0}, r_idx{0};

    /* ===============================
     * 4. FILE READER THREAD
     * =============================== */
    std::thread reader([&]() {
        std::ifstream infile(filename, std::ios::binary);
        if (!infile) {
            std::cerr << "Cannot open " << filename << "\n";
            stop_flag = true;
            return;
        }

        while (!stop_flag) {
            size_t next = (w_idx + 1) % RB_SIZE;
            if (next == r_idx) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                continue; // buffer full
            }

            infile.read(reinterpret_cast<char*>(ring[w_idx].data()),
                        ring[w_idx].size() * sizeof(std::complex<int16_t>));

            size_t nread =
                infile.gcount() / sizeof(std::complex<int16_t>);

            if (nread == 0) {
                stop_flag = true;
                break;
            }

            if (nread < ring[w_idx].size()) {
                ring[w_idx].resize(nread);
            }

            w_idx = next;
        }

        infile.close();
    });

    /* ===============================
     * 5. TX THREAD
     * =============================== */
    std::thread tx([&]() {
        uhd::set_thread_priority_safe();

        uhd::tx_metadata_t md;
        md.start_of_burst = true;
        md.end_of_burst = false;
        md.has_time_spec = true;

        usrp->set_time_now(uhd::time_spec_t(0.0));
        md.time_spec = usrp->get_time_now() + uhd::time_spec_t(0.1);

        while (!stop_flag || r_idx != w_idx) {
            if (r_idx == w_idx) {
                std::this_thread::sleep_for(std::chrono::microseconds(5));
                continue;
            }

            tx_stream->send(ring[r_idx].data(),
                            ring[r_idx].size(),
                            md);

            md.start_of_burst = false;
            md.has_time_spec = false;

            r_idx = (r_idx + 1) % RB_SIZE;
        }

        md.end_of_burst = true;
        std::complex<int16_t> dummy;
        tx_stream->send(&dummy, 0, md);
    });

    reader.join();
    tx.join();

    std::cout << "Replay finished cleanly\n";
    return 0;
}
