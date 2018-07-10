#ifndef _GN_PERF_STAT_ACC_HPP_
#define _GN_PERF_STAT_ACC_HPP_

class stat_acc
{
    double sum, sum2, min, max;
    unsigned long long cnt;

public:
    inline unsigned long long sample_count() const
    { return cnt; }

    template<typename T>
    T sample(T value)
    {
        sum += value;
        sum2 += value * value;
        min = std::min(value, min);
        max = std::max(value, max);

        cnt++;
        return value;
    }

    template<typename Callable>
    std::string summary(bool raw_output, bool &output_header, const std::string &name, Callable cb)
    {
        std::stringstream ss;
        auto avg = sum / cnt,
             sdd = (sum2 - sum / cnt * sum / cnt) / (cnt - 1);

        if (!raw_output && !output_header)
        {
            ss << std::setw(10) << "mes" << std::setw(0) << "\t";
            ss << std::setw(10) << "avg" << std::setw(0) << "\t";
            ss << std::setw(10) << "sdd" << std::setw(0) << "\t";
            ss << std::setw(10) << "sd%" << std::setw(0) << "\t";
            ss << std::setw(10) << "min" << std::setw(0) << "\t";
            ss << std::setw(10) << "max" << std::setw(0) << std::endl;
            output_header = true;
        }

        auto w(raw_output ? 0 : 10);
        ss << std::setw(w) << name << std::setw(0) << "\t";
        ss << std::setw(w) << cb(avg) << std::setw(0) << "\t";
        ss << std::setw(w) << sdd / avg * cb(avg) << std::setw(0) << "\t";
        ss << std::setw(w) << sdd / avg * 100.0 << std::setw(0) << "\t";
        ss << std::setw(w) << cb(min) << std::setw(0) << "\t";
        ss << std::setw(w) << cb(max);

        return ss.str();
    }

    inline std::string summary(bool raw_output, bool &output_header, const std::string &name)
    {
        return summary(raw_output, output_header, name, [](auto x) { return x; });
    }

    inline stat_acc()
        : sum(0.0),
        sum2(0.0),
        min(std::numeric_limits<double>::max()),
        max(std::numeric_limits<double>::min()),
        cnt(0)
    {
    }
};

#endif /* _GN_PERF_STAT_ACC_HPP_ */
