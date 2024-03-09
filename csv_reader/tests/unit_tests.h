#pragma once

//--gtest_catch_exceptions=0 --gtest_filter=Quasistationary.EulerWithMOC_step_inter

#include "../time_series/vector_timeseries.h"
#include "../time_series/csv_timeseries_readers.h"

TEST(CsvRead, Test1)
{
    string folder = "data/";
    vector<pair<string, string>>parameters =
    {
        { folder + "rho_in", "kg/m3" }/*,
        { folder + "visc_in", "mm^2/s-m^2/s"s },
        { folder + "p_in", "MPa-kPa"s },
        { folder + "Q_in", "m3/h-m3/s"s },
        { folder + "p_out", "MPa-kPa"s },
        { folder + "Q_out", "m3/h-m3/s"s }*/
    };

    pair<string, string> modeling_period = {
        "01.08.2021 00:00:00",
        "01.09.2021 00:00:00"
    };
    csv_timeseries_reader tags(parameters);

    auto tag_data = tags.read_csvs();

    vector_timeseries_t params(tag_data);

    string test_time = "10.08.2021 08:53:50";

    vector<double> inter = params(StringToUnix(test_time));

    ASSERT_NEAR(inter[0], StringToUnix(test_time), 5);
}