#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <stdexcept>
#include <numeric>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/date_time.hpp>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <unistd.h>
#include <boost/thread.hpp>
#endif

namespace asio = boost::asio;
using namespace std;
using std::chrono::hours;
using std::chrono::system_clock;

static constexpr char kLogFileAll[] = "total temperature report.log";
static constexpr char kLogFileHour[] = "temperature by hour report.log";
static constexpr char kLogFileDay[] = "temperature by day report.log";
static constexpr hours kLogHour(1);
static constexpr hours kLogDay(24);
static constexpr hours kLogMounth(24 * 30);

class SerialPort {
public:
    SerialPort(const std::string& port_name) : port_name_(port_name), port_(new asio::serial_port(*io_service_.get(), port_name_)) {}

    void Open() 
    {
        port_->set_option(asio::serial_port_base::baud_rate(9600));

        if (!port_->is_open()) {
            throw std::runtime_error("Failed to open serial port: " + port_name_);
        }

        ConfiguratePort();
    }

    size_t ReadSome(char* data, size_t max_length)
    {
        auto sz = port_->read_some(asio::buffer(data, max_length));
        return sz;
    }

private:

    void ConfiguratePort()
    {
        //https://stackoverflow.com/questions/6068655/what-are-the-functions-i-need-use-for-serial-communication-in-vc
#ifdef _WIN32
        HANDLE serialHandle = port_->native_handle();
        if (serialHandle == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to get serial port handle " + port_name_);
        }

        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        if (!GetCommState(serialHandle, &dcbSerialParams)) {
            throw std::runtime_error("Failed to get serial port state" + port_name_);
        }

        
        dcbSerialParams.BaudRate = CBR_9600;       // Baud rate
        dcbSerialParams.ByteSize = 8;              // Data bits
        dcbSerialParams.Parity = NOPARITY;         // Parity
        dcbSerialParams.StopBits = ONESTOPBIT;     // Stop bits
        dcbSerialParams.fOutxCtsFlow = false;      // No CTS flow control
        dcbSerialParams.fOutxDsrFlow = false;      // No DSR flow control
        dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE; // Disable DTR control
        dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE; // Disable RTS control

        
        if (!SetCommState(serialHandle, &dcbSerialParams)) {
            throw std::runtime_error("Failed to set serial port state" + port_name_);
        }
#endif

    }

    std::string port_name_;
    std::unique_ptr<asio::io_service> io_service_;
    std::unique_ptr<asio::serial_port> port_;
};

class FileWriter {
public:
    FileWriter(const std::string& filename) {}

    void Append(const std::string& filename, const std::string& line) {
        ofstream file(filename, ios::app);
        if (file.is_open()) {
            file << line << '\n';
        }
        else {
            cerr << "Failed to open file: " << filename << endl;
        }
    }

    void ClearEntries(const std::string& filename, const system_clock::time_point& threshold) {
        ifstream input_file(filename);
        if (!input_file.is_open()) {
            cerr << "Failed to open log file: " << filename << endl;
            return;
        }

        vector<string> valid_lines;

       
        string line;
        while (getline(input_file, line)) {
            string timestamp_str = line.substr(1, kTimestampLength);
            tm timestamp = {};
            istringstream(timestamp_str) >> get_time(&timestamp, kTimestampFormat);
            auto entry_time = system_clock::from_time_t(mktime(&timestamp));

            
            if (entry_time >= threshold) {
                valid_lines.push_back(line);
            }
        }

        input_file.close();

        
        ofstream output_file(filename);
        if (!output_file.is_open()) {
            cerr << "Failed to open log file for writing: " << filename << endl;
            return;
        }

        // Write valid lines back
        for (const auto& valid_line : valid_lines) {
            output_file << valid_line << '\n';
        }

        
        output_file.close();
    }



private:
    static constexpr int kTimestampLength = 19;
    static constexpr const char* kTimestampFormat = "%Y/%m/%d %H:%M:%S";
};

class TemperatureLogger {
public:
    TemperatureLogger(const std::string& port_name)
        : port_(port_name), file_writer_all_(kLogFileAll), last_all_log_update_(system_clock::now()) {}

