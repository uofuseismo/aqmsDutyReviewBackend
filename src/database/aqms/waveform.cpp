#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include "aqmsDutyReviewBackend/database/aqms/waveform.hpp"
#include "aqmsDutyReviewBackend/database/aqms/segment.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

class Waveform::WaveformImpl
{
public:
    std::vector<Segment> mSegments; 
    StreamIdentifier mStreamIdentifier;
};

/// Constructor
Waveform::Waveform() :
    pImpl(std::make_unique<WaveformImpl> ())
{
}

/// Copy constructor
Waveform::Waveform(const Waveform &waveform)
{
    *this = waveform;
}

/// Move constructor
Waveform::Waveform(Waveform &&waveform) noexcept
{
    *this = std::move(waveform);
}

/// Copy assignemnt
Waveform& Waveform::operator=(const Waveform &waveform)
{
    if (&waveform == this){return *this;}
    pImpl = std::make_unique<WaveformImpl> (*waveform.pImpl);
    return *this;
}

/// Move assignment
Waveform& Waveform::operator=(Waveform &&waveform) noexcept
{
    if (&waveform == this){return *this;}
    pImpl = std::move(waveform.pImpl);
    return *this;
}

/// Destructor
Waveform::~Waveform() = default;

/// Iterators
Waveform::const_iterator Waveform::begin() const
{
    return pImpl->mSegments.begin();
}

Waveform::const_iterator Waveform::cbegin() const
{
    return pImpl->mSegments.cbegin();
}

Waveform::const_iterator Waveform::end() const
{
    return pImpl->mSegments.end();
}

Waveform::const_iterator Waveform::cend() const
{
    return pImpl->mSegments.cend();
}

const Segment& Waveform::at(size_t pos) const
{
    return pImpl->mSegments.at(pos);
}

const Segment& Waveform::operator[](size_t pos) const
{
    return pImpl->mSegments[pos];
}

