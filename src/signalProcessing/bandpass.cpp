#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include "aqmsDutyReviewBackend/signalProcessing/bandpass.hpp"

namespace
{

std::vector< std::array<double, 3> > bs40
{
     std::array<double, 3> { 0.6734790477138246, 1.3469580954276492, 0.6734790477138246  },
     std::array<double, 3> { 1.0, 0.0, -1.0  },
     std::array<double, 3> { 1.0, -2.0, 1.0  }
};

std::vector< std::array<double, 3> > as40
{
     std::array<double, 3> { 1.0, 1.6484753179477056, 0.7338442895664062  },
     std::array<double, 3> { 1.0, -0.19991468011753089, -0.6681786379192985  },
     std::array<double, 3> { 1.0, -1.918993448174642, 0.9249638747061851  }
};

std::vector< std::array<double, 3> > bs80
{
     std::array<double, 3> { 0.8214636347864291, 1.6429272695728583, 0.8214636347864291  },
     std::array<double, 3> { 1.0, 0.0, -1.0  },
     std::array<double, 3> { 1.0, -2.0, 1.0  }
};

std::vector< std::array<double, 3> > as80
{
     std::array<double, 3> { 1.0, 1.8322493755567273, 0.8551240027931296  },
     std::array<double, 3> { 1.0, -0.107703445133422, -0.8206787908286602  },
     std::array<double, 3> { 1.0, -1.9600400538405367, 0.9615546705021153  }
};

std::vector< std::array<double, 3> > bs100
{
     std::array<double, 3> { 0.7065679722682777, 1.4131359445365554, 0.7065679722682777  },
     std::array<double, 3> { 1.0, 0.0, -1.0  },
     std::array<double, 3> { 1.0, -2.0, 1.0  }
};

std::vector< std::array<double, 3> > as100
{
     std::array<double, 3> { 1.0, 1.647913599700461, 0.7329342638536966  },
     std::array<double, 3> { 1.0, -0.24355497400690285, -0.7028117712403572  },
     std::array<double, 3> { 1.0, -1.9681772087145875, 0.9691512708948594  }
};

std::vector< std::array<double, 3> > bs200
{
     std::array<double, 3> { 0.717921056028987, 1.435842112057974, 0.717921056028987  },
     std::array<double, 3> { 1.0, 0.0, -1.0  },
     std::array<double, 3> { 1.0, -2.0, 1.0  }
};

std::vector< std::array<double, 3> > as200
{
     std::array<double, 3> { 1.0, 1.6477316110123414, 0.7326354764117732  },
     std::array<double, 3> { 1.0, -0.2584223068197652, -0.7146105533645492  },
     std::array<double, 3> { 1.0, -1.9841892720072534, 0.9844343905181866  }
};

std::vector< std::array<double, 3> > bs500
{
     std::array<double, 3> { 0.7723380978421511, 1.5446761956843023, 0.7723380978421511  },
     std::array<double, 3> { 1.0, 0.0, -1.0  },
     std::array<double, 3> { 1.0, -2.0, 1.0  }
};

std::vector< std::array<double, 3> > as500
{
     std::array<double, 3> { 1.0, 1.7229839217145448, 0.7788929286543191  },
     std::array<double, 3> { 1.0, -0.21821025731826826, -0.7706599046098872  },
     std::array<double, 3> { 1.0, -1.9936996438745507, 0.9937390141821811  }
};

std::vector< std::array<double, 3> > bs1000
{
     std::array<double, 3> { 0.5258784146071691, 1.0517568292143382, 0.5258784146071691  },
     std::array<double, 3> { 1.0, 0.0, -1.0  },
     std::array<double, 3> { 1.0, -2.0, 1.0  }
};

std::vector< std::array<double, 3> > as1000
{
     std::array<double, 3> { 1.0, 1.2505393112081853, 0.5459198908866272  },
     std::array<double, 3> { 1.0, -0.48771304484689926, -0.5075484297228321  },
     std::array<double, 3> { 1.0, -1.9968550812497046, 0.9968649403993294  }
};


/// Biquadratic section
/// Repesent filter as B = gain*[1, b1, b2] and [1, a1, a2]
struct Section 
{
    //NOLINTBEGIN(bugprone-easily-swappable-parameters)
    Section(const std::array<double, 3> &b,
            const std::array<double, 3> &a)
    //NOLINTEND(bugprone-easily-swappable-parameters)
    {
        if (b[0] == 0)
        {
            throw std::invalid_argument("b0 cannot be 0");
        }
        if (a[0] == 0)
        {
            throw std::invalid_argument("a0 cannot be 0");
        }
        double b0In = b[0];
        double b1In = b[1];
        double b2In = b[2];
        const double a0In = a[0];
        double a1In = a[1];
        double a2In = a[2];
        //  [b0, b1, b2]    1/a0 [b0, b1, b2]    [b0/a0, b1/a0, b2/a0]
        //  ------------ -> ----------------- -> ---------------------
        /// [a0, a1, a2]    1/a0 [a0, a1, a2]     [1, a1/a0, a2/a0]
        if (a0In != 1)
        {
            a1In = a1In/a0In;
            a2In = a2In/a0In;
            b0In = b0In/a0In;
            b1In = b1In/a0In;
            b2In = b2In/a0In;
        }
        /// b0 is "gain" in this implementation
        gain = b0In;
        b1 = b1In/gain;
        b2 = b2In/gain;
        a1 = a1In;
        a2 = a2In;
        s0 = 0; 
        s1 = 0; 
        s2 = 0;  
    }
    /// Applies filter
    /// https://ccrma.stanford.edu/%7Ejos/filters/Biquad_Software_Implementations.html
    [[nodiscard]] double operator()(const double x)
    {
        // Denominator
        double A = gain*x;
        A -= a1*s1;
        A -= a2*s2;
        s0 = A;
        // Numerator
        A += b1*s1;
        const double output = b2*s2 + A;
        // Finish updating state
        s2 = s1;
        s1 = s0;
        return output;
    }
    /// Reset initial conditions
    void resetInitialConditions()
    {
        s1 = 0;
        s2 = 0;
        s0 = 0;
    }
//private:
    double a2; 
    double a1; 
    double b1; 
    double b2; 
    double gain;
    double s1{0};
    double s2{0};
    double s0{0};
};

struct SecondOrderSections
{
    SecondOrderSections(const std::vector<std::array<double, 3>> &bs,
                        const std::vector<std::array<double, 3>> &as)
    {
        if (bs.size() != as.size())
        {
            throw std::invalid_argument("bs.size must equal as.size()");
        }
        if (bs.empty())
        {
            throw std::invalid_argument("No filter coefficients");
        }
        auto nSections = static_cast<int> (bs.size());
        sections.reserve(nSections);
        for (int iSection = 0; iSection < nSections; ++iSection)
        {
            const Section section{bs[iSection], as[iSection]};
            sections.push_back(section);
        } 
    }
    /// Apply filter 
    [[nodiscard]] double filter(const double x) const
    {
        double v{x};
        for (auto &section : sections)
        {
            v = section(v);
        }
        return v;
    } 
    /// Apply filter to signal
    [[nodiscard]] std::vector<double> filter(const std::vector<double> &x) const
    {
        std::vector<double> y(x.size());
        for (int i = 0; i < static_cast<int> (x.size()); ++i)
        {
            y[i] = filter(x[i]);
        }
        /// Clean up initial conditions
        for (auto &section : sections)
        {
            section.resetInitialConditions();
        }
        return y;
    }
//private:
    mutable std::vector<Section> sections;
};

}