    void Run() {
        try {
            port_.Open();
        }
        catch (const std::exception& e) {
            cerr << "Error: " << e.what() << endl;
            throw std::runtime_error("Failed open com");
        }
        
        while (true) {
            ReadTemperature();
            UpdateLogs();
            std::this_thread::sleep_for(std::chrono::seconds(kSleepSeconds));
        }
    }

private:
    SerialPort port_;
    std::vector<double> hourly_temperatures_;
    double daily_total_temperature_ = 0.0;
    int daily_temperature_count_ = 0;
    system_clock::time_point last_hourly_update_ = system_clock::now();
    system_clock::time_point last_daily_update_ = system_clock::now();
    system_clock::time_point last_monthly_update_ = system_clock::now();
    system_clock::time_point last_year_update_ = system_clock::now();
    system_clock::time_point last_all_log_update_;
    FileWriter file_writer_all_;

    static constexpr int kSleepSeconds = 2;

    void ReadTemperature() {
        static char data[256]; 
        size_t bytes_read = port_.ReadSome(data, sizeof(data));
        
        if (bytes_read > 0) {
            auto temperature = stod(data);
            LogTemperatureToFile(temperature, kLogFileAll);
            hourly_temperatures_.push_back(temperature);
            daily_total_temperature_ += temperature;
            ++daily_temperature_count_;
        }
    }

    void UpdateLogs() {
        auto current_time = system_clock::now();
        if (current_time - last_all_log_update_ > kLogDay) {
            file_writer_all_.ClearEntries(kLogFileAll, current_time - hours(kLogDayHours));
            last_all_log_update_ = current_time;
        }

        
        if (current_time - last_hourly_update_ > kLogHour) {
            UpdateHourlyLogToFile();
            last_hourly_update_ = current_time;
        }

        
        if (current_time - last_monthly_update_ > kLogMounth) {
            file_writer_all_.ClearEntries(kLogFileHour, current_time - hours(kLogMounthHours));
            last_monthly_update_ = std::chrono::system_clock::now();
        }
        

        if (current_time - last_daily_update_ > kLogDay) {
            UpdateDailyLogToFile();
            last_daily_update_ = current_time;
        }

        if (current_time - last_year_update_ > hours(kLogYearHours)) {
            file_writer_all_.ClearEntries(kLogFileDay, current_time - hours(kLogYearHours));
            last_year_update_ = current_time;
        }
        
    }

    static constexpr int kLogDayHours = 24;
    static constexpr int kLogMounthHours = 24 * 30;
    static constexpr int kLogYearHours = 24 * 365;
    static constexpr const char* kTimestampFormat = "%Y/%m/%d %H:%M:%S";

    void LogTemperatureToFile(double temperature, const std::string& filename) {
        auto now = std::chrono::system_clock::now();
        time_t currentTime = std::chrono::system_clock::to_time_t(now);

        struct std::tm localTime;
#ifdef _WIN32
        localtime_s(&localTime, &currentTime);
#else
        localtime_r(&currentTime, &localTime);
#endif
        std::stringstream log_entry;
        log_entry << "|" << std::put_time(&localTime, kTimestampFormat) << "| " << std::to_string(temperature) << " C";
        
        FileWriter file_writer(filename);
        file_writer.Append(filename, log_entry.str());
    }

    void UpdateHourlyLogToFile() {
        if (!hourly_temperatures_.empty()) {
            double sum = std::accumulate(hourly_temperatures_.begin(), hourly_temperatures_.end(), 0.0);
            double average = sum / hourly_temperatures_.size();
            LogTemperatureToFile(average, kLogFileHour);
            hourly_temperatures_.clear();
        }
    }

    void UpdateDailyLogToFile() {
        if (daily_temperature_count_ > 0) {
            double average = daily_total_temperature_ / daily_temperature_count_;
            LogTemperatureToFile(average, kLogFileDay);
            daily_total_temperature_ = 0.0;
            daily_temperature_count_ = 0;
        }
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <serial_port>" << endl;
        return 1;
    }

    try {
        TemperatureLogger logger(argv[1]);
        logger.Run();
    }
    catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
