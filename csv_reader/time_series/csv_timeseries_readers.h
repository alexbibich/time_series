#include <ctime>

#include <iomanip>
#include <iostream>
#include <chrono>
#include <map>
#include <fstream>

#include <fixed/fixed.h>
#include <pde_solvers/pde_solvers.h>

using std::ifstream;
using std::map;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

using std::runtime_error;
using std::make_pair;

using namespace pde_solvers;

/// @brief Перевод UNIX времени в строку формата dd:mm:yyyy HH:MM:SS
/// @param t время UNIX
/// @return строку формата dd:mm:yyyy HH:MM:SS
string UnixToString(time_t t) {

    struct tm tm;
    gmtime_s(&tm, &t);
    char buffer[200];
    strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &tm);
    return string(buffer);
}

/// @brief Перевод строки формата dd:mm:yyyy HH:MM:SS в переменную UNIX времени
/// @param source строка формата dd:mm:yyyy HH:MM:SS
/// @return время UNIX
std::time_t StringToUnix(const std::string& source)
{
    using namespace std::chrono;
    using namespace std::literals::string_literals;
    auto in = std::istringstream(source);
    auto tp = sys_seconds{};
    in >> parse("%d.%m.%Y %H:%M:%S"s, tp);
    time_t result = system_clock::to_time_t(tp);
    //string s = UnixToString(result); // для проверки
    return result;
}

/// @brief Перевод из строкого типа в тип double
/// @param str Переменная строкового типа
/// @param delim разделитель целой и дробной частей
/// @return Переменная типа double
inline double str2double(const string& str, char delim = '.') 
{
    if (delim == '.') {
        stringstream ss;
        ss.str(str);
        double result;
        ss >> result;
        return result;
    }
    else {
        size_t i = str.find(delim);
        if (i == std::string::npos) {
            stringstream ss;
            ss.str(str);
            double result;
            ss >> result;
            return result;
        }
        else {
            string str_ = str;
            str_[i] = '.';
            stringstream ss;
            ss.str(str_);
            double result;
            ss >> result;
            return result;
        }

    }
}

/// @brief Деление строки на подстроки по знаку
/// @param str_to_split Изначальная строка
/// @param delimeter Делитель
/// @return Вектор подстрок
vector<string> split_str(string& str_to_split, char delimeter)
{
    stringstream str_stream(str_to_split);
    vector<string> split_output;
    string str;
    while (getline(str_stream, str, delimeter))
    {
        split_output.push_back(str);
    }

    return split_output;
}

/// @brief Перевод единиц измерения
class converter_dimension
{
public:

    /// @brief Пересчёт единиц измерений
    /// @param value Текущее значение
    /// @param c Коэффициенты перевода
    /// @return Значение параметра в заданных единицах измерения
    static double convert_dimension(double value, std::pair<double, double> c = make_pair(1.0, 0.0))
    {
        return value / c.first - c.second;
    };

    /// @brief Перевод единиц измерения
    /// @param value Текущее значение
    /// @param dimension Инструкция перевода
    /// @return Значение в требуемых единицах измерения
    double convert(double value, const string& dimension) const
    {
        auto it = units.find(dimension);
        if (it != units.end()) {
            value = convert_dimension(value, it->second);
        }

        return value;
    }

private:
    /// @brief Коэффициенты для перевода единиц
    const map<string, std::pair<double, double>> units{
    {"m3/s", {1.0, 0}},
    {"m3/h-m3/s", {1 / 3600, 0.0}},
    {"m3/h", {3600, 0.0}},
    {"C", {1.0, -KELVIN_OFFSET}},
    {"K", {1.0, 0.0}},
    {"kgf/cm2(e)", {1 / TECHNICAL_ATMOSPHERE, -ATMOSPHERIC_PRESSURE / TECHNICAL_ATMOSPHERE}},
    {"kgf/cm2", {1 / TECHNICAL_ATMOSPHERE, -ATMOSPHERIC_PRESSURE / TECHNICAL_ATMOSPHERE}},
    {"kgf/cm2(a)", {1 / TECHNICAL_ATMOSPHERE, 0.0}},
    {"MPa", {1e-6, 0.0}},
    {"MPa-kPa", {1e3, 0.0}},
    {"mm^2/s-m^2/s", {1e-6, 0.0}},
    {"mm^2/s", {1e6, 0.0}},
    };
};

/// @brief Чтение параметров из исторических данных для нескольких тегов
class csv_timeseries_reader 
{
public:

    /// @brief Конструктор
    /// @param data Вектор пар, из которых первый элемент название тега,
    /// а второй - инструкция для перевода единиц измерения
    csv_timeseries_reader(const vector<pair<string, string>>& data)
        :filename_dim{ data }
    {

    };

    

