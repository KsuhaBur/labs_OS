#define _CRT_SECURE_NO_WARNINGS
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
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "database.h"

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
namespace beast = boost::beast;
namespace http = beast::http;
namespace pt = boost::property_tree;

static constexpr char total_temp[] = "total_temp";
static constexpr char hout_temp[] = "hour_temp";
static constexpr char day_temp[] = "day_temp";
static constexpr hours kLogHour(1);
static constexpr hours kLogDay(24);
static constexpr hours kLogMounth(24 * 30);

using namespace std;

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

    bool addTemperatureData(const std::string& timestamp, double temperature) {
        try {
            pt::ptree data;
            data.put("timestamp", timestamp);
            data.put("temperature", temperature);

            std::ostringstream oss;
            pt::write_json(oss, data);
            std::string dataStr = oss.str();

            asio::io_context io_context;
            asio::ip::tcp::resolver resolver(io_context);
            beast::tcp_stream stream(io_context);
            auto const results = resolver.resolve("localhost", "3000");
            stream.connect(results);
            http::request<http::string_body> request(http::verb::post, "/addTemperatureData", 11);
            request.set(http::field::host, "localhost");
            request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            request.set(http::field::content_type, "application/json");
            request.body() = dataStr;

            std::cout << "Sending JSON data: " << dataStr << std::endl;

            http::write(stream, request);
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> response;
            http::read(stream, buffer, response);

            if (response.result() == http::status::ok) {
                std::cout << "Data added successfully" << std::endl;
                return true;
            }
            else {
                std::cout << "Failed to add data" << std::endl;
                return false;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
    }
    
    void Append(const std::string& filename, const std::string& line, double temp = 0) {
        
    }

    void ClearEntries(const std::string& filename, const system_clock::time_point& threshold) {
        char timestamp[20];
        auto thresholdTime = std::chrono::system_clock::to_time_t(threshold);

        struct std::tm localTime;
#ifdef _WIN32
        localtime_s(&localTime, &thresholdTime);
#else
        localtime_r(&thresholdTime, &localTime);
#endif

        std::strftime(timestamp, sizeof(timestamp), kTimestampFormat, &localTime);

        std::string deletePrompt = "DELETE FROM " + filename + " WHERE timestamp < '" + timestamp + "';";

        auto status = sqlite3_exec(db, deletePrompt.c_str(), 0, 0, 0);
        if (status) {
            std::cerr << "Failed to clear old entries in the database: " << sqlite3_errmsg(db) << std::endl;
        }
    }

    

private:
    static constexpr int kTimestampLength = 19;
    static constexpr const char* kTimestampFormat = "%Y-%m-%d %H:%M:%S";
};

class TemperatureLogger {
public:
    TemperatureLogger(const std::string& port_name)
        : port_(port_name), file_writer_all_(total_temp), last_all_log_update_(system_clock::now()) {}

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
            LogTemperatureToFile(temperature, total_temp);
            hourly_temperatures_.push_back(temperature);
            daily_total_temperature_ += temperature;
            ++daily_temperature_count_;
        }
    }

    void UpdateLogs() {
        auto current_time = system_clock::now();
        if (current_time - last_all_log_update_ > kLogDay) {
            file_writer_all_.ClearEntries(total_temp, current_time - hours(kLogDayHours));
            last_all_log_update_ = current_time;
        }

        
        if (current_time - last_hourly_update_ > kLogHour) {
            UpdateHourlyLogToFile();
            last_hourly_update_ = current_time;
        }

        
        if (current_time - last_monthly_update_ > kLogMounth) {
            file_writer_all_.ClearEntries(hout_temp, current_time - hours(kLogMounthHours));
            last_monthly_update_ = std::chrono::system_clock::now();
        }
        

        if (current_time - last_daily_update_ > kLogDay) {
            UpdateDailyLogToFile();
            last_daily_update_ = current_time;
        }

        if (current_time - last_year_update_ > hours(kLogYearHours)) {
            file_writer_all_.ClearEntries(day_temp, current_time - hours(kLogYearHours));
            last_year_update_ = current_time;
        }
        
    }

    static constexpr int kLogDayHours = 24;
    static constexpr int kLogMounthHours = 24 * 30;
    static constexpr int kLogYearHours = 24 * 365;
    static constexpr const char* kTimestampFormat = "%Y-%m-%d %H:%M:%S";

    //+
    void LogTemperatureToFile(double temperature, const std::string& filename) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm* ptm = std::localtime(&now);

        char timestamp[20];
        std::strftime(timestamp, sizeof(timestamp), kTimestampFormat, ptm);

        std::stringstream insertQuery;
        insertQuery << "INSERT INTO " << filename << " (timestamp, temperature) VALUES ('" << timestamp << "', " << temperature << ");";

        auto  status = sqlite3_exec(db, insertQuery.str().c_str(), 0, 0, 0);
        if (status) {
            cerr << "Failed to insert data into the database" << endl;
        }

    }

    void UpdateHourlyLogToFile() {
        if (!hourly_temperatures_.empty()) {
            double sum = std::accumulate(hourly_temperatures_.begin(), hourly_temperatures_.end(), 0.0);
            double average = sum / hourly_temperatures_.size();
            LogTemperatureToFile(average, hout_temp);
            hourly_temperatures_.clear();
        }
    }

    void UpdateDailyLogToFile() {
        if (daily_temperature_count_ > 0) {
            double average = daily_total_temperature_ / daily_temperature_count_;
            LogTemperatureToFile(average, day_temp);
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