int AQMSDutyReviewBackend::SignalProcessing::getValidSamplingRate(
    const double samplingRate)
{
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Sampling rate must be positive");
    }
    if (std::abs(samplingRate - 100) < 1.e-3)
    {
        return 100;
    }
    else if (std::abs(samplingRate - 40) < 1.e-4)
    {
        return 40;
    }
    else if (std::abs(samplingRate - 80) < 1.e-4)
    {
        return 80;
    }
    else if (std::abs(samplingRate - 200) < 1.e-3)
    {
        return 200;
    }
    else if (std::abs(samplingRate - 500) < 2.e-3)
    {
        return 500;
    }
    else if (std::abs(samplingRate - 1000) < 1.e-2)
    {
        return 1000;
    }
    throw std::runtime_error("Unhandled sampling rate of "
                           + std::to_string(samplingRate)); 
}

std::vector<double>
AQMSDutyReviewBackend::SignalProcessing::bandpassFilter(
    const std::vector<double> &x,
    const int samplingRate)
{
    if (x.empty()){return x;}
    if (samplingRate == 100)
    {
        const ::SecondOrderSections sos{bs100, as100};
        return sos.filter(x);
    }
    else if (samplingRate == 40)
    {
        const ::SecondOrderSections sos{bs40, as40};
        return sos.filter(x);
    }
    else if (samplingRate == 80)
    {
        const ::SecondOrderSections sos{bs80, as80};
        return sos.filter(x);
    }
    else if (samplingRate == 200)
    {
        const ::SecondOrderSections sos{bs200, as200};
        return sos.filter(x);
    }
    else if (samplingRate == 500)
    {
        const ::SecondOrderSections sos{bs500, as500};
        return sos.filter(x);
    }
    else if (samplingRate == 1000)
    { 
        const ::SecondOrderSections sos{bs1000, as1000};
        return sos.filter(x);
    }
    throw std::runtime_error("Unhandled sampling rate of "
                           + std::to_string(samplingRate));
}


/*
int main()
{
    SecondOrderSections sos{bs100, as100};
    std::vector<double> x(100, 0.0);
    x[0] = 1;
    auto y = sos.filter(x);
}
*/
