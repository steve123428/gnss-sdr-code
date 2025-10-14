#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <chrono>

namespace po = boost::program_options;

int UHD_SAFE_MAIN(int argc, char *argv[]) {
    // 인자 파싱
    std::string addr;
    double set_time;
    
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("addr", po::value<std::string>(&addr)->default_value("192.168.40.2"), "USRP address")
        ("time", po::value<double>(&set_time)->default_value(0.0), "Time to set at next PPS")
    ;
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return EXIT_SUCCESS;
    }
    
    // USRP 연결
    std::string usrp_args = "addr=" + addr + ",clock_source=external,time_source=external";
    std::cout << "Creating USRP with args: " << usrp_args << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(usrp_args);
    
    // GPS 시간 가져오기 (set_time이 0이면)
    if (set_time == 0.0) {
        uhd::sensor_value_t gps_time_sensor = usrp->get_mboard_sensor("gps_time", 0);
        set_time = static_cast<double>(gps_time_sensor.to_int()) + 2.0;
        std::cout << "Auto GPS time: " << gps_time_sensor.to_int() << std::endl;
    }
    
    std::cout << "Setting time to " << set_time << " at next PPS..." << std::endl;
    
    // 다음 PPS에 시간 설정
    usrp->set_time_next_pps(uhd::time_spec_t(set_time));
    
    // PPS 대기
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 확인
    double current_time = usrp->get_time_now().get_real_secs();
    std::cout << "USRP time now: " << current_time << std::endl;
    std::cout << "Synchronization complete!" << std::endl;
    
    return EXIT_SUCCESS;
}