    /// @brief Чтение параметров для заданного периода
    /// @param unixtime_from Начало периода задаётся строкой формата dd:mm:yyyy HH:MM:SS
    /// @param unixtime_to Конец периода задаётся строкой формата dd:mm:yyyy HH:MM:SS
    /// @return возвращает временной ряд в формате 
    /// для хранения в vector_timeseries_t
    vector<pair<vector<time_t>, vector<double>>> read_csvs(const string& unixtime_from, const string& unixtime_to) const
    {
        time_t start_period = StringToUnix(unixtime_from);
        time_t end_period = StringToUnix(unixtime_to);

        return read_csvs(start_period, end_period);
    }

    /// @brief Чтение параметров для заданного периода
    /// @param start_period Начало периода задаётся типом данных time_t
    /// @param end_period Конец периода задаётся типом данных time_t
    /// @return возвращает временной ряд в формате 
    /// для хранения в vector_timeseries_t
    vector<pair<vector<time_t>, vector<double>>> read_csvs(
        time_t start_period = std::numeric_limits<time_t>::min(), 
        time_t end_period = std::numeric_limits<time_t>::max()) const
    {
        vector<pair<vector<time_t>, vector<double>>> data;


        string extension = ".csv";

        for (size_t i = 0; i < filename_dim.size(); i++)
        {
            data.push_back(read(filename_dim[i].first + extension, filename_dim[i].second, start_period, end_period));
        }

        return data;
    };

    /// @brief Чтение одного файла 
    /// @param filename Название файла
    /// @param dimension Инструкция перевода 
    /// единиц измерения
    /// @param time_begin Начало периода
    /// @param time_end Конец периода
    /// @return Временной ряд
    static pair<vector<time_t>, vector<double>> read(const string& filename, const string& dimension, time_t time_begin = std::numeric_limits<time_t>::min(),
        time_t time_end = std::numeric_limits<time_t>::max())
    {

        ifstream infile(filename);
        if (!infile)
            throw runtime_error("file is not exist");

        vector<time_t> t;
        vector<double> x;

        std::string line_from_file;
        std::vector<std::string> split_line_to_file;
        while (getline(infile, line_from_file))
        {
            split_line_to_file = split_str(line_from_file, ';');
            const auto& date = split_line_to_file[0];
            time_t ut = StringToUnix(date);

            if (ut < time_begin) {
                continue;
            }
            if (ut > time_end) {
                break;
            }

            double value = str2double(split_line_to_file[1], ',');

            converter_dimension converter;
            value = converter.convert(value, dimension);


            t.push_back(ut);
            x.push_back(value);

        }

        return make_pair(t, x);
    }

private:
    /// @brief Вектор пар, из которых первый элемент название тега,
    /// а второй - инструкция для перевода единиц измерения
    const vector<pair<string, string>>& filename_dim;
};

class csv_tag_reader
{
public:
    /// @brief Конструктор
    /// @param data Название тега и инструкция перевода единиц измерения
    csv_tag_reader(const pair<string, string>& data)
        :tagname_dim{ data }
    {

    };

    /// @brief Чтение параметра для заданного периода
    /// @param unixtime_from Начало периода задаётся строкой формата dd:mm:yyyy HH:MM:SS
    /// @param unixtime_to Конец периода задаётся строкой формата dd:mm:yyyy HH:MM:SS
    /// @return возвращает временной ряд в формате 
    /// для хранения в vector_timeseries_t
    pair<vector<time_t>, vector<double>> read_csv(const string& unixtime_from, const string& unixtime_to) const
    {
        time_t start_period = StringToUnix(unixtime_from);
        time_t end_period = StringToUnix(unixtime_to);

        return read_csv(start_period, end_period);
    }

    /// @brief Чтение параметров для заданного периода
    /// @param start_period Начало периода задаётся типом данных time_t
    /// @param end_period Конец периода задаётся типом данных time_t
    /// @return возвращает временной ряд в формате 
    /// для хранения в vector_timeseries_t
    pair<vector<time_t>, vector<double>> read_csv(
        time_t start_period = std::numeric_limits<time_t>::min(),
        time_t end_period = std::numeric_limits<time_t>::max()) const
    {
        pair<vector<time_t>, vector<double>> data;

        string extension = ".csv";

        data = csv_timeseries_reader::read(tagname_dim.first, tagname_dim.second, start_period, end_period);

        return data;
    };

private:
    /// @brief Название тега и инструкция перевода единиц измерения
    const pair<string, string>& tagname_dim;
